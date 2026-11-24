#include "core/session/GroupRoom.h"
#include "core/session/GroupFileFetcher.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace landrop {

namespace {

QString roomTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

QString cleanRoomFileName(const QString &name)
{
    QString cleaned = QFileInfo(name).fileName().trimmed();
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return cleaned.isEmpty() ? QStringLiteral("file") : cleaned;
}

QString socketIpv4String(QTcpSocket *sock)
{
    if (!sock) return {};
    const QHostAddress addr = sock->peerAddress();
    bool ok = false;
    const quint32 ipv4 = addr.toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : addr.toString();
}

} // namespace

GroupRoom::GroupRoom(QString groupId, QString groupName, QString hostId, QString hostName,
                     QString pin, bool isHost, QString localDeviceId, QString localDeviceName,
                     QString localPlatform, TransferManager *transferManager, QObject *parent)
    : QObject(parent)
    , m_groupId(std::move(groupId))
    , m_groupName(std::move(groupName))
    , m_hostId(std::move(hostId))
    , m_hostName(std::move(hostName))
    , m_pin(std::move(pin))
    , m_isHost(isHost)
    , m_isJoined(isHost)
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_transferManager(transferManager)
{
    if (m_isHost) {
        m_memberModel.upsertMember(GroupMemberInfo{
            m_localDeviceId, m_localDeviceName, m_localPlatform, AndroidHelper::localIpAddress(), true
        });
        appendSystemMessage(QStringLiteral("已创建群聊「%1」").arg(m_groupName));
    }
    connect(&m_hostCodec, &PacketCodec::packetReceived, this, &GroupRoom::handleMemberPacket);
    connect(&m_hostCodec, &PacketCodec::protocolError, this, [this](const QString &) { leave(); });
}

GroupRoom::~GroupRoom()
{
    for (auto it = m_memberConns.begin(); it != m_memberConns.end(); ++it) {
        if (it->socket) { it->socket->disconnect(); it->socket->abort(); it->socket->deleteLater(); }
    }
    m_memberConns.clear();
    if (m_hostSocket) {
        m_hostSocket->disconnect(this);
        m_hostSocket->abort();
        m_hostSocket->deleteLater();
        m_hostSocket = nullptr;
    }
}

void GroupRoom::clearUnread()
{
    if (m_unreadCount != 0) {
        m_unreadCount = 0;
        emit unreadCountChanged(m_groupId, m_unreadCount);
    }
}

void GroupRoom::updateLocalDeviceName(const QString &newName)
{
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || m_localDeviceName == trimmed) return;
    m_localDeviceName = trimmed;
    m_messages.updateSenderName(m_localDeviceId, m_localDeviceName);

    if (m_isHost) {
        m_hostName = m_localDeviceName;
        m_memberModel.upsertMember(GroupMemberInfo{
            m_localDeviceId, m_localDeviceName, m_localPlatform, AndroidHelper::localIpAddress(), true
        });
        QJsonObject sync{
            {QStringLiteral("groupId"), m_groupId},
            {QStringLiteral("event"), QStringLiteral("renamed")},
            {QStringLiteral("actorId"), m_localDeviceId},
            {QStringLiteral("actorName"), m_localDeviceName},
            {QStringLiteral("members"), m_memberModel.toJsonArray()}
        };
        broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));
    } else if (m_isJoined && m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject obj{
            {QStringLiteral("groupId"), m_groupId},
            {QStringLiteral("deviceId"), m_localDeviceId},
            {QStringLiteral("deviceName"), m_localDeviceName},
            {QStringLiteral("platform"), m_localPlatform}
        };
        m_hostSocket->write(PacketCodec::encodeJson(MsgType::DeviceInfoUpdate, obj));
    }
}

