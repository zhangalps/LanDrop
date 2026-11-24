#pragma once

#include "core/Protocol.h"

#include <QAbstractListModel>
#include <QJsonArray>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

namespace landrop {

struct DiscoveredDevice {
    QString deviceId;
    QString deviceName;
    QString ipAddress;
    quint16 tcpPort = 0;
    QString platform;
    qint64 lastSeenMs = 0;
    int connectionState = static_cast<int>(SessionState::Disconnected);
    int unreadCount = 0;
};

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
    QHash<QString, QPair<bool, bool>> m_cachedMembership; // groupId -> {isHost, isJoined}
};

class DeviceDiscovery : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString localDeviceId READ localDeviceId CONSTANT)
    Q_PROPERTY(QString localDeviceName READ localDeviceName NOTIFY localDeviceNameChanged)
    Q_PROPERTY(QString defaultDeviceName READ defaultDeviceName CONSTANT)
    Q_PROPERTY(QString localIpAddress READ localIpAddress NOTIFY localIpAddressChanged)
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QObject *groupModel READ groupModel CONSTANT)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        IpAddressRole,
        TcpPortRole,
        PlatformRole,
        ConnectionStateRole,
        UnreadCountRole
    };

    explicit DeviceDiscovery(QString localDeviceId,
                             QString localDeviceName,
                             QString defaultDeviceName,
                             QString localPlatform,
                             quint16 tcpPort,
                             QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString localDeviceId() const { return m_localDeviceId; }
    QString localDeviceName() const { return m_localDeviceName; }
    QString defaultDeviceName() const { return m_defaultDeviceName; }
    QString localIpAddress() const { return m_localIpAddress; }
    bool isOnline() const { return m_online; }
    NearbyGroupModel *groupModel() { return &m_groupModel; }

    Q_INVOKABLE void setLocalDeviceName(const QString &name);
    Q_INVOKABLE void resetLocalDeviceName();

    void setHostedGroups(const QJsonArray &groups);
    void start(quint16 discoveryPort = kDiscoveryPort);
    void stop();
    void refreshNow() { sendBroadcast(); }

public slots:
    void syncSessionStatus(const QString &deviceId, int connectionState, int unreadCount);
    void updateDeviceInfo(const QString &deviceId, const QString &deviceName);
    void syncGroupMembership(const QString &groupId, bool isHost, bool isJoined, int memberCount, int unreadCount);
    void removeHostedGroup(const QString &groupId);

signals:
    void localDeviceNameChanged(const QString &newName);
    void localIpAddressChanged();
    void onlineChanged();
    void countChanged();

private slots:
    void sendBroadcast();
    void onReadyRead();
    void pruneTimedOutDevices();

private:
    int indexOfDevice(const QString &deviceId) const;

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_defaultDeviceName;
    QString m_localPlatform;
    QString m_localIpAddress;
    quint16 m_tcpPort = 0;
    quint16 m_discoveryPort = kDiscoveryPort;
    bool m_online = false;

    QUdpSocket m_udpSocket;
    QTimer m_broadcastTimer;
    QTimer m_pruneTimer;
    QVector<DiscoveredDevice> m_devices;
    QHash<QString, QPair<int, int>> m_cachedSessionStatus; // deviceId -> {state, unread}

    QJsonArray m_hostedGroups;
    NearbyGroupModel m_groupModel;
};

} // namespace landrop
