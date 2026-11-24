#include "core/model/GroupMemberListModel.h"

#include <QJsonObject>

namespace landrop {

GroupMemberListModel::GroupMemberListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GroupMemberListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_members.size());
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

} // namespace landrop
