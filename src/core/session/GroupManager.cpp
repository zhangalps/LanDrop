#include "core/session/GroupManager.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace landrop {

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

QString GroupManager::activeGroupName() const { auto *r = roomForId(m_activeGroupId); return r ? r->groupName() : QString{}; }
int GroupManager::activeGroupMemberCount() const { auto *r = roomForId(m_activeGroupId); return r ? r->memberCount() : 0; }
bool GroupManager::activeGroupIsHost() const { auto *r = roomForId(m_activeGroupId); return r ? r->isHost() : false; }
bool GroupManager::activeGroupIsJoined() const { auto *r = roomForId(m_activeGroupId); return r ? r->isJoined() : false; }
QObject *GroupManager::activeGroupMessageModel() const { auto *r = roomForId(m_activeGroupId); return r ? r->messages() : nullptr; }
QObject *GroupManager::activeGroupMemberModel() const { auto *r = roomForId(m_activeGroupId); return r ? r->memberModel() : nullptr; }
QString GroupManager::incomingJoinGroupName() const { return m_pendingJoins.isEmpty() ? QString{} : m_pendingJoins.first().groupName; }
QString GroupManager::incomingJoinPeerName() const { return m_pendingJoins.isEmpty() ? QString{} : m_pendingJoins.first().member.deviceName; }
GroupRoom *GroupManager::roomForId(const QString &groupId) const { return m_rooms.value(groupId, nullptr); }

QString GroupManager::createGroup(const QString &groupName, const QString &pin)
{
    QString cleanedName = groupName.trimmed();
    if (cleanedName.isEmpty()) cleanedName = m_localDeviceName + QStringLiteral(" 的群聊");
    if (cleanedName.size() > 24) cleanedName = cleanedName.left(24);

    const QString groupId = QStringLiteral("grp-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    auto *room = new GroupRoom(
        groupId, cleanedName, m_localDeviceId, m_localDeviceName, pin.trimmed(),
        true, m_localDeviceId, m_localDeviceName, m_localPlatform, m_transferManager, this);
    m_rooms.insert(groupId, room);

    connect(room, &GroupRoom::memberCountChanged, this, [this](const QString &gid, int count) {
        GroupRoom *r = roomForId(gid);
        emit groupStatusUpdated(gid, r ? r->isHost() : false, r ? r->isJoined() : false, count, r ? r->unreadCount() : 0);
        if (gid == m_activeGroupId) emit activeGroupMetaChanged();
        publishHostedGroups();
    });

    connect(room, &GroupRoom::unreadCountChanged, this, [this](const QString &gid, int unread) {
        if (gid == m_activeGroupId && unread > 0) {
            if (GroupRoom *r = roomForId(gid)) r->clearUnread();
            return;
        }
        GroupRoom *r = roomForId(gid);
        emit groupStatusUpdated(gid, r ? r->isHost() : false, r ? r->isJoined() : false, r ? r->memberCount() : 0, unread);
    });

    publishHostedGroups();
    selectGroup(groupId);
    return groupId;
}

void GroupManager::joinGroup(const QString &groupId, const QString &groupName,
                             const QString &hostId, const QString &hostName,
                             const QString &hostIp, quint16 hostPort, const QString &pin)
{
    if (groupId.isEmpty()) return;

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
            if (m_joiningGroupId == gid) {
                m_joiningGroupId.clear();
                emit joiningGroupChanged();
            }
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, joined, r ? r->memberCount() : 0, r ? r->unreadCount() : 0);
            if (joined) selectGroup(gid);
            if (gid == m_activeGroupId) emit activeGroupMetaChanged();
        });
        connect(room, &GroupRoom::joinRejected, this, [this](const QString &gid, const QString &reason) {
            if (m_joiningGroupId == gid) {
                m_joiningGroupId.clear();
                emit joiningGroupChanged();
            }
            emit groupNotice(reason);
            if (GroupRoom *r = m_rooms.take(gid)) r->deleteLater();
        });
        connect(room, &GroupRoom::memberCountChanged, this, [this](const QString &gid, int count) {
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, r ? r->isJoined() : false, count, r ? r->unreadCount() : 0);
            if (gid == m_activeGroupId) emit activeGroupMetaChanged();
        });
        connect(room, &GroupRoom::unreadCountChanged, this, [this](const QString &gid, int unread) {
            if (gid == m_activeGroupId && unread > 0) {
                if (GroupRoom *r = roomForId(gid)) r->clearUnread();
                return;
            }
            GroupRoom *r = roomForId(gid);
            emit groupStatusUpdated(gid, false, r ? r->isJoined() : false, r ? r->memberCount() : 0, unread);
        });
        connect(room, &GroupRoom::disbanded, this, [this](const QString &gid, const QString &reason) {
            emit groupNotice(reason);
            emit groupStatusUpdated(gid, false, false, 0, 0);
            if (gid == m_activeGroupId) emit activeGroupMetaChanged();
        });
    }
    if (m_joiningGroupId != groupId) {
        m_joiningGroupId = groupId;
        emit joiningGroupChanged();
    }
    emit groupNotice(pin.isEmpty()
                         ? QStringLiteral("已申请加入「%1」，等待群主确认...").arg(groupName)
                         : QStringLiteral("正在加入「%1」...").arg(groupName));
    room->connectToHost(hostIp, hostPort, pin);
}

