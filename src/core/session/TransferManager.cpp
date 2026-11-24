#include "core/session/TransferManager.h"
#include "core/session/GroupFileFetcher.h"
#include "platform/AndroidHelper.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace landrop {

TransferManager::TransferManager(QString localDeviceId,
                                 QString localDeviceName,
                                 QString localPlatform,
                                 QObject *parent)
    : QObject(parent)
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_downloadDir(AndroidHelper::downloadDirectory())
{
    connect(&m_server, &QTcpServer::newConnection, this, &TransferManager::onNewTcpConnection);
}

TransferManager::~TransferManager() { stopServer(); }

bool TransferManager::startServer(quint16 port)
{
    return m_server.isListening() || m_server.listen(QHostAddress::AnyIPv4, port);
}

void TransferManager::stopServer()
{
    m_server.close();
    while (!m_pendingQueue.isEmpty()) {
        const auto item = m_pendingQueue.takeFirst();
        if (item.socket) { item.socket->disconnect(); item.socket->abort(); item.socket->deleteLater(); }
        if (item.codec) item.codec->deleteLater();
    }
    for (PeerSession *session : m_sessions) {
        session->disconnectFromPeer();
    }
}

void TransferManager::setDownloadDirectory(const QString &dir)
{
    m_downloadDir = dir;
    for (PeerSession *session : m_sessions) session->setDownloadDir(dir);
}

void TransferManager::setLocalDeviceName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_localDeviceName == trimmed) return;
    m_localDeviceName = trimmed;
    for (PeerSession *session : m_sessions) session->sendLocalInfoUpdate(m_localDeviceName);
}

void TransferManager::registerSharedFile(const QString &transferId, const QString &filePath, const QString &fileName, bool deleteOnFinish)
{
    m_sharedFiles.insert(transferId, SharedFileEntry{filePath, fileName, deleteOnFinish});
}

void TransferManager::unregisterSharedFile(const QString &transferId)
{
    if (auto it = m_sharedFiles.find(transferId); it != m_sharedFiles.end()) {
        if (it->deleteOnFinish) QFile::remove(it->filePath);
        m_sharedFiles.erase(it);
    }
}

QString TransferManager::activePeerName() const { auto *s = sessionForPeer(m_activePeerId); return s ? s->peerName() : QString{}; }
QString TransferManager::activePeerIp() const { auto *s = sessionForPeer(m_activePeerId); return s ? s->peerIp() : QString{}; }
int TransferManager::activePeerState() const { auto *s = sessionForPeer(m_activePeerId); return s ? static_cast<int>(s->state()) : 0; }
QObject *TransferManager::activeMessageModel() const { auto *s = sessionForPeer(m_activePeerId); return s ? s->messages() : nullptr; }

QString TransferManager::incomingPeerId() const { return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().peerId; }
QString TransferManager::incomingPeerName() const { return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().peerName; }
QString TransferManager::incomingPeerPlatform() const { return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().platform; }
PeerSession *TransferManager::sessionForPeer(const QString &peerId) const { return m_sessions.value(peerId, nullptr); }

PeerSession *TransferManager::ensureSession(const QString &peerId, const QString &peerName, const QString &platform)
{
    PeerSession *session = m_sessions.value(peerId, nullptr);
    if (!session) {
        session = new PeerSession(peerId, peerName, platform, m_localDeviceId, m_localDeviceName, m_localPlatform, m_downloadDir, this);
        m_sessions.insert(peerId, session);

        connect(session, &PeerSession::stateChanged, this, [this](const QString &pid, SessionState newState) {
            PeerSession *s = sessionForPeer(pid);
            emit peerStatusUpdated(pid, static_cast<int>(newState), s ? s->unreadCount() : 0);
            if (pid == m_activePeerId) { emit activePeerChanged(); emit activePeerStateChanged(); }
            if (newState == SessionState::Connected) emit sessionConnected(pid);
        });

        connect(session, &PeerSession::unreadCountChanged, this, [this](const QString &pid, int unread) {
            if (pid == m_activePeerId && unread > 0) {
                if (PeerSession *s = sessionForPeer(pid)) s->clearUnread();
                return;
            }
            PeerSession *s = sessionForPeer(pid);
            emit peerStatusUpdated(pid, s ? static_cast<int>(s->state()) : 0, unread);
        });

        connect(session, &PeerSession::handshakeRejected, this, &TransferManager::handshakeRejected);
        connect(session, &PeerSession::peerInfoUpdated, this, [this](const QString &pid, const QString &name, const QString &) {
            emit peerInfoUpdated(pid, name);
            if (pid == m_activePeerId) emit activePeerChanged();
        });
        connect(session, &PeerSession::connectionFailed, this, [this](const QString &, const QString &reason) {
            emit connectionFailed(reason);
        });
        connect(session, &PeerSession::fileTransferCompleted, this, &TransferManager::fileTransferCompleted);
    } else {
        session->setPeerInfo(peerName, platform);
    }
    return session;
}

