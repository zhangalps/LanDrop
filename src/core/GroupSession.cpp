#include "core/GroupSession.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <cstring>

namespace landrop {

namespace {

QString groupTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

QString cleanGroupFileName(const QString &name)
{
    QString cleaned = QFileInfo(name).fileName().trimmed();
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return cleaned.isEmpty() ? QStringLiteral("file") : cleaned;
}

} // namespace

// ============================================================================
// GroupMemberListModel
// ============================================================================

GroupMemberListModel::GroupMemberListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GroupMemberListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_members.size());
}

QVariant GroupMemberListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_members.size()) {
        return {};
    }
    const GroupMemberInfo &m = m_members.at(index.row());
    switch (role) {
    case DeviceIdRole:   return m.deviceId;
    case DeviceNameRole: return m.deviceName;
    case PlatformRole:   return m.platform;
    case IpAddressRole:  return m.ipAddress;
    case IsHostRole:     return m.isHost;
    default:             return {};
    }
}

QHash<int, QByteArray> GroupMemberListModel::roleNames() const
{
    return {
        {DeviceIdRole,   "deviceId"},
        {DeviceNameRole, "deviceName"},
        {PlatformRole,   "platform"},
        {IpAddressRole,  "ipAddress"},
        {IsHostRole,     "isHost"}
    };
}

void GroupMemberListModel::setMembers(QVector<GroupMemberInfo> members)
{
    beginResetModel();
    m_members = std::move(members);
    endResetModel();
    emit countChanged();
}

void GroupMemberListModel::upsertMember(const GroupMemberInfo &member)
{
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members.at(i).deviceId == member.deviceId) {
            m_members[i] = member;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
            return;
        }
    }
    const int row = static_cast<int>(m_members.size());
    beginInsertRows(QModelIndex(), row, row);
    m_members.append(member);
    endInsertRows();
    emit countChanged();
}

void GroupMemberListModel::removeMember(const QString &deviceId)
{
    for (int i = 0; i < m_members.size(); ++i) {
        if (m_members.at(i).deviceId == deviceId) {
            beginRemoveRows(QModelIndex(), i, i);
            m_members.removeAt(i);
            endRemoveRows();
            emit countChanged();
            return;
        }
    }
}

QJsonArray GroupMemberListModel::toJsonArray() const
{
    QJsonArray arr;
    for (const GroupMemberInfo &m : m_members) {
        QJsonObject obj;
        obj.insert(QStringLiteral("deviceId"), m.deviceId);
        obj.insert(QStringLiteral("deviceName"), m.deviceName);
        obj.insert(QStringLiteral("platform"), m.platform);
        obj.insert(QStringLiteral("ipAddress"), m.ipAddress);
        obj.insert(QStringLiteral("isHost"), m.isHost);
        arr.append(obj);
    }
    return arr;
}

// ============================================================================
// GroupRoom
// ============================================================================

GroupRoom::GroupRoom(QString groupId,
                     QString groupName,
                     QString hostId,
                     QString hostName,
                     QString pin,
                     bool isHost,
                     QString localDeviceId,
                     QString localDeviceName,
                     QString localPlatform,
                     TransferManager *transferManager,
                     QObject *parent)
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
        GroupMemberInfo self;
        self.deviceId = m_localDeviceId;
        self.deviceName = m_localDeviceName;
        self.platform = m_localPlatform;
        self.ipAddress = AndroidHelper::localIpAddress();
        self.isHost = true;
        m_memberModel.upsertMember(self);
        appendSystemMessage(QStringLiteral("已创建群聊「%1」").arg(m_groupName));
    }

    connect(&m_hostCodec, &PacketCodec::packetReceived, this, &GroupRoom::handleMemberPacket);
    connect(&m_hostCodec, &PacketCodec::protocolError, this, [this](const QString &) {
        leave();
    });
}

GroupRoom::~GroupRoom()
{
    for (auto it = m_memberConns.begin(); it != m_memberConns.end(); ++it) {
        if (it->socket) {
            it->socket->disconnect();
            it->socket->abort();
            it->socket->deleteLater();
        }
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
    if (newName.trimmed().isEmpty() || m_localDeviceName == newName.trimmed()) {
        return;
    }
    m_localDeviceName = newName.trimmed();
    m_messages.updateSenderName(m_localDeviceId, m_localDeviceName);

    QJsonObject obj;
    obj.insert(QStringLiteral("groupId"), m_groupId);
    obj.insert(QStringLiteral("deviceId"), m_localDeviceId);
    obj.insert(QStringLiteral("deviceName"), m_localDeviceName);
    obj.insert(QStringLiteral("platform"), m_localPlatform);

    if (m_isHost) {
        m_hostName = m_localDeviceName;
        GroupMemberInfo self;
        self.deviceId = m_localDeviceId;
        self.deviceName = m_localDeviceName;
        self.platform = m_localPlatform;
        self.ipAddress = AndroidHelper::localIpAddress();
        self.isHost = true;
        m_memberModel.upsertMember(self);

        QJsonObject sync;
        sync.insert(QStringLiteral("groupId"), m_groupId);
        sync.insert(QStringLiteral("event"), QStringLiteral("renamed"));
        sync.insert(QStringLiteral("actorId"), m_localDeviceId);
        sync.insert(QStringLiteral("actorName"), m_localDeviceName);
        sync.insert(QStringLiteral("members"), m_memberModel.toJsonArray());
        broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));
    } else if (m_isJoined && m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) {
        m_hostSocket->write(PacketCodec::encodeJson(MsgType::DeviceInfoUpdate, obj));
    }
}

