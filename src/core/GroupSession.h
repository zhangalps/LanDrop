#pragma once

#include "core/PacketCodec.h"
#include "core/Protocol.h"
#include "core/TransferSession.h"

#include <QAbstractListModel>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

#include <memory>

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

class GroupRoom : public QObject
{
    Q_OBJECT

public:
    GroupRoom(QString groupId,
              QString groupName,
              QString hostId,
              QString hostName,
              QString pin,
              bool isHost,
              QString localDeviceId,
              QString localDeviceName,
              QString localPlatform,
              TransferManager *transferManager,
              QObject *parent = nullptr);
    ~GroupRoom() override;

    QString groupId() const { return m_groupId; }
    QString groupName() const { return m_groupName; }
    QString hostId() const { return m_hostId; }
    QString hostName() const { return m_hostName; }
    QString pin() const { return m_pin; }
    bool hasPin() const { return !m_pin.isEmpty(); }
    bool isHost() const { return m_isHost; }
    bool isJoined() const { return m_isJoined; }
    int memberCount() const { return m_memberModel.rowCount(); }
    int unreadCount() const { return m_unreadCount; }
    MessageListModel *messages() { return &m_messages; }
    GroupMemberListModel *memberModel() { return &m_memberModel; }

    void clearUnread();
    void updateLocalDeviceName(const QString &newName);

    // Host operations
    void addMemberConnection(const GroupMemberInfo &member, QTcpSocket *socket, PacketCodec *codec);
    bool isKnownMember(const QString &deviceId) const { return m_knownMemberIds.contains(deviceId); }
    void disband();

    // Member operations
    void connectToHost(const QString &hostIp, quint16 hostPort, const QString &pinAttempt);
    void reconnectIfDropped();
    bool shouldMaintainConnection() const { return m_shouldMaintainConnection; }
    void leave();

    // Messaging & files
    void sendText(const QString &text);
    void sendFiles(const QList<QUrl> &urls);
    void downloadFile(const QString &transferId);

signals:
    void joinedChanged(const QString &groupId, bool joined);
    void joinRejected(const QString &groupId, const QString &reason);
    void memberCountChanged(const QString &groupId, int count);
    void unreadCountChanged(const QString &groupId, int unread);
    void disbanded(const QString &groupId, const QString &reason);

private:
    struct MemberConn {
        QString deviceId;
        QTcpSocket *socket = nullptr;
        PacketCodec *codec = nullptr;
    };

    void appendSystemMessage(const QString &text);
    void broadcastToMembers(const QByteArray &frame, const QString &excludeId = {});
    void handleHostPacket(const QString &senderId, MsgType type, const QByteArray &payload);
    void handleMemberPacket(MsgType type, const QByteArray &payload);
    void handleAnnounceMessage(const QJsonObject &obj);
    QString makeUniqueSavePath(const QString &fileName) const;

    QString m_groupId;
    QString m_groupName;
    QString m_hostId;
    QString m_hostName;
    QString m_pin;
    bool m_isHost = false;
    bool m_isJoined = false;
    int m_unreadCount = 0;

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_localPlatform;
    TransferManager *m_transferManager = nullptr;

    MessageListModel m_messages;
    GroupMemberListModel m_memberModel;

    // Host state
    QHash<QString, MemberConn> m_memberConns;
    QSet<QString> m_knownMemberIds;

    // Member state
    QTcpSocket *m_hostSocket = nullptr;
    PacketCodec m_hostCodec;
    QString m_hostIp;
    quint16 m_hostPort = 0;
    QString m_pinAttempt;
    bool m_shouldMaintainConnection = false;
};

class GroupManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeGroupId READ activeGroupId NOTIFY activeGroupChanged)
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
    ~GroupManager() override;

    QString activeGroupId() const { return m_activeGroupId; }
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
    QVector<PendingGroupJoin> m_pendingJoins;
};

} // namespace landrop