void GroupManager::selectGroup(const QString &groupId)
{
    if (m_transferManager) m_transferManager->clearActivePeer();
    if (m_activeGroupId == groupId) {
        if (GroupRoom *r = roomForId(groupId)) r->clearUnread();
        return;
    }
    m_activeGroupId = groupId;
    if (GroupRoom *r = roomForId(groupId)) r->clearUnread();
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
    if (!r) return;
    const QString gid = m_activeGroupId;
    const bool wasHost = r->isHost();
    r->leave();

    if (wasHost) {
        m_rooms.remove(gid);
        r->deleteLater();
        publishHostedGroups();
        emit groupRemoved(gid);
    } else {
        emit groupStatusUpdated(gid, false, false, 0, 0);
    }
    clearActiveGroup();
}

void GroupManager::sendText(const QString &text) { if (auto *r = roomForId(m_activeGroupId)) r->sendText(text); }
void GroupManager::sendFiles(const QList<QUrl> &urls) { if (auto *r = roomForId(m_activeGroupId)) r->sendFiles(urls); }
void GroupManager::downloadGroupFile(const QString &transferId) { if (auto *r = roomForId(m_activeGroupId)) r->downloadFile(transferId); }

void GroupManager::setLocalDeviceName(const QString &newName)
{
    if (newName.trimmed().isEmpty()) return;
    m_localDeviceName = newName.trimmed();
    for (GroupRoom *r : m_rooms) r->updateLocalDeviceName(m_localDeviceName);
    publishHostedGroups();
}

void GroupManager::reconnectDroppedGroups()
{
    for (GroupRoom *r : m_rooms) r->reconnectIfDropped();
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
        QJsonObject ack{{QStringLiteral("accepted"), false}, {QStringLiteral("groupId"), groupId}, {QStringLiteral("reason"), reason}};
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
    GroupMemberInfo member{devId, devName, platform, ok ? QHostAddress(ipv4).toString() : addr.toString(), false};

    if (room->hasPin()) {
        if (pinAttempt == room->pin() || room->isKnownMember(devId)) room->addMemberConnection(member, socket, codec);
        else rejectSocket(QStringLiteral("口令错误"));
        return;
    }
    if (room->isKnownMember(devId)) {
        room->addMemberConnection(member, socket, codec);
        return;
    }

    m_pendingJoins.append(PendingGroupJoin{groupId, room->groupName(), member, socket, codec});
    emit incomingJoinChanged();
}

void GroupManager::acceptIncomingJoin()
{
    if (m_pendingJoins.isEmpty()) return;
    const PendingGroupJoin item = m_pendingJoins.takeFirst();
    emit incomingJoinChanged();

    GroupRoom *room = roomForId(item.groupId);
    if (!room || !room->isHost() || !item.socket || item.socket->state() != QAbstractSocket::ConnectedState) {
        if (item.socket) item.socket->deleteLater();
        if (item.codec) item.codec->deleteLater();
        return;
    }
    room->addMemberConnection(item.member, item.socket, item.codec);
}

void GroupManager::rejectIncomingJoin()
{
    if (m_pendingJoins.isEmpty()) return;
    const PendingGroupJoin item = m_pendingJoins.takeFirst();
    emit incomingJoinChanged();

    if (item.socket) {
        if (item.socket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject ack{{QStringLiteral("accepted"), false}, {QStringLiteral("groupId"), item.groupId},
                            {QStringLiteral("reason"), QStringLiteral("群主拒绝了加入申请")}};
            item.socket->write(PacketCodec::encodeJson(MsgType::GroupJoinAck, ack));
            item.socket->flush();
            connect(item.socket, &QTcpSocket::disconnected, item.socket, &QObject::deleteLater);
            item.socket->disconnectFromHost();
        } else {
            item.socket->deleteLater();
        }
    }
    if (item.codec) item.codec->deleteLater();
}

void GroupManager::publishHostedGroups()
{
    QJsonArray arr;
    for (GroupRoom *r : m_rooms) {
        if (!r->isHost()) continue;
        arr.append(QJsonObject{
            {QStringLiteral("groupId"), r->groupId()},
            {QStringLiteral("groupName"), r->groupName()},
            {QStringLiteral("memberCount"), r->memberCount()},
            {QStringLiteral("hasPin"), r->hasPin()}
        });
    }
    emit hostedGroupsUpdated(arr);
}

} // namespace landrop
