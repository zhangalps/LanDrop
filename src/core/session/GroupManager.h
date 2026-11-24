#pragma once

#include "core/session/GroupRoom.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QUrl>
#include <QVector>

namespace landrop {

class GroupManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeGroupId READ activeGroupId NOTIFY activeGroupChanged)
    Q_PROPERTY(QString joiningGroupId READ joiningGroupId NOTIFY joiningGroupChanged)
    Q_PROPERTY(QString activeGroupName READ activeGroupName NOTIFY activeGroupChanged)
    Q_PROPERTY(int activeGroupMemberCount READ activeGroupMemberCount NOTIFY activeGroupMetaChanged)
    Q_PROPERTY(bool activeGroupIsHost READ activeGroupIsHost NOTIFY activeGroupChanged)
    Q_PROPERTY(bool activeGroupIsJoined READ activeGroupIsJoined NOTIFY activeGroupMetaChanged)
    Q_PROPERTY(QObject *activeGroupMessageModel READ activeGroupMessageModel NOTIFY activeGroupChanged)
    Q_PROPERTY(QObject *activeGroupMemberModel READ activeGroupMemberModel NOTIFY activeGroupChanged)
    Q_PROPERTY(bool hasIncomingJoinRequest READ hasIncomingJoinRequest NOTIFY incomingJoinChanged)
    Q_PROPERTY(QString incomingJoinGroupName READ incomingJoinGroupName NOTIFY incomingJoinChanged)
    Q_PROPERTY(QString incomingJoinPeerName READ incomingJoinPeerName NOTIFY incomingJoinChanged)

public:
    GroupManager(QString localDeviceId,
                 QString localDeviceName,
                 QString localPlatform,
                 TransferManager *transferManager,
                 QObject *parent = nullptr);
    ~GroupManager() override = default;

    QString activeGroupId() const { return m_activeGroupId; }
    QString joiningGroupId() const { return m_joiningGroupId; }
    QString activeGroupName() const;
    int activeGroupMemberCount() const;
    bool activeGroupIsHost() const;
    bool activeGroupIsJoined() const;
    QObject *activeGroupMessageModel() const;
    QObject *activeGroupMemberModel() const;

    bool hasIncomingJoinRequest() const { return !m_pendingJoins.isEmpty(); }
    QString incomingJoinGroupName() const;
    QString incomingJoinPeerName() const;

    Q_INVOKABLE QString createGroup(const QString &groupName, const QString &pin = {});
    Q_INVOKABLE void joinGroup(const QString &groupId,
                               const QString &groupName,
                               const QString &hostId,
                               const QString &hostName,
                               const QString &hostIp,
                               quint16 hostPort,
                               const QString &pin = {});
    Q_INVOKABLE void selectGroup(const QString &groupId);
    Q_INVOKABLE void clearActiveGroup();
    Q_INVOKABLE void leaveOrDisbandActiveGroup();
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void sendFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void downloadGroupFile(const QString &transferId);
    Q_INVOKABLE void acceptIncomingJoin();
    Q_INVOKABLE void rejectIncomingJoin();

public slots:
    void setLocalDeviceName(const QString &newName);
    void reconnectDroppedGroups();
    void onIncomingGroupJoin(QTcpSocket *socket, landrop::PacketCodec *codec, const QByteArray &payload);

signals:
    void activeGroupChanged();
    void joiningGroupChanged();
    void activeGroupMetaChanged();
    void incomingJoinChanged();
    void hostedGroupsUpdated(const QJsonArray &groups);
    void groupStatusUpdated(const QString &groupId, bool isHost, bool isJoined, int memberCount, int unreadCount);
    void groupRemoved(const QString &groupId);
    void groupNotice(const QString &message);

private:
    struct PendingGroupJoin {
        QString groupId;
        QString groupName;
        GroupMemberInfo member;
        QTcpSocket *socket = nullptr;
        PacketCodec *codec = nullptr;
    };

    void publishHostedGroups();
    GroupRoom *roomForId(const QString &groupId) const;

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_localPlatform;
    TransferManager *m_transferManager = nullptr;

    QHash<QString, GroupRoom *> m_rooms;
    QString m_activeGroupId;
    QString m_joiningGroupId;
    QVector<PendingGroupJoin> m_pendingJoins;
};

} // namespace landrop