void GroupRoom::addMemberConnection(const GroupMemberInfo &member, QTcpSocket *socket, PacketCodec *codec)
{
    if (!m_isHost || !socket || !codec) return;

    if (auto oldIt = m_memberConns.find(member.deviceId); oldIt != m_memberConns.end()) {
        if (oldIt->socket) { oldIt->socket->disconnect(this); oldIt->socket->abort(); oldIt->socket->deleteLater(); }
        m_memberConns.erase(oldIt);
    }

    socket->setParent(this);
    codec->setParent(socket);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    m_knownMemberIds.insert(member.deviceId);

    m_memberConns.insert(member.deviceId, MemberConn{member.deviceId, socket, codec});
    m_memberModel.upsertMember(member);

    QJsonObject ack{
        {QStringLiteral("accepted"), true},
        {QStringLiteral("groupId"), m_groupId},
        {QStringLiteral("groupName"), m_groupName},
        {QStringLiteral("hostId"), m_hostId},
        {QStringLiteral("hostName"), m_hostName},
        {QStringLiteral("members"), m_memberModel.toJsonArray()}
    };
    socket->write(PacketCodec::encodeJson(MsgType::GroupJoinAck, ack));

    QJsonObject sync{
        {QStringLiteral("groupId"), m_groupId},
        {QStringLiteral("event"), QStringLiteral("joined")},
        {QStringLiteral("actorId"), member.deviceId},
        {QStringLiteral("actorName"), member.deviceName},
        {QStringLiteral("members"), m_memberModel.toJsonArray()}
    };
    broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync), member.deviceId);

    appendSystemMessage(QStringLiteral("%1 加入了群聊").arg(member.deviceName));
    emit memberCountChanged(m_groupId, m_memberModel.rowCount());

    const QString peerId = member.deviceId;
    connect(socket, &QTcpSocket::readyRead, codec, [socket, codec]() {
        const QByteArray chunk = socket->readAll();
        if (!chunk.isEmpty()) codec->feed(chunk);
    });
    connect(codec, &PacketCodec::packetReceived, this, [this, peerId](MsgType type, const QByteArray &payload) {
        handleHostPacket(peerId, type, payload);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, peerId, socket]() {
        auto it = m_memberConns.find(peerId);
        if (it != m_memberConns.end() && it->socket == socket) {
            m_memberConns.erase(it);
            QString leftName = peerId;
            for (const auto &m : m_memberModel.members()) {
                if (m.deviceId == peerId) { leftName = m.deviceName; break; }
            }
            m_memberModel.removeMember(peerId);
            QJsonObject sync{
                {QStringLiteral("groupId"), m_groupId},
                {QStringLiteral("event"), QStringLiteral("left")},
                {QStringLiteral("actorId"), peerId},
                {QStringLiteral("actorName"), leftName},
                {QStringLiteral("members"), m_memberModel.toJsonArray()}
            };
            broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));
            appendSystemMessage(QStringLiteral("%1 离开了群聊").arg(leftName));
            emit memberCountChanged(m_groupId, m_memberModel.rowCount());
        }
        socket->deleteLater();
    });

    if (socket->bytesAvailable() > 0) codec->feed(socket->readAll());
}

void GroupRoom::disband()
{
    if (!m_isHost) return;
    QJsonObject obj{{QStringLiteral("groupId"), m_groupId}, {QStringLiteral("reason"), QStringLiteral("群主已解散群聊")}};
    broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupDisband, obj));
    for (auto it = m_memberConns.begin(); it != m_memberConns.end(); ++it) {
        if (it->socket) { it->socket->flush(); it->socket->disconnectFromHost(); }
    }
    m_memberConns.clear();
    m_isJoined = false;
    emit disbanded(m_groupId, QStringLiteral("群聊已解散"));
}

void GroupRoom::connectToHost(const QString &hostIp, quint16 hostPort, const QString &pinAttempt)
{
    if (m_isHost) return;
    if (m_hostSocket) {
        m_hostSocket->disconnect(this);
        m_hostSocket->abort();
        m_hostSocket->deleteLater();
        m_hostSocket = nullptr;
    }

    m_hostIp = hostIp;
    m_hostPort = hostPort;
    m_pinAttempt = pinAttempt;
    m_shouldMaintainConnection = true;
    m_hostCodec.reset();
    m_hostSocket = new QTcpSocket(this);
    m_hostSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_hostSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    connect(m_hostSocket, &QTcpSocket::connected, this, [this]() {
        QJsonObject req{
            {QStringLiteral("groupId"), m_groupId},
            {QStringLiteral("deviceId"), m_localDeviceId},
            {QStringLiteral("deviceName"), m_localDeviceName},
            {QStringLiteral("platform"), m_localPlatform},
            {QStringLiteral("pin"), m_pinAttempt}
        };
        m_hostSocket->write(PacketCodec::encodeJson(MsgType::GroupJoinReq, req));
    });
    connect(m_hostSocket, &QTcpSocket::readyRead, this, [this]() {
        if (m_hostSocket) {
            const QByteArray chunk = m_hostSocket->readAll();
            if (!chunk.isEmpty()) m_hostCodec.feed(chunk);
        }
    });
    connect(m_hostSocket, &QTcpSocket::disconnected, this, [this]() {
        const bool wasJoined = m_isJoined;
        m_isJoined = false;
        if (m_hostSocket) { m_hostSocket->deleteLater(); m_hostSocket = nullptr; }
        if (wasJoined) {
            appendSystemMessage(QStringLiteral("与群主断开连接"));
            emit joinedChanged(m_groupId, false);
        }
    });
    connect(m_hostSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_isJoined && m_hostSocket) emit joinRejected(m_groupId, m_hostSocket->errorString());
    });

    m_hostSocket->connectToHost(hostIp, hostPort);
}

void GroupRoom::reconnectIfDropped()
{
    if (!m_isHost && m_shouldMaintainConnection && !m_isJoined
        && !m_hostIp.isEmpty() && m_hostPort > 0 && !m_hostSocket) {
        connectToHost(m_hostIp, m_hostPort, m_pinAttempt);
    }
}