void GroupRoom::addMemberConnection(const GroupMemberInfo &member, QTcpSocket *socket, PacketCodec *codec)
{
    if (!m_isHost || !socket || !codec) {
        return;
    }

    // Replace existing connection if the same deviceId reconnects
    if (auto oldIt = m_memberConns.find(member.deviceId); oldIt != m_memberConns.end()) {
        if (oldIt->socket) {
            oldIt->socket->disconnect(this);
            oldIt->socket->abort();
            oldIt->socket->deleteLater();
        }
        m_memberConns.erase(oldIt);
    }

    socket->setParent(this);
    codec->setParent(socket);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    m_knownMemberIds.insert(member.deviceId);

    MemberConn conn;
    conn.deviceId = member.deviceId;
    conn.socket = socket;
    conn.codec = codec;
    m_memberConns.insert(member.deviceId, conn);

    m_memberModel.upsertMember(member);

    // Send acceptance + full member list to newcomer
    QJsonObject ack;
    ack.insert(QStringLiteral("accepted"), true);
    ack.insert(QStringLiteral("groupId"), m_groupId);
    ack.insert(QStringLiteral("groupName"), m_groupName);
    ack.insert(QStringLiteral("hostId"), m_hostId);
    ack.insert(QStringLiteral("hostName"), m_hostName);
    ack.insert(QStringLiteral("members"), m_memberModel.toJsonArray());
    socket->write(PacketCodec::encodeJson(MsgType::GroupJoinAck, ack));

    // Notify existing members
    QJsonObject sync;
    sync.insert(QStringLiteral("groupId"), m_groupId);
    sync.insert(QStringLiteral("event"), QStringLiteral("joined"));
    sync.insert(QStringLiteral("actorId"), member.deviceId);
    sync.insert(QStringLiteral("actorName"), member.deviceName);
    sync.insert(QStringLiteral("members"), m_memberModel.toJsonArray());
    broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync), member.deviceId);

    appendSystemMessage(QStringLiteral("%1 加入了群聊").arg(member.deviceName));
    emit memberCountChanged(m_groupId, m_memberModel.rowCount());

    const QString peerId = member.deviceId;
    connect(socket, &QTcpSocket::readyRead, codec, [socket, codec]() {
        const QByteArray chunk = socket->readAll();
        if (!chunk.isEmpty()) {
            codec->feed(chunk);
        }
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
                if (m.deviceId == peerId) {
                    leftName = m.deviceName;
                    break;
                }
            }
            m_memberModel.removeMember(peerId);

            QJsonObject sync;
            sync.insert(QStringLiteral("groupId"), m_groupId);
            sync.insert(QStringLiteral("event"), QStringLiteral("left"));
            sync.insert(QStringLiteral("actorId"), peerId);
            sync.insert(QStringLiteral("actorName"), leftName);
            sync.insert(QStringLiteral("members"), m_memberModel.toJsonArray());
            broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));

            appendSystemMessage(QStringLiteral("%1 离开了群聊").arg(leftName));
            emit memberCountChanged(m_groupId, m_memberModel.rowCount());
        }
        socket->deleteLater();
    });

    if (socket->bytesAvailable() > 0) {
        codec->feed(socket->readAll());
    }
}

void GroupRoom::disband()
{
    if (!m_isHost) {
        return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("groupId"), m_groupId);
    obj.insert(QStringLiteral("reason"), QStringLiteral("群主已解散群聊"));
    broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupDisband, obj));

    for (auto it = m_memberConns.begin(); it != m_memberConns.end(); ++it) {
        if (it->socket) {
            it->socket->flush();
            it->socket->disconnectFromHost();
        }
    }
    m_memberConns.clear();
    m_isJoined = false;
    emit disbanded(m_groupId, QStringLiteral("群聊已解散"));
}

