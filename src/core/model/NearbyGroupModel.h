#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

namespace landrop {

struct DiscoveredGroup {
    QString groupId;
    QString groupName;
    QString hostId;
    QString hostName;
    QString hostIp;
    quint16 hostPort = 0;
    int memberCount = 1;
    bool hasPin = false;
    bool isHost = false;
    bool isJoined = false;
    int unreadCount = 0;
    qint64 lastSeenMs = 0;
};

class NearbyGroupModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        GroupIdRole = Qt::UserRole + 1,
        GroupNameRole,
        HostIdRole,
        HostNameRole,
        HostIpRole,
        HostPortRole,
        MemberCountRole,
        HasPinRole,
        IsHostRole,
        IsJoinedRole,
        UnreadCountRole
    };

    explicit NearbyGroupModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsertGroup(DiscoveredGroup group);
    void removeGroup(const QString &groupId);
    void setMembershipState(const QString &groupId, bool isHost, bool isJoined, int memberCount, int unreadCount);
    void pruneTimedOut(qint64 nowMs, qint64 timeoutMs);

signals:
    void countChanged();

private:
    int indexOfGroup(const QString &groupId) const;

    QVector<DiscoveredGroup> m_groups;
    QHash<QString, int> m_cachedUnread;
    QHash<QString, QPair<bool, bool>> m_cachedMembership;
};

} // namespace landrop