void TransferManager::connectToDevice(const QString &deviceId, const QString &deviceName,
                                      const QString &platform, const QString &ip, quint16 port)
{
    if (deviceId.isEmpty()) return;
    PeerSession *session = ensureSession(deviceId, deviceName, platform);
    selectPeer(deviceId);
    if (session->state() == SessionState::Disconnected) session->connectOutgoing(ip, port);
}

void TransferManager::connectByAddress(const QString &addressInput)
{
    QString cleaned = addressInput.trimmed();
    cleaned.replace(QChar(0xFF1A), QLatin1Char(':'));
    if (cleaned.isEmpty()) return;

    QString host = cleaned;
    quint16 port = kDefaultTcpPort;
    if (const int colonIdx = cleaned.lastIndexOf(QLatin1Char(':')); colonIdx > 0) {
        bool ok = false;
        const int parsed = cleaned.mid(colonIdx + 1).toInt(&ok);
        if (ok && parsed > 0 && parsed <= 65535) {
            host = cleaned.left(colonIdx).trimmed();
            port = static_cast<quint16>(parsed);
        }
    }
    if (host.isEmpty()) { emit connectionFailed(QStringLiteral("Invalid IP address")); return; }
    connectToDevice(QStringLiteral("direct:%1:%2").arg(host).arg(port), host, QStringLiteral("direct"), host, port);
}

void TransferManager::selectPeer(const QString &peerId)
{
    if (m_activePeerId == peerId) {
        if (PeerSession *s = sessionForPeer(peerId)) s->clearUnread();
        return;
    }
    m_activePeerId = peerId;
    if (PeerSession *s = sessionForPeer(peerId)) s->clearUnread();
    emit activePeerChanged();
    emit activePeerStateChanged();
}

void TransferManager::clearActivePeer()
{
    if (!m_activePeerId.isEmpty()) {
        m_activePeerId.clear();
        emit activePeerChanged();
        emit activePeerStateChanged();
    }
}

void TransferManager::onNewTcpConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        auto *codec = new PacketCodec(socket);

        connect(socket, &QTcpSocket::readyRead, codec, [socket, codec]() {
            const QByteArray data = socket->readAll();
            if (!data.isEmpty()) codec->feed(data);
        });
        connect(codec, &PacketCodec::protocolError, socket, [socket]() {
            socket->abort();
            socket->deleteLater();
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            for (int i = 0; i < m_pendingQueue.size(); ++i) {
                if (m_pendingQueue.at(i).socket == socket) {
                    const QString pid = m_pendingQueue.at(i).peerId;
                    m_pendingQueue.removeAt(i);
                    emit incomingRequestChanged();
                    PeerSession *s = sessionForPeer(pid);
                    emit peerStatusUpdated(pid, s ? static_cast<int>(s->state()) : 0, s ? s->unreadCount() : 0);
                    break;
                }
            }
            socket->deleteLater();
        });

        connect(codec, &PacketCodec::packetReceived, this, [this, socket, codec](MsgType type, const QByteArray &payload) {
            if (type == MsgType::GroupJoinReq) {
                socket->disconnect(codec); socket->disconnect(this); codec->disconnect(this);
                emit incomingGroupJoin(socket, codec, payload);
                return;
            }
            if (type == MsgType::GroupFileFetchReq) {
                socket->disconnect(codec); socket->disconnect(this); codec->disconnect(this); codec->deleteLater();
                handleGroupFileFetch(socket, payload);
                return;
            }
            if (type != MsgType::HandshakeReq) { socket->abort(); socket->deleteLater(); return; }

            socket->disconnect(codec);
            codec->disconnect(this);

            const QJsonObject obj = QJsonDocument::fromJson(payload).object();
            const QString peerId = obj.value(QStringLiteral("deviceId")).toString();
            const QString peerName = obj.value(QStringLiteral("deviceName")).toString(QStringLiteral("Unknown"));
            const QString platform = obj.value(QStringLiteral("platform")).toString(QStringLiteral("desktop"));
            if (peerId.isEmpty() || peerId == m_localDeviceId) { socket->abort(); socket->deleteLater(); return; }

            if (PeerSession *existing = sessionForPeer(peerId); existing && existing->shouldMaintainConnection()) {
                socket->disconnect(this);
                codec->deleteLater();
                QJsonObject ack{{QStringLiteral("accepted"), true}, {QStringLiteral("deviceId"), m_localDeviceId},
                                {QStringLiteral("deviceName"), m_localDeviceName}, {QStringLiteral("platform"), m_localPlatform}};
                socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));
                existing->setPeerInfo(peerName, platform);
                existing->attachAcceptedSocket(socket);
                return;
            }

            removePendingIncoming(peerId, true);
            m_pendingQueue.append(PendingIncoming{peerId, peerName, platform, socket, codec});
            emit peerStatusUpdated(peerId, static_cast<int>(SessionState::PendingIncoming), 0);
            emit incomingRequestChanged();
        });

        if (socket->bytesAvailable() > 0) codec->feed(socket->readAll());
    }
}