void GroupRoom::connectToHost(const QString &hostIp, quint16 hostPort, const QString &pinAttempt)
{
    if (m_isHost) {
        return;
    }
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
        QJsonObject req;
        req.insert(QStringLiteral("groupId"), m_groupId);
        req.insert(QStringLiteral("deviceId"), m_localDeviceId);
        req.insert(QStringLiteral("deviceName"), m_localDeviceName);
        req.insert(QStringLiteral("platform"), m_localPlatform);
        req.insert(QStringLiteral("pin"), m_pinAttempt);
        m_hostSocket->write(PacketCodec::encodeJson(MsgType::GroupJoinReq, req));
    });

    connect(m_hostSocket, &QTcpSocket::readyRead, this, [this]() {
        if (!m_hostSocket) {
            return;
        }
        const QByteArray chunk = m_hostSocket->readAll();
        if (!chunk.isEmpty()) {
            m_hostCodec.feed(chunk);
        }
    });

    connect(m_hostSocket, &QTcpSocket::disconnected, this, [this]() {
        const bool wasJoined = m_isJoined;
        m_isJoined = false;
        if (wasJoined) {
            appendSystemMessage(QStringLiteral("与群主断开连接"));
            emit joinedChanged(m_groupId, false);
        }
    });

    connect(m_hostSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_isJoined && m_hostSocket) {
            emit joinRejected(m_groupId, m_hostSocket->errorString());
        }
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
    if (m_isHost) {
        disband();
        return;
    }
    if (m_hostSocket) {
        if (m_hostSocket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject obj;
            obj.insert(QStringLiteral("groupId"), m_groupId);
            obj.insert(QStringLiteral("deviceId"), m_localDeviceId);
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
    if (trimmed.isEmpty() || !m_isJoined) {
        return;
    }

    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QJsonObject obj;
    obj.insert(QStringLiteral("groupId"), m_groupId);
    obj.insert(QStringLiteral("msgId"), msgId);
    obj.insert(QStringLiteral("senderId"), m_localDeviceId);
    obj.insert(QStringLiteral("senderName"), m_localDeviceName);
    obj.insert(QStringLiteral("text"), trimmed);
    obj.insert(QStringLiteral("timestamp"), nowMs);

    const QByteArray frame = PacketCodec::encodeJson(MsgType::GroupTextMessage, obj);
    if (m_isHost) {
        broadcastToMembers(frame);
    } else if (m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) {
        m_hostSocket->write(frame);
    } else {
        return;
    }

    ChatMessage msg;
    msg.msgId = msgId;
    msg.isOutgoing = true;
    msg.kind = QStringLiteral("text");
    msg.text = trimmed;
    msg.senderId = m_localDeviceId;
    msg.senderName = m_localDeviceName;
    msg.status = QStringLiteral("completed");
    msg.progress = 1.0;
    msg.timestampText = groupTimeText();
    m_messages.appendMessage(std::move(msg));
}

void GroupRoom::sendFiles(const QList<QUrl> &urls)
{
    if (!m_isJoined || !m_transferManager) {
        return;
    }

    const QString myIp = AndroidHelper::localIpAddress();
    const quint16 myPort = m_transferManager->serverPort();

    for (const QUrl &url : urls) {
        const PreparedFile prepared = AndroidHelper::prepareReadableFile(url);
        if (prepared.readablePath.isEmpty() || prepared.fileSize < 0) {
            continue;
        }

        const QString fileName = cleanGroupFileName(prepared.displayName);
        const QString mediaKind = classifyMediaKind(fileName);
        const QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);

        m_transferManager->registerSharedFile(transferId, prepared.readablePath, fileName, false);

        QJsonObject ann;
        ann.insert(QStringLiteral("groupId"), m_groupId);
        ann.insert(QStringLiteral("transferId"), transferId);
        ann.insert(QStringLiteral("senderId"), m_localDeviceId);
        ann.insert(QStringLiteral("senderName"), m_localDeviceName);
        ann.insert(QStringLiteral("senderIp"), myIp);
        ann.insert(QStringLiteral("senderPort"), static_cast<int>(myPort));
        ann.insert(QStringLiteral("fileName"), fileName);
        ann.insert(QStringLiteral("fileSize"), prepared.fileSize);
        ann.insert(QStringLiteral("mediaType"), mediaKind);
        ann.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

        const QByteArray frame = PacketCodec::encodeJson(MsgType::GroupFileAnnounce, ann);
        if (m_isHost) {
            broadcastToMembers(frame);
        } else if (m_hostSocket && m_hostSocket->state() == QAbstractSocket::ConnectedState) {
            m_hostSocket->write(frame);
        }

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
        if (msg.fileExtension.isEmpty()) {
            msg.fileExtension = QStringLiteral("FILE");
        }
        msg.fileSize = prepared.fileSize;
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = groupTimeText();
        m_messages.appendMessage(std::move(msg));
    }
}