void GroupRoom::leave()
{
    m_shouldMaintainConnection = false;
    if (m_isHost) { disband(); return; }
    if (m_hostSocket) {
        if (m_hostSocket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject obj{{QStringLiteral("groupId"), m_groupId}, {QStringLiteral("deviceId"), m_localDeviceId}};
            m_hostSocket->write(PacketCodec::encodeJson(MsgType::GroupLeave, obj));
            m_hostSocket->flush();
        }
        m_hostSocket->disconnect(this);
        m_hostSocket->disconnectFromHost();
        m_hostSocket->deleteLater();
        m_hostSocket = nullptr;
    }
    m_isJoined = false;
    emit joinedChanged(m_groupId, false);
}

void GroupRoom::sendText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || !m_isJoined) return;

    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject obj{
        {QStringLiteral("groupId"), m_groupId},
        {QStringLiteral("msgId"), msgId},
        {QStringLiteral("senderId"), m_localDeviceId},
        {QStringLiteral("senderName"), m_localDeviceName},
        {QStringLiteral("text"), trimmed},
        {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
    };
    const QByteArray frame = PacketCodec::encodeJson(MsgType::GroupTextMessage, obj);

    if (m_isHost) broadcastToMembers(frame);
    else if (m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) m_hostSocket->write(frame);
    else return;

    ChatMessage msg;
    msg.msgId = msgId;
    msg.isOutgoing = true;
    msg.kind = QStringLiteral("text");
    msg.text = trimmed;
    msg.senderId = m_localDeviceId;
    msg.senderName = m_localDeviceName;
    msg.status = QStringLiteral("completed");
    msg.progress = 1.0;
    msg.timestampText = roomTimeText();
    m_messages.appendMessage(std::move(msg));
}

void GroupRoom::sendFiles(const QList<QUrl> &urls)
{
    if (!m_isJoined || !m_transferManager) return;
    const QString myIp = AndroidHelper::localIpAddress();
    const quint16 myPort = m_transferManager->serverPort();

    for (const QUrl &url : urls) {
        const PreparedFile prepared = AndroidHelper::prepareReadableFile(url);
        if (prepared.readablePath.isEmpty() || prepared.fileSize < 0) continue;

        const QString fileName = cleanRoomFileName(prepared.displayName);
        const QString mediaKind = classifyMediaKind(fileName);
        const QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_transferManager->registerSharedFile(transferId, prepared.readablePath, fileName, false);

        QJsonObject ann{
            {QStringLiteral("groupId"), m_groupId},
            {QStringLiteral("transferId"), transferId},
            {QStringLiteral("senderId"), m_localDeviceId},
            {QStringLiteral("senderName"), m_localDeviceName},
            {QStringLiteral("senderIp"), myIp},
            {QStringLiteral("senderPort"), static_cast<int>(myPort)},
            {QStringLiteral("fileName"), fileName},
            {QStringLiteral("fileSize"), prepared.fileSize},
            {QStringLiteral("mediaType"), mediaKind},
            {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
        };
        const QByteArray frame = PacketCodec::encodeJson(MsgType::GroupFileAnnounce, ann);
        if (m_isHost) broadcastToMembers(frame);
        else if (m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) m_hostSocket->write(frame);

        ChatMessage msg;
        msg.msgId = transferId;
        msg.isOutgoing = true;
        msg.kind = mediaKind;
        msg.senderId = m_localDeviceId;
        msg.senderName = m_localDeviceName;
        msg.senderIp = myIp;
        msg.senderPort = myPort;
        msg.fileName = fileName;
        msg.filePath = prepared.readablePath;
        msg.fileUrl = QUrl::fromLocalFile(prepared.readablePath).toString();
        msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
        if (msg.fileExtension.isEmpty()) msg.fileExtension = QStringLiteral("FILE");
        msg.fileSize = prepared.fileSize;
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = roomTimeText();
        m_messages.appendMessage(std::move(msg));
    }
}

void GroupRoom::downloadFile(const QString &transferId)
{
    const QString dir = m_transferManager ? m_transferManager->downloadDirectory() : AndroidHelper::downloadDirectory();
    GroupFileFetcher::fetchFile(this, &m_messages, m_localDeviceId, dir, transferId);
}

void GroupRoom::appendSystemMessage(const QString &text)
{
    ChatMessage msg;
    msg.msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.kind = QStringLiteral("system");
    msg.text = text;
    msg.status = QStringLiteral("completed");
    msg.timestampText = roomTimeText();
    m_messages.appendMessage(std::move(msg));
}

void GroupRoom::broadcastToMembers(const QByteArray &frame, const QString &excludeId)
{
    for (auto it = m_memberConns.constBegin(); it != m_memberConns.constEnd(); ++it) {
        if (!excludeId.isEmpty() && it.key() == excludeId) continue;
        if (it->socket && it->socket->state() == QAbstractSocket::ConnectedState) it->socket->write(frame);
    }
}

} // namespace landrop
