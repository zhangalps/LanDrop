#include "core/session/PeerSession.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace landrop {

namespace {

QString nowTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

} // namespace

PeerSession::PeerSession(QString peerId, QString peerName, QString peerPlatform,
                         QString localDeviceId, QString localDeviceName,
                         QString localPlatform, QString downloadDir, QObject *parent)
    : QObject(parent)
    , m_peerId(std::move(peerId))
    , m_peerName(std::move(peerName))
    , m_peerPlatform(std::move(peerPlatform))
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_downloadDir(std::move(downloadDir))
{
    connect(&m_codec, &PacketCodec::packetReceived, this, &PeerSession::onPacketReceived);
    connect(&m_codec, &PacketCodec::protocolError, this, &PeerSession::onProtocolError);
}

PeerSession::~PeerSession()
{
    abortAllActiveTransfers(QStringLiteral("canceled"));
    detachSocket();
}

void PeerSession::setPeerInfo(const QString &name, const QString &platform)
{
    if (!name.isEmpty()) m_peerName = name;
    if (!platform.isEmpty()) m_peerPlatform = platform;
}

void PeerSession::clearUnread()
{
    if (m_unreadCount != 0) {
        m_unreadCount = 0;
        emit unreadCountChanged(m_peerId, m_unreadCount);
    }
}

void PeerSession::connectOutgoing(const QString &ip, quint16 port)
{
    m_peerIp = ip;
    m_lastIp = ip;
    m_lastPort = port;
    m_shouldMaintainConnection = true;
    if (m_state == SessionState::Connected || m_state == SessionState::PendingOutgoing) {
        return;
    }

    detachSocket();
    auto *socket = new QTcpSocket(this);
    attachSocket(socket);
    setState(SessionState::PendingOutgoing);
    socket->connectToHost(ip, port);
}

void PeerSession::attachAcceptedSocket(QTcpSocket *socket)
{
    detachSocket();
    socket->setParent(this);
    const QHostAddress addr = socket->peerAddress();
    bool ok = false;
    const quint32 ipv4 = addr.toIPv4Address(&ok);
    m_peerIp = ok ? QHostAddress(ipv4).toString() : addr.toString();
    m_lastIp = m_peerIp;
    m_shouldMaintainConnection = true;
    attachSocket(socket);
    setState(SessionState::Connected);
    if (m_socket->bytesAvailable() > 0) onSocketReadyRead();
}

void PeerSession::disconnectFromPeer()
{
    m_shouldMaintainConnection = false;
    abortAllActiveTransfers(QStringLiteral("canceled"));
    detachSocket();
    setState(SessionState::Disconnected);
}

void PeerSession::reconnectIfDropped()
{
    if (m_shouldMaintainConnection && m_state == SessionState::Disconnected
        && !m_lastIp.isEmpty() && m_lastPort > 0) {
        connectOutgoing(m_lastIp, m_lastPort);
    }
}

void PeerSession::attachSocket(QTcpSocket *socket)
{
    m_socket = socket;
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    m_codec.reset();

    connect(m_socket, &QTcpSocket::connected, this, &PeerSession::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &PeerSession::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &PeerSession::onSocketBytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, &PeerSession::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PeerSession::onSocketError);
}

void PeerSession::detachSocket()
{
    if (!m_socket) return;
    m_socket->disconnect(this);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
    m_codec.reset();
}

void PeerSession::setState(SessionState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_peerId, m_state);
    }
}

void PeerSession::onSocketConnected()
{
    if (!m_socket) return;
    QJsonObject req{
        {QStringLiteral("deviceId"), m_localDeviceId},
        {QStringLiteral("deviceName"), m_localDeviceName},
        {QStringLiteral("platform"), m_localPlatform}
    };
    m_socket->write(PacketCodec::encodeJson(MsgType::HandshakeReq, req));
}