void GroupRoom::downloadFile(const QString &transferId)
{
    const ChatMessage *existing = m_messages.findMessage(transferId);
    if (!existing || existing->isOutgoing || existing->senderIp.isEmpty() || existing->senderPort == 0) {
        return;
    }
    if (existing->status == QLatin1String("transferring") || existing->status == QLatin1String("completed")) {
        return;
    }

    const QString fileName = existing->fileName;
    const QString mediaKind = existing->kind;
    const QString targetIp = existing->senderIp;
    const quint16 targetPort = existing->senderPort;

    m_messages.updateTransferStatus(transferId, QStringLiteral("transferring"));

    auto *sock = new QTcpSocket(this);
    auto *codec = new PacketCodec(sock);
    sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    struct FetchState {
        QString savedPath;
        std::shared_ptr<QFile> file;
        qint64 received = 0;
        bool finished = false;
    };
    auto state = std::make_shared<FetchState>();

    connect(sock, &QTcpSocket::connected, sock, [sock, transferId, this]() {
        QJsonObject req;
        req.insert(QStringLiteral("transferId"), transferId);
        req.insert(QStringLiteral("requesterId"), m_localDeviceId);
        sock->write(PacketCodec::encodeJson(MsgType::GroupFileFetchReq, req));
    });

    connect(sock, &QTcpSocket::readyRead, codec, [sock, codec]() {
        const QByteArray chunk = sock->readAll();
        if (!chunk.isEmpty()) {
            codec->feed(chunk);
        }
    });

    connect(codec, &PacketCodec::packetReceived, this, [this, sock, state, transferId, fileName, mediaKind](MsgType type, const QByteArray &payload) {
        if (type == MsgType::FileMeta) {
            state->savedPath = makeUniqueSavePath(fileName);
            state->file = std::make_shared<QFile>(state->savedPath);
            if (!state->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                state->file.reset();
                m_messages.updateTransferStatus(transferId, QStringLiteral("failed"));
                sock->abort();
            }
        } else if (type == MsgType::FileChunk) {
            if (!state->file || payload.size() < kTransferIdByteSize) {
                return;
            }
            const qint64 n = payload.size() - kTransferIdByteSize;
            if (n > 0) {
                state->file->write(payload.constData() + kTransferIdByteSize, n);
                state->received += n;
                m_messages.updateTransferProgress(transferId, state->received);
            }
        } else if (type == MsgType::FileComplete) {
            if (state->file) {
                state->file->flush();
                state->file->close();
                state->file.reset();
            }
            state->finished = true;
#ifdef Q_OS_ANDROID
            AndroidHelper::exportToPublicStorage(state->savedPath, fileName, mediaKind);
#endif
            m_messages.updateTransferStatus(transferId, QStringLiteral("completed"), state->savedPath);
            sock->disconnectFromHost();
        }
    });

    connect(sock, &QTcpSocket::disconnected, this, [this, sock, state, transferId]() {
        if (!state->finished) {
            if (state->file) {
                state->file->close();
                QFile::remove(state->savedPath);
            }
            m_messages.updateTransferStatus(transferId, QStringLiteral("failed"));
        }
        sock->deleteLater();
    });

    connect(sock, &QTcpSocket::errorOccurred, this, [this, sock, state, transferId](QAbstractSocket::SocketError) {
        if (!state->finished) {
            m_messages.updateTransferStatus(transferId, QStringLiteral("failed"));
            sock->deleteLater();
        }
    });

    sock->connectToHost(targetIp, targetPort);
}

void GroupRoom::appendSystemMessage(const QString &text)
{
    ChatMessage msg;
    msg.msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.kind = QStringLiteral("system");
    msg.text = text;
    msg.status = QStringLiteral("completed");
    msg.timestampText = groupTimeText();
    m_messages.appendMessage(std::move(msg));
}

void GroupRoom::broadcastToMembers(const QByteArray &frame, const QString &excludeId)
{
    for (auto it = m_memberConns.constBegin(); it != m_memberConns.constEnd(); ++it) {
        if (!excludeId.isEmpty() && it.key() == excludeId) {
            continue;
        }
        if (it->socket && it->socket->state() == QAbstractSocket::ConnectedState) {
            it->socket->write(frame);
        }
    }
}

void GroupRoom::handleHostPacket(const QString &senderId, MsgType type, const QByteArray &payload)
{
    switch (type) {
    case MsgType::GroupTextMessage: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        // Relay to all other members
        broadcastToMembers(PacketCodec::encode(MsgType::GroupTextMessage, payload), senderId);

        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString();
        msg.isOutgoing = false;
        msg.kind = QStringLiteral("text");
        msg.senderId = senderId;
        msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = groupTimeText();
        m_messages.appendMessage(std::move(msg));

        ++m_unreadCount;
        emit unreadCountChanged(m_groupId, m_unreadCount);
        break;
    }

    case MsgType::GroupFileAnnounce: {
        QJsonObject obj = QJsonDocument::fromJson(payload).object();
        // Ensure senderIp reflects the actual reachable socket peerAddress seen by the Host
        if (auto it = m_memberConns.constFind(senderId); it != m_memberConns.constEnd() && it->socket) {
            const QHostAddress addr = it->socket->peerAddress();
            bool ok = false;
            const quint32 ipv4 = addr.toIPv4Address(&ok);
            const QString realIp = ok ? QHostAddress(ipv4).toString() : addr.toString();
            if (!realIp.isEmpty()) {
                obj.insert(QStringLiteral("senderIp"), realIp);
            }
        }
        broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupFileAnnounce, obj), senderId);
        handleAnnounceMessage(obj);
        break;
    }

    case MsgType::DeviceInfoUpdate: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString newName = obj.value(QStringLiteral("deviceName")).toString().trimmed();
        if (!newName.isEmpty()) {
            for (GroupMemberInfo m : m_memberModel.members()) {
                if (m.deviceId == senderId) {
                    m.deviceName = newName;
                    m_memberModel.upsertMember(m);
                    break;
                }
            }
            m_messages.updateSenderName(senderId, newName);
            QJsonObject sync;
            sync.insert(QStringLiteral("groupId"), m_groupId);
            sync.insert(QStringLiteral("event"), QStringLiteral("renamed"));
            sync.insert(QStringLiteral("actorId"), senderId);
            sync.insert(QStringLiteral("actorName"), newName);
            sync.insert(QStringLiteral("members"), m_memberModel.toJsonArray());
            broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));
        }
        break;
    }

    case MsgType::GroupLeave: {
        if (auto it = m_memberConns.find(senderId); it != m_memberConns.end() && it->socket) {
            it->socket->disconnectFromHost();
        }
        break;
    }

    default:
        break;
    }
}

