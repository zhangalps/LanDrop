#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace landrop {

struct GroupMemberInfo {
    QString deviceId;
    QString deviceName;
    QString platform;
    QString ipAddress;
    bool isHost = false;
};

class GroupMemberListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        PlatformRole,
        IpAddressRole,
        IsHostRole
    };

    explicit GroupMemberListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMembers(QVector<GroupMemberInfo> members);
    void upsertMember(const GroupMemberInfo &member);
    void removeMember(const QString &deviceId);
    const QVector<GroupMemberInfo> &members() const { return m_members; }
    QJsonArray toJsonArray() const;

signals:
    void countChanged();

private:
    QVector<GroupMemberInfo> m_members;
};

} // namespace landrop
