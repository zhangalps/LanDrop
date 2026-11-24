#include "core/model/NearbyGroupModel.h"

namespace landrop {

NearbyGroupModel::NearbyGroupModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NearbyGroupModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_groups.size());
}

QVariant NearbyGroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_groups.size()) {
        return {};
    }

    const DiscoveredGroup &g = m_groups.at(index.row());
    switch (role) {
    case GroupIdRole:     return g.groupId;
    case GroupNameRole:   return g.groupName;
    case HostIdRole:      return g.hostId;
    case HostNameRole:    return g.hostName;
    case HostIpRole:      return g.hostIp;
    case HostPortRole:    return g.hostPort;
    case MemberCountRole: return g.memberCount;
    case HasPinRole:      return g.hasPin;
    case IsHostRole:      return g.isHost;
    case IsJoinedRole:    return g.isJoined;
    case UnreadCountRole: return g.unreadCount;
    default:              return {};
    }
}

QHash<int, QByteArray> NearbyGroupModel::roleNames() const
{
    return {
        {GroupIdRole,     "groupId"},
        {GroupNameRole,   "groupName"},
        {HostIdRole,      "hostId"},
        {HostNameRole,    "hostName"},
        {HostIpRole,      "hostIp"},
        {HostPortRole,    "hostPort"},
        {MemberCountRole, "memberCount"},
        {HasPinRole,      "hasPin"},
        {IsHostRole,      "isHost"},
        {IsJoinedRole,    "isJoined"},
        {UnreadCountRole, "unreadCount"}
    };
}

void NearbyGroupModel::upsertGroup(DiscoveredGroup group)
{
    if (const auto it = m_cachedMembership.constFind(group.groupId); it != m_cachedMembership.cend()) {
        group.isHost = it->first;
        group.isJoined = it->second;
    }
    if (const auto it = m_cachedUnread.constFind(group.groupId); it != m_cachedUnread.cend()) {
        group.unreadCount = *it;
    }

    const int idx = indexOfGroup(group.groupId);
    if (idx >= 0) {
        DiscoveredGroup &existing = m_groups[idx];
        existing.lastSeenMs = group.lastSeenMs;
        QVector<int> changed;
        if (existing.groupName != group.groupName) { existing.groupName = group.groupName; changed.append(GroupNameRole); }
        if (existing.hostName != group.hostName) { existing.hostName = group.hostName; changed.append(HostNameRole); }
        if (existing.hostIp != group.hostIp) { existing.hostIp = group.hostIp; changed.append(HostIpRole); }
        if (existing.hostPort != group.hostPort) { existing.hostPort = group.hostPort; changed.append(HostPortRole); }
        if (!existing.isHost && !existing.isJoined && existing.memberCount != group.memberCount) {
            existing.memberCount = group.memberCount;
            changed.append(MemberCountRole);
        }
        if (existing.hasPin != group.hasPin) { existing.hasPin = group.hasPin; changed.append(HasPinRole); }
        if (!changed.isEmpty()) {
            const QModelIndex mIdx = index(idx);
            emit dataChanged(mIdx, mIdx, changed);
        }
    } else {
        const int row = static_cast<int>(m_groups.size());
        beginInsertRows(QModelIndex(), row, row);
        m_groups.append(std::move(group));
        endInsertRows();
        emit countChanged();
    }
}

void NearbyGroupModel::removeGroup(const QString &groupId)
{
    m_cachedUnread.remove(groupId);
    m_cachedMembership.remove(groupId);
    const int idx = indexOfGroup(groupId);
    if (idx >= 0) {
        beginRemoveRows(QModelIndex(), idx, idx);
        m_groups.removeAt(idx);
        endRemoveRows();
        emit countChanged();
    }
}

void NearbyGroupModel::setMembershipState(const QString &groupId, bool isHost, bool isJoined, int memberCount, int unreadCount)
{
    m_cachedMembership.insert(groupId, {isHost, isJoined});
    m_cachedUnread.insert(groupId, unreadCount);
    const int idx = indexOfGroup(groupId);
    if (idx < 0) return;

    DiscoveredGroup &g = m_groups[idx];
    QVector<int> changed;
    if (g.isHost != isHost) { g.isHost = isHost; changed.append(IsHostRole); }
    if (g.isJoined != isJoined) { g.isJoined = isJoined; changed.append(IsJoinedRole); }
    if (memberCount > 0 && g.memberCount != memberCount) { g.memberCount = memberCount; changed.append(MemberCountRole); }
    if (g.unreadCount != unreadCount) { g.unreadCount = unreadCount; changed.append(UnreadCountRole); }
    if (!changed.isEmpty()) {
        const QModelIndex mIdx = index(idx);
        emit dataChanged(mIdx, mIdx, changed);
    }
}

void NearbyGroupModel::pruneTimedOut(qint64 nowMs, qint64 timeoutMs)
{
    for (int i = static_cast<int>(m_groups.size()) - 1; i >= 0; --i) {
        const DiscoveredGroup &g = m_groups.at(i);
        if (g.isHost || g.isJoined) continue;
        if (nowMs - g.lastSeenMs > timeoutMs) {
            beginRemoveRows(QModelIndex(), i, i);
            m_groups.removeAt(i);
            endRemoveRows();
            emit countChanged();
        }
    }
}

int NearbyGroupModel::indexOfGroup(const QString &groupId) const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).groupId == groupId) return i;
    }
    return -1;
}

} // namespace landrop