void GroupRoom::handleMemberPacket(MsgType type, const QByteArray &payload)
{
    const QJsonObject obj = QJsonDocument::fromJson(payload).object();
    switch (type) {
    case MsgType::GroupJoinAck: {
        const bool accepted = obj.value(QStringLiteral("accepted")).toBool(false);
        if (!accepted) {
            m_shouldMaintainConnection = false;
            const QString reason = obj.value(QStringLiteral("reason")).toString(QStringLiteral("加入被拒绝"));
            if (m_hostSocket) {
                m_hostSocket->disconnect(this);
                m_hostSocket->disconnectFromHost();
                m_hostSocket->deleteLater();
                m_hostSocket = nullptr;
            }
            emit joinRejected(m_groupId, reason);
            return;
        }

        m_groupName = obj.value(QStringLiteral("groupName")).toString(m_groupName);
        m_hostName = obj.value(QStringLiteral("hostName")).toString(m_hostName);

        QVector<GroupMemberInfo> list;
        const QJsonArray arr = obj.value(QStringLiteral("members")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject mObj = v.toObject();
            GroupMemberInfo m;
            m.deviceId = mObj.value(QStringLiteral("deviceId")).toString();
            m.deviceName = mObj.value(QStringLiteral("deviceName")).toString();
            m.platform = mObj.value(QStringLiteral("platform")).toString();
            m.ipAddress = mObj.value(QStringLiteral("ipAddress")).toString();
            m.isHost = mObj.value(QStringLiteral("isHost")).toBool(false);
            m_messages.updateSenderName(m.deviceId, m.deviceName);
            list.append(std::move(m));
        }
        m_memberModel.setMembers(std::move(list));
        m_isJoined = true;
        appendSystemMessage(QStringLiteral("已加入群聊「%1」").arg(m_groupName));
        emit joinedChanged(m_groupId, true);
        emit memberCountChanged(m_groupId, m_memberModel.rowCount());
        break;
    }

    case MsgType::GroupMemberSync: {
        QVector<GroupMemberInfo> list;
        const QJsonArray arr = obj.value(QStringLiteral("members")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject mObj = v.toObject();
            GroupMemberInfo m;
            m.deviceId = mObj.value(QStringLiteral("deviceId")).toString();
            m.deviceName = mObj.value(QStringLiteral("deviceName")).toString();
            m.platform = mObj.value(QStringLiteral("platform")).toString();
            m.ipAddress = mObj.value(QStringLiteral("ipAddress")).toString();
            m.isHost = mObj.value(QStringLiteral("isHost")).toBool(false);
            m_messages.updateSenderName(m.deviceId, m.deviceName);
            list.append(std::move(m));
        }
        m_memberModel.setMembers(std::move(list));
        emit memberCountChanged(m_groupId, m_memberModel.rowCount());

        const QString ev = obj.value(QStringLiteral("event")).toString();
        const QString actor = obj.value(QStringLiteral("actorName")).toString();
        if (ev == QLatin1String("joined") && !actor.isEmpty()) {
            appendSystemMessage(QStringLiteral("%1 加入了群聊").arg(actor));
        } else if (ev == QLatin1String("left") && !actor.isEmpty()) {
            appendSystemMessage(QStringLiteral("%1 离开了群聊").arg(actor));
        }
        break;
    }

    case MsgType::GroupTextMessage: {
        const QString senderId = obj.value(QStringLiteral("senderId")).toString();
        if (senderId == m_localDeviceId) {
            return;
        }
        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString();
        msg.isOutgoing = false;
        msg.kind = QStringLiteral("text");
        msg.senderId = senderId;
        msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = groupTimeText();
        m_messages.appendMessage(std::move(msg));

        ++m_unreadCount;
        emit unreadCountChanged(m_groupId, m_unreadCount);
        break;
    }

    case MsgType::GroupFileAnnounce: {
        QJsonObject objCopy = obj;
        const QString senderId = objCopy.value(QStringLiteral("senderId")).toString();
        if (senderId == m_localDeviceId) {
            return;
        }
        // If sent by the Host, guarantee senderIp matches the active m_hostSocket peerAddress
        if (senderId == m_hostId && m_hostSocket) {
            const QHostAddress addr = m_hostSocket->peerAddress();
            bool ok = false;
            const quint32 ipv4 = addr.toIPv4Address(&ok);
            const QString hostPeerIp = ok ? QHostAddress(ipv4).toString() : addr.toString();
            if (!hostPeerIp.isEmpty()) {
                objCopy.insert(QStringLiteral("senderIp"), hostPeerIp);
            }
        }
        handleAnnounceMessage(objCopy);
        break;
    }

    case MsgType::GroupDisband: {
        m_shouldMaintainConnection = false;
        const QString reason = obj.value(QStringLiteral("reason")).toString(QStringLiteral("群主已解散群聊"));
        m_isJoined = false;
        if (m_hostSocket) {
            m_hostSocket->disconnect(this);
            m_hostSocket->disconnectFromHost();
            m_hostSocket->deleteLater();
            m_hostSocket = nullptr;
        }
        appendSystemMessage(reason);
        emit disbanded(m_groupId, reason);
        break;
    }

    default:
        break;
    }
}

