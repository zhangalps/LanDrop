#include "core/session/GroupRoom.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace landrop {

namespace {

QString packetTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

QString cleanAnnouncedFileName(const QString &name)
{
    QString cleaned = QFileInfo(name).fileName().trimmed();
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return cleaned.isEmpty() ? QStringLiteral("file") : cleaned;
}

QString peerSocketIpv4(QTcpSocket *sock)
{
    if (!sock) return {};
    const QHostAddress addr = sock->peerAddress();
    bool ok = false;
    const quint32 ipv4 = addr.toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : addr.toString();
}

} // namespace

void GroupRoom::handleHostPacket(const QString &senderId, MsgType type, const QByteArray &payload)
{
    switch (type) {
    case MsgType::GroupTextMessage: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        broadcastToMembers(PacketCodec::encode(MsgType::GroupTextMessage, payload), senderId);
        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString();
        msg.kind = QStringLiteral("text");
        msg.senderId = senderId;
        msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = packetTimeText();
        m_messages.appendMessage(std::move(msg));
        emit unreadCountChanged(m_groupId, ++m_unreadCount);
        break;
    }
    case MsgType::GroupFileAnnounce: {
        QJsonObject obj = QJsonDocument::fromJson(payload).object();
        if (auto it = m_memberConns.constFind(senderId); it != m_memberConns.constEnd()) {
            const QString realIp = peerSocketIpv4(it->socket);
            if (!realIp.isEmpty()) obj.insert(QStringLiteral("senderIp"), realIp);
        }
        broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupFileAnnounce, obj), senderId);
        handleAnnounceMessage(obj);
        break;
    }
    case MsgType::DeviceInfoUpdate: {
        const QString newName = QJsonDocument::fromJson(payload).object().value(QStringLiteral("deviceName")).toString().trimmed();
        if (!newName.isEmpty()) {
            for (GroupMemberInfo m : m_memberModel.members()) {
                if (m.deviceId == senderId) { m.deviceName = newName; m_memberModel.upsertMember(m); break; }
            }
            m_messages.updateSenderName(senderId, newName);
            QJsonObject sync{
                {QStringLiteral("groupId"), m_groupId}, {QStringLiteral("event"), QStringLiteral("renamed")},
                {QStringLiteral("actorId"), senderId}, {QStringLiteral("actorName"), newName},
                {QStringLiteral("members"), m_memberModel.toJsonArray()}
            };
            broadcastToMembers(PacketCodec::encodeJson(MsgType::GroupMemberSync, sync));
        }
        break;
    }
    case MsgType::GroupLeave:
        if (auto it = m_memberConns.find(senderId); it != m_memberConns.end() && it->socket) it->socket->disconnectFromHost();
        break;
    default: break;
    }
}