void PeerSession::onSocketReadyRead()
{
    if (!m_socket) return;
    const QByteArray chunk = m_socket->readAll();
    if (!chunk.isEmpty()) m_codec.feed(chunk);
}

void PeerSession::onSocketBytesWritten(qint64)
{
    if (m_hasActiveOutgoing) pumpOutgoingFile();
}

void PeerSession::onSocketDisconnected()
{
    abortAllActiveTransfers(QStringLiteral("failed"));
    detachSocket();
    setState(SessionState::Disconnected);
}

void PeerSession::onSocketError(QAbstractSocket::SocketError)
{
    if (m_state == SessionState::PendingOutgoing) {
        const QString errStr = m_socket ? m_socket->errorString() : QStringLiteral("Connection failed");
        abortAllActiveTransfers(QStringLiteral("failed"));
        detachSocket();
        setState(SessionState::Disconnected);
        emit connectionFailed(m_peerId, errStr);
    }
}

void PeerSession::onProtocolError(const QString &) { disconnectFromPeer(); }

void PeerSession::sendLocalInfoUpdate(const QString &newName)
{
    m_localDeviceName = newName;
    if (m_state == SessionState::Connected && m_socket) {
        QJsonObject obj{
            {QStringLiteral("deviceId"), m_localDeviceId},
            {QStringLiteral("deviceName"), newName},
            {QStringLiteral("platform"), m_localPlatform}
        };
        m_socket->write(PacketCodec::encodeJson(MsgType::DeviceInfoUpdate, obj));
    }
}

void PeerSession::sendText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || m_state != SessionState::Connected || !m_socket) return;

    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject obj{
        {QStringLiteral("msgId"), msgId},
        {QStringLiteral("text"), trimmed},
        {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
    };
    m_socket->write(PacketCodec::encodeJson(MsgType::TextMessage, obj));

    ChatMessage msg;
    msg.msgId = msgId;
    msg.isOutgoing = true;
    msg.kind = QStringLiteral("text");
    msg.text = trimmed;
    msg.status = QStringLiteral("completed");
    msg.progress = 1.0;
    msg.timestampText = nowTimeText();
    m_messages.appendMessage(std::move(msg));
}

void PeerSession::onPacketReceived(MsgType type, const QByteArray &payload)
{
    switch (type) {
    case MsgType::HandshakeAck: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        if (obj.value(QStringLiteral("accepted")).toBool(false)) {
            setPeerInfo(obj.value(QStringLiteral("deviceName")).toString(),
                        obj.value(QStringLiteral("platform")).toString());
            setState(SessionState::Connected);
        } else {
            m_shouldMaintainConnection = false;
            detachSocket();
            setState(SessionState::Disconnected);
            emit handshakeRejected(m_peerId, obj.value(QStringLiteral("reason")).toString(QStringLiteral("Rejected")));
        }
        break;
    }
    case MsgType::TextMessage: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        msg.kind = QStringLiteral("text");
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = nowTimeText();
        m_messages.appendMessage(std::move(msg));
        emit unreadCountChanged(m_peerId, ++m_unreadCount);
        emit messageReceived(m_peerId);
        break;
    }
    case MsgType::FileMeta:     handleIncomingFileMeta(QJsonDocument::fromJson(payload).object()); break;
    case MsgType::FileChunk:    handleIncomingFileChunk(payload); break;
    case MsgType::FileComplete: handleIncomingFileComplete(QJsonDocument::fromJson(payload).object()); break;
    case MsgType::FileCancel:   handleIncomingFileCancel(QJsonDocument::fromJson(payload).object()); break;
    case MsgType::DeviceInfoUpdate: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString newName = obj.value(QStringLiteral("deviceName")).toString().trimmed();
        if (!newName.isEmpty()) {
            setPeerInfo(newName, obj.value(QStringLiteral("platform")).toString());
            emit peerInfoUpdated(m_peerId, m_peerName, m_peerPlatform);
        }
        break;
    }
    default: break;
    }
}

} // namespace landrop