void GroupRoom::handleAnnounceMessage(const QJsonObject &obj)
{
    const QString transferId = obj.value(QStringLiteral("transferId")).toString();
    const QString fileName = cleanGroupFileName(obj.value(QStringLiteral("fileName")).toString());
    const qint64 fileSize = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble(0));
    const QString mediaType = obj.value(QStringLiteral("mediaType")).toString(classifyMediaKind(fileName));

    ChatMessage msg;
    msg.msgId = transferId;
    msg.isOutgoing = false;
    msg.kind = mediaType;
    msg.senderId = obj.value(QStringLiteral("senderId")).toString();
    msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
    msg.senderIp = obj.value(QStringLiteral("senderIp")).toString();
    msg.senderPort = static_cast<quint16>(obj.value(QStringLiteral("senderPort")).toInt(0));
    msg.fileName = fileName;
    msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
    if (msg.fileExtension.isEmpty()) {
        msg.fileExtension = QStringLiteral("FILE");
    }
    msg.fileSize = fileSize;
    msg.status = QStringLiteral("available");
    msg.progress = 0.0;
    msg.timestampText = groupTimeText();
    m_messages.appendMessage(std::move(msg));

    ++m_unreadCount;
    emit unreadCountChanged(m_groupId, m_unreadCount);

    // Automatically fetch images so thumbnails render inline immediately
    if (mediaType == QLatin1String("image")) {
        downloadFile(transferId);
    }
}

QString GroupRoom::makeUniqueSavePath(const QString &fileName) const
{
    const QString dirPath = m_transferManager ? m_transferManager->downloadDirectory() : AndroidHelper::downloadDirectory();
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString candidate = dir.filePath(fileName);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString ext = info.suffix();
    for (int counter = 1; counter < 10000; ++counter) {
        const QString nextName = ext.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(counter)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(counter).arg(ext);
        candidate = dir.filePath(nextName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return dir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('_') + fileName);
}

// ============================================================================
// GroupManager
// ============================================================================

GroupManager::GroupManager(QString localDeviceId,
                           QString localDeviceName,
                           QString localPlatform,
                           TransferManager *transferManager,
                           QObject *parent)
    : QObject(parent)
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_transferManager(transferManager)
{
}

GroupManager::~GroupManager() = default;

QString GroupManager::activeGroupName() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->groupName();
    }
    return {};
}

int GroupManager::activeGroupMemberCount() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->memberCount();
    }
    return 0;
}

bool GroupManager::activeGroupIsHost() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->isHost();
    }
    return false;
}

bool GroupManager::activeGroupIsJoined() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->isJoined();
    }
    return false;
}

QObject *GroupManager::activeGroupMessageModel() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->messages();
    }
    return nullptr;
}

QObject *GroupManager::activeGroupMemberModel() const
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        return r->memberModel();
    }
    return nullptr;
}

QString GroupManager::incomingJoinGroupName() const
{
    return m_pendingJoins.isEmpty() ? QString{} : m_pendingJoins.first().groupName;
}

QString GroupManager::incomingJoinPeerName() const
{
    return m_pendingJoins.isEmpty() ? QString{} : m_pendingJoins.first().member.deviceName;
}

GroupRoom *GroupManager::roomForId(const QString &groupId) const
{
    return m_rooms.value(groupId, nullptr);
}

