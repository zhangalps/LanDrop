#pragma once

#include "core/Protocol.h"
#include "core/model/NearbyGroupModel.h"

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

class DeviceDiscovery : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString localDeviceId READ localDeviceId CONSTANT)
    Q_PROPERTY(QString localDeviceName READ localDeviceName NOTIFY localDeviceNameChanged)
    Q_PROPERTY(QString defaultDeviceName READ defaultDeviceName CONSTANT)
    Q_PROPERTY(QString localIpAddress READ localIpAddress NOTIFY localIpAddressChanged)
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)
    Q_PROPERTY(bool discoverable READ isDiscoverable WRITE setDiscoverable NOTIFY discoverableChanged)
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
    bool isOnline() const { return m_online && m_discoverable; }
    bool isDiscoverable() const { return m_discoverable; }
    NearbyGroupModel *groupModel() { return &m_groupModel; }

    Q_INVOKABLE void setLocalDeviceName(const QString &name);
    Q_INVOKABLE void resetLocalDeviceName();
    Q_INVOKABLE void setDiscoverable(bool enabled);

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
    void discoverableChanged();
    void countChanged();

private slots:
    void sendBroadcast();
    void onReadyRead();
    void pruneTimedOutDevices();

private:
    int indexOfDevice(const QString &deviceId) const;
    void broadcastPayload(const QByteArray &payload);
    void sendOfflineNotice();
    void parseAnnouncedGroups(const QJsonArray &groupsArr,
                              const QString &hostId,
                              const QString &hostName,
                              const QString &hostIp,
                              quint16 hostPort,
                              qint64 nowMs);

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_defaultDeviceName;
    QString m_localPlatform;
    QString m_localIpAddress;
    quint16 m_tcpPort = 0;
    quint16 m_discoveryPort = kDiscoveryPort;
    bool m_online = false;
    bool m_discoverable = true;

    QUdpSocket m_udpSocket;
    QTimer m_broadcastTimer;
    QTimer m_pruneTimer;
    QVector<DiscoveredDevice> m_devices;
    QHash<QString, QPair<int, int>> m_cachedSessionStatus;

    QJsonArray m_hostedGroups;
    NearbyGroupModel m_groupModel;
};

} // namespace landrop