void TransferManager::acceptIncomingRequest(const QString &peerId)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId != peerId) continue;
        const PendingIncoming item = m_pendingQueue.takeAt(i);
        emit incomingRequestChanged();

        if (!item.socket || item.socket->state() != QAbstractSocket::ConnectedState) {
            if (item.socket) item.socket->deleteLater();
            return;
        }
        item.socket->disconnect(this);
        if (item.codec) item.codec->deleteLater();

        QJsonObject ack{{QStringLiteral("accepted"), true}, {QStringLiteral("deviceId"), m_localDeviceId},
                        {QStringLiteral("deviceName"), m_localDeviceName}, {QStringLiteral("platform"), m_localPlatform}};
        item.socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));

        PeerSession *session = ensureSession(item.peerId, item.peerName, item.platform);
        session->attachAcceptedSocket(item.socket);
        selectPeer(item.peerId);
        return;
    }
}

void TransferManager::rejectIncomingRequest(const QString &peerId)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId != peerId) continue;
        const PendingIncoming item = m_pendingQueue.takeAt(i);
        emit incomingRequestChanged();

        if (item.socket) {
            item.socket->disconnect(this);
            if (item.socket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject ack{{QStringLiteral("accepted"), false}, {QStringLiteral("reason"), QStringLiteral("Declined by user")}};
                item.socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));
                item.socket->flush();
                connect(item.socket, &QTcpSocket::disconnected, item.socket, &QObject::deleteLater);
                item.socket->disconnectFromHost();
            } else {
                item.socket->deleteLater();
            }
        }
        PeerSession *s = sessionForPeer(peerId);
        emit peerStatusUpdated(peerId, s ? static_cast<int>(s->state()) : 0, s ? s->unreadCount() : 0);
        return;
    }
}

void TransferManager::removePendingIncoming(const QString &peerId, bool closeSocket)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId == peerId) {
            const PendingIncoming item = m_pendingQueue.takeAt(i);
            if (closeSocket && item.socket) {
                item.socket->disconnect(this);
                item.socket->abort();
                item.socket->deleteLater();
            }
            emit incomingRequestChanged();
            return;
        }
    }
}

void TransferManager::disconnectPeer(const QString &peerId) { if (auto *s = sessionForPeer(peerId)) s->disconnectFromPeer(); }
void TransferManager::reconnectActiveSessions() { for (PeerSession *s : m_sessions) s->reconnectIfDropped(); }
void TransferManager::sendText(const QString &text) { if (auto *s = sessionForPeer(m_activePeerId)) s->sendText(text); }
void TransferManager::sendFiles(const QList<QUrl> &urls) { if (auto *s = sessionForPeer(m_activePeerId)) for (const QUrl &u : urls) s->enqueueFile(u); }
void TransferManager::cancelTransfer(const QString &transferId) { if (auto *s = sessionForPeer(m_activePeerId)) s->cancelTransfer(transferId); }

void TransferManager::handleGroupFileFetch(QTcpSocket *socket, const QByteArray &payload)
{
    const QString transferId = QJsonDocument::fromJson(payload).object().value(QStringLiteral("transferId")).toString();
    const auto it = m_sharedFiles.constFind(transferId);
    if (it == m_sharedFiles.cend()) {
        socket->abort();
        socket->deleteLater();
        return;
    }
    GroupFileFetcher::serveFile(socket, transferId, it->filePath, it->fileName);
}

} // namespace landrop