QString GroupManager::createGroup(const QString &groupName, const QString &pin)
{
    QString cleanedName = groupName.trimmed();
    if (cleanedName.isEmpty()) {
        cleanedName = m_localDeviceName + QStringLiteral(" 的群聊");
    }
    if (cleanedName.size() > 24) {
        cleanedName = cleanedName.left(24);
    }

    const QString groupId = QStringLiteral("grp-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    auto *room = new GroupRoom(
        groupId, cleanedName, m_localDeviceId, m_localDeviceName, pin.trimmed(),
        true, m_localDeviceId, m_localDeviceName, m_localPlatform, m_transferManager, this);
    m_rooms.insert(groupId, room);

    connect(room, &GroupRoom::memberCountChanged, this, [this](const QString &gid, int count) {
        GroupRoom *r = roomForId(gid);
        emit groupStatusUpdated(gid, r ? r->isHost() : false, r ? r->isJoined() : false, count, r ? r->unreadCount() : 0);
        if (gid == m_activeGroupId) {
            emit activeGroupMetaChanged();
        }
        publishHostedGroups();
    });

    connect(room, &GroupRoom::unreadCountChanged, this, [this](const QString &gid, int unread) {
        if (gid == m_activeGroupId && unread > 0) {
            if (GroupRoom *r = roomForId(gid)) {
                r->clearUnread();
            }
            return;
        }
        GroupRoom *r = roomForId(gid);
        emit groupStatusUpdated(gid, r ? r->isHost() : false, r ? r->isJoined() : false, r ? r->memberCount() : 0, unread);
    });

    publishHostedGroups();
    selectGroup(groupId);
    return groupId;
}

void GroupManager::joinGroup(const QString &groupId,
                             const QString &groupName,
                             const QString &hostId,
                             const QString &hostName,
                             const QString &hostIp,
                             quint16 hostPort,
                             const QString &pin)
{
    if (groupId.isEmpty()) {
        return;
    }

    GroupRoom *room = roomForId(groupId);
    if (room && room->isJoined()) {
        selectGroup(groupId);
        return;
    }

    if (!room) {
        room = new GroupRoom(
            groupId, groupName, hostId, hostName, QString(),
            false, m_localDeviceId, m_localDeviceName, m_localPlatform, m_transferManager, this);
        m_rooms.insert(groupId, room);

        connect(room, &GroupRoom::joinedChanged, this, [this](const QString &gid, bool joined) {
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, joined, r ? r->memberCount() : 0, r ? r->unreadCount() : 0);
            if (joined) {
                selectGroup(gid);
            }
            if (gid == m_activeGroupId) {
                emit activeGroupMetaChanged();
            }
        });

        connect(room, &GroupRoom::joinRejected, this, [this](const QString &gid, const QString &reason) {
            emit groupNotice(reason);
            if (GroupRoom *r = m_rooms.take(gid)) {
                r->deleteLater();
            }
        });

        connect(room, &GroupRoom::memberCountChanged, this, [this](const QString &gid, int count) {
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, r ? r->isJoined() : false, count, r ? r->unreadCount() : 0);
            if (gid == m_activeGroupId) {
                emit activeGroupMetaChanged();
            }
        });

        connect(room, &GroupRoom::unreadCountChanged, this, [this](const QString &gid, int unread) {
            if (gid == m_activeGroupId && unread > 0) {
                if (GroupRoom *r = roomForId(gid)) {
                    r->clearUnread();
                }
                return;
            }
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, r ? r->isJoined() : false, r ? r->memberCount() : 0, unread);
        });

        connect(room, &GroupRoom::disbanded, this, [this](const QString &gid, const QString &reason) {
            emit groupNotice(reason);
            emit groupStatusUpdated(gid, false, false, 0, 0);
            if (gid == m_activeGroupId) {
                emit activeGroupMetaChanged();
            }
        });
    }

    room->connectToHost(hostIp, hostPort, pin);
}

void GroupManager::selectGroup(const QString &groupId)
{
    if (m_transferManager) {
        m_transferManager->clearActivePeer();
    }
    if (m_activeGroupId == groupId) {
        if (GroupRoom *r = roomForId(groupId)) {
            r->clearUnread();
        }
        return;
    }
    m_activeGroupId = groupId;
    if (GroupRoom *r = roomForId(groupId)) {
        r->clearUnread();
    }
    emit activeGroupChanged();
    emit activeGroupMetaChanged();
}

void GroupManager::clearActiveGroup()
{
    if (!m_activeGroupId.isEmpty()) {
        m_activeGroupId.clear();
        emit activeGroupChanged();
        emit activeGroupMetaChanged();
    }
}

void GroupManager::leaveOrDisbandActiveGroup()
{
    GroupRoom *r = roomForId(m_activeGroupId);
    if (!r) {
        return;
    }
    const QString gid = m_activeGroupId;
    const bool wasHost = r->isHost();
    r->leave();

    if (wasHost) {
        m_rooms.remove(gid);
        r->deleteLater();
        publishHostedGroups();
        emit groupRemoved(gid);
        clearActiveGroup();
    } else {
        emit groupStatusUpdated(gid, false, false, 0, 0);
        clearActiveGroup();
    }
}

void GroupManager::sendText(const QString &text)
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        r->sendText(text);
    }
}

void GroupManager::sendFiles(const QList<QUrl> &urls)
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        r->sendFiles(urls);
    }
}

void GroupManager::downloadGroupFile(const QString &transferId)
{
    if (GroupRoom *r = roomForId(m_activeGroupId)) {
        r->downloadFile(transferId);
    }
}