void GroupRoom::handleMemberPacket(MsgType type, const QByteArray &payload)
{
    const QJsonObject obj = QJsonDocument::fromJson(payload).object();
    switch (type) {
    case MsgType::GroupJoinAck: {
        if (!obj.value(QStringLiteral("accepted")).toBool(false)) {
            m_shouldMaintainConnection = false;
            if (m_hostSocket) { m_hostSocket->disconnect(this); m_hostSocket->disconnectFromHost(); m_hostSocket->deleteLater(); m_hostSocket = nullptr; }
            emit joinRejected(m_groupId, obj.value(QStringLiteral("reason")).toString(QStringLiteral("加入被拒绝")));
            return;
        }
        m_groupName = obj.value(QStringLiteral("groupName")).toString(m_groupName);
        m_hostName = obj.value(QStringLiteral("hostName")).toString(m_hostName);
        applyMembersFromJson(obj.value(QStringLiteral("members")).toArray());
        m_isJoined = true;
        appendSystemMessage(QStringLiteral("已加入群聊「%1」").arg(m_groupName));
        emit joinedChanged(m_groupId, true);
        emit memberCountChanged(m_groupId, m_memberModel.rowCount());
        break;
    }
    case MsgType::GroupMemberSync: {
        applyMembersFromJson(obj.value(QStringLiteral("members")).toArray());
        emit memberCountChanged(m_groupId, m_memberModel.rowCount());
        const QString ev = obj.value(QStringLiteral("event")).toString();
        const QString actor = obj.value(QStringLiteral("actorName")).toString();
        if (ev == QLatin1String("joined") && !actor.isEmpty()) appendSystemMessage(QStringLiteral("%1 加入了群聊").arg(actor));
        else if (ev == QLatin1String("left") && !actor.isEmpty()) appendSystemMessage(QStringLiteral("%1 离开了群聊").arg(actor));
        break;
    }
    case MsgType::GroupTextMessage: {
        const QString senderId = obj.value(QStringLiteral("senderId")).toString();
        if (senderId == m_localDeviceId) return;
        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString();
        msg.kind = QStringLiteral("text");
        msg.senderId = senderId;
        msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = packetTimeText();
        m_messages.appendMessage(std::move(msg));
        emit unreadCountChanged(m_groupId, ++m_unreadCount);
        break;
    }
    case MsgType::GroupFileAnnounce: {
        QJsonObject copy = obj;
        const QString senderId = copy.value(QStringLiteral("senderId")).toString();
        if (senderId == m_localDeviceId) return;
        if (senderId == m_hostId) {
            const QString hostPeerIp = peerSocketIpv4(m_hostSocket);
            if (!hostPeerIp.isEmpty()) copy.insert(QStringLiteral("senderIp"), hostPeerIp);
        }
        handleAnnounceMessage(copy);
        break;
    }
    case MsgType::GroupDisband: {
        m_shouldMaintainConnection = false;
        const QString reason = obj.value(QStringLiteral("reason")).toString(QStringLiteral("群主已解散群聊"));
        m_isJoined = false;
        if (m_hostSocket) { m_hostSocket->disconnect(this); m_hostSocket->disconnectFromHost(); m_hostSocket->deleteLater(); m_hostSocket = nullptr; }
        appendSystemMessage(reason);
        emit disbanded(m_groupId, reason);
        break;
    }
    default: break;
    }
}

void GroupRoom::handleAnnounceMessage(const QJsonObject &obj)
{
    const QString transferId = obj.value(QStringLiteral("transferId")).toString();
    const QString fileName = cleanAnnouncedFileName(obj.value(QStringLiteral("fileName")).toString());
    const QString mediaType = obj.value(QStringLiteral("mediaType")).toString(classifyMediaKind(fileName));

    ChatMessage msg;
    msg.msgId = transferId;
    msg.kind = mediaType;
    msg.senderId = obj.value(QStringLiteral("senderId")).toString();
    msg.senderName = obj.value(QStringLiteral("senderName")).toString(QStringLiteral("Member"));
    msg.senderIp = obj.value(QStringLiteral("senderIp")).toString();
    msg.senderPort = static_cast<quint16>(obj.value(QStringLiteral("senderPort")).toInt(0));
    msg.fileName = fileName;
    msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
    if (msg.fileExtension.isEmpty()) msg.fileExtension = QStringLiteral("FILE");
    msg.fileSize = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble(0));
    msg.status = QStringLiteral("available");
    msg.timestampText = packetTimeText();
    m_messages.appendMessage(std::move(msg));

    emit unreadCountChanged(m_groupId, ++m_unreadCount);
    if (mediaType == QLatin1String("image")) downloadFile(transferId);
}

void GroupRoom::applyMembersFromJson(const QJsonArray &arr)
{
    QVector<GroupMemberInfo> list;
    list.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject mObj = v.toObject();
        GroupMemberInfo m{
            mObj.value(QStringLiteral("deviceId")).toString(),
            mObj.value(QStringLiteral("deviceName")).toString(),
            mObj.value(QStringLiteral("platform")).toString(),
            mObj.value(QStringLiteral("ipAddress")).toString(),
            mObj.value(QStringLiteral("isHost")).toBool(false)
        };
        m_messages.updateSenderName(m.deviceId, m.deviceName);
        list.append(std::move(m));
    }
    m_memberModel.setMembers(std::move(list));
}

} // namespace landrop