void GroupManager::setLocalDeviceName(const QString &newName)
{
    if (newName.trimmed().isEmpty()) {
        return;
    }
    m_localDeviceName = newName.trimmed();
    for (GroupRoom *r : m_rooms) {
        r->updateLocalDeviceName(m_localDeviceName);
    }
    publishHostedGroups();
}

void GroupManager::reconnectDroppedGroups()
{
    for (GroupRoom *r : m_rooms) {
        r->reconnectIfDropped();
    }
}

void GroupManager::onIncomingGroupJoin(QTcpSocket *socket, PacketCodec *codec, const QByteArray &payload)
{
    const QJsonObject req = QJsonDocument::fromJson(payload).object();
    const QString groupId = req.value(QStringLiteral("groupId")).toString();
    const QString devId = req.value(QStringLiteral("deviceId")).toString();
    const QString devName = req.value(QStringLiteral("deviceName")).toString(QStringLiteral("Unknown"));
    const QString platform = req.value(QStringLiteral("platform")).toString(QStringLiteral("desktop"));
    const QString pinAttempt = req.value(QStringLiteral("pin")).toString();

    GroupRoom *room = roomForId(groupId);
    auto rejectSocket = [&](const QString &reason) {
        QJsonObject ack;
        ack.insert(QStringLiteral("accepted"), false);
        ack.insert(QStringLiteral("groupId"), groupId);
        ack.insert(QStringLiteral("reason"), reason);
        socket->write(PacketCodec::encodeJson(MsgType::GroupJoinAck, ack));
        socket->flush();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        socket->disconnectFromHost();
        codec->deleteLater();
    };

    if (!room || !room->isHost() || devId.isEmpty() || devId == m_localDeviceId) {
        rejectSocket(QStringLiteral("群聊不存在或已解散"));
        return;
    }

    const QHostAddress addr = socket->peerAddress();
    bool ok = false;
    const quint32 ipv4 = addr.toIPv4Address(&ok);
    const QString ipStr = ok ? QHostAddress(ipv4).toString() : addr.toString();

    GroupMemberInfo member;
    member.deviceId = devId;
    member.deviceName = devName;
    member.platform = platform;
    member.ipAddress = ipStr;
    member.isHost = false;

    if (room->hasPin()) {
        if (pinAttempt == room->pin() || room->isKnownMember(devId)) {
            room->addMemberConnection(member, socket, codec);
        } else {
            rejectSocket(QStringLiteral("口令错误"));
        }
        return;
    }

    // Auto-approve returning members who temporarily dropped in background
    if (room->isKnownMember(devId)) {
        room->addMemberConnection(member, socket, codec);
        return;
    }

    // No PIN set: prompt host for lightweight confirmation
    PendingGroupJoin pending;
    pending.groupId = groupId;
    pending.groupName = room->groupName();
    pending.member = member;
    pending.socket = socket;
    pending.codec = codec;
    m_pendingJoins.append(pending);
    emit incomingJoinChanged();
}

void GroupManager::acceptIncomingJoin()
{
    if (m_pendingJoins.isEmpty()) {
        return;
    }
    const PendingGroupJoin item = m_pendingJoins.takeFirst();
    emit incomingJoinChanged();

    GroupRoom *room = roomForId(item.groupId);
    if (!room || !room->isHost() || !item.socket || item.socket->state() != QAbstractSocket::ConnectedState) {
        if (item.socket) {
            item.socket->deleteLater();
        }
        if (item.codec) {
            item.codec->deleteLater();
        }
        return;
    }
    room->addMemberConnection(item.member, item.socket, item.codec);
}

void GroupManager::rejectIncomingJoin()
{
    if (m_pendingJoins.isEmpty()) {
        return;
    }
    const PendingGroupJoin item = m_pendingJoins.takeFirst();
    emit incomingJoinChanged();

    if (item.socket) {
        if (item.socket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject ack;
            ack.insert(QStringLiteral("accepted"), false);
            ack.insert(QStringLiteral("groupId"), item.groupId);
            ack.insert(QStringLiteral("reason"), QStringLiteral("群主拒绝了加入申请"));
            item.socket->write(PacketCodec::encodeJson(MsgType::GroupJoinAck, ack));
            item.socket->flush();
            connect(item.socket, &QTcpSocket::disconnected, item.socket, &QObject::deleteLater);
            item.socket->disconnectFromHost();
        } else {
            item.socket->deleteLater();
        }
    }
    if (item.codec) {
        item.codec->deleteLater();
    }
}

void GroupManager::publishHostedGroups()
{
    QJsonArray arr;
    for (GroupRoom *r : m_rooms) {
        if (!r->isHost()) {
            continue;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("groupId"), r->groupId());
        obj.insert(QStringLiteral("groupName"), r->groupName());
        obj.insert(QStringLiteral("memberCount"), r->memberCount());
        obj.insert(QStringLiteral("hasPin"), r->hasPin());
        arr.append(obj);
    }
    emit hostedGroupsUpdated(arr);
}

} // namespace landrop
