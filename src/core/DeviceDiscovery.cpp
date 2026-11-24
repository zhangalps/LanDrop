#include "core/DeviceDiscovery.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSet>

namespace landrop {

DeviceDiscovery::DeviceDiscovery(QString localDeviceId,
                                 QString localDeviceName,
                                 QString defaultDeviceName,
                                 QString localPlatform,
                                 quint16 tcpPort,
                                 QObject *parent)
    : QAbstractListModel(parent)
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_defaultDeviceName(std::move(defaultDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_localIpAddress(AndroidHelper::localIpAddress())
    , m_tcpPort(tcpPort)
{
    m_broadcastTimer.setInterval(kDiscoveryIntervalMs);
    m_pruneTimer.setInterval(1000);

    connect(&m_udpSocket, &QUdpSocket::readyRead, this, &DeviceDiscovery::onReadyRead);
    connect(&m_broadcastTimer, &QTimer::timeout, this, &DeviceDiscovery::sendBroadcast);
    connect(&m_pruneTimer, &QTimer::timeout, this, &DeviceDiscovery::pruneTimedOutDevices);
}

int DeviceDiscovery::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_devices.size());
}

QVariant DeviceDiscovery::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size()) {
        return {};
    }

    const DiscoveredDevice &dev = m_devices.at(index.row());
    switch (role) {
    case DeviceIdRole:        return dev.deviceId;
    case DeviceNameRole:      return dev.deviceName;
    case IpAddressRole:       return dev.ipAddress;
    case TcpPortRole:         return dev.tcpPort;
    case PlatformRole:        return dev.platform;
    case ConnectionStateRole: return dev.connectionState;
    case UnreadCountRole:     return dev.unreadCount;
    default:                  return {};
    }
}

QHash<int, QByteArray> DeviceDiscovery::roleNames() const
{
    return {
        {DeviceIdRole,        "deviceId"},
        {DeviceNameRole,      "deviceName"},
        {IpAddressRole,       "ipAddress"},
        {TcpPortRole,         "tcpPort"},
        {PlatformRole,        "platform"},
        {ConnectionStateRole, "connectionState"},
        {UnreadCountRole,     "unreadCount"}
    };
}

void DeviceDiscovery::setLocalDeviceName(const QString &name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return;
    if (trimmed.size() > 24) trimmed = trimmed.left(24);
    if (m_localDeviceName == trimmed) return;

    m_localDeviceName = trimmed;
    emit localDeviceNameChanged(m_localDeviceName);
    sendBroadcast();
}

void DeviceDiscovery::resetLocalDeviceName()
{
    setLocalDeviceName(m_defaultDeviceName);
}

void DeviceDiscovery::setDiscoverable(bool enabled)
{
    if (m_discoverable == enabled) return;
    m_discoverable = enabled;
    emit discoverableChanged();
    emit onlineChanged();

    if (!m_online) return;
    if (m_discoverable) {
        sendBroadcast();
        m_broadcastTimer.start();
    } else {
        m_broadcastTimer.stop();
        sendOfflineNotice();
    }
}

void DeviceDiscovery::setHostedGroups(const QJsonArray &groups)
{
    m_hostedGroups = groups;
    for (const QJsonValue &val : groups) {
        const QJsonObject gObj = val.toObject();
        DiscoveredGroup g;
        g.groupId = gObj.value(QStringLiteral("groupId")).toString();
        g.groupName = gObj.value(QStringLiteral("groupName")).toString();
        g.hostId = m_localDeviceId;
        g.hostName = m_localDeviceName;
        g.hostIp = m_localIpAddress;
        g.hostPort = m_tcpPort;
        g.memberCount = gObj.value(QStringLiteral("memberCount")).toInt(1);
        g.hasPin = gObj.value(QStringLiteral("hasPin")).toBool(false);
        g.isHost = true;
        g.isJoined = true;
        g.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        if (!g.groupId.isEmpty()) {
            m_groupModel.upsertGroup(std::move(g));
        }
    }
    sendBroadcast();
}

void DeviceDiscovery::start(quint16 discoveryPort)
{
    m_discoveryPort = discoveryPort;
    if (m_udpSocket.state() == QAbstractSocket::BoundState) {
        m_udpSocket.close();
    }

    const bool bound = m_udpSocket.bind(
        QHostAddress::AnyIPv4, m_discoveryPort,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    if (m_online != bound) {
        m_online = bound;
        emit onlineChanged();
    }

    if (bound && m_discoverable) {
        sendBroadcast();
        m_broadcastTimer.start();
    }
    if (bound) {
        m_pruneTimer.start();
    }
}

void DeviceDiscovery::stop()
{
    if (m_online && m_discoverable) {
        sendOfflineNotice();
    }
    m_broadcastTimer.stop();
    m_pruneTimer.stop();
    m_udpSocket.close();
    if (m_online) {
        m_online = false;
        emit onlineChanged();
    }
}

void DeviceDiscovery::syncSessionStatus(const QString &deviceId, int connectionState, int unreadCount)
{
    m_cachedSessionStatus.insert(deviceId, {connectionState, unreadCount});
    const int idx = indexOfDevice(deviceId);
    if (idx < 0) return;

    DiscoveredDevice &dev = m_devices[idx];
    if (dev.connectionState != connectionState || dev.unreadCount != unreadCount) {
        dev.connectionState = connectionState;
        dev.unreadCount = unreadCount;
        const QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, {ConnectionStateRole, UnreadCountRole});
    }
}

void DeviceDiscovery::updateDeviceInfo(const QString &deviceId, const QString &deviceName)
{
    const int idx = indexOfDevice(deviceId);
    if (idx < 0 || deviceName.isEmpty()) return;

    DiscoveredDevice &dev = m_devices[idx];
    if (dev.deviceName != deviceName) {
        dev.deviceName = deviceName;
        const QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, {DeviceNameRole});
    }
}

void DeviceDiscovery::syncGroupMembership(const QString &groupId, bool isHost, bool isJoined, int memberCount, int unreadCount)
{
    m_groupModel.setMembershipState(groupId, isHost, isJoined, memberCount, unreadCount);
}

void DeviceDiscovery::removeHostedGroup(const QString &groupId)
{
    m_groupModel.removeGroup(groupId);
}

void DeviceDiscovery::broadcastPayload(const QByteArray &payload)
{
    QSet<QString> sentTargets;
    auto sendToTarget = [&](const QHostAddress &addr) {
        if (addr.isNull()) return;
        const QString key = addr.toString();
        if (sentTargets.contains(key)) return;
        sentTargets.insert(key);
        m_udpSocket.writeDatagram(payload, addr, m_discoveryPort);
    };

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)
            || !flags.testFlag(QNetworkInterface::CanBroadcast)) {
            continue;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                sendToTarget(entry.broadcast());
            }
        }
    }

    const QString wifiBroadcast = AndroidHelper::androidWifiBroadcastAddress();
    if (!wifiBroadcast.isEmpty()) sendToTarget(QHostAddress(wifiBroadcast));
    if (sentTargets.isEmpty()) sendToTarget(QHostAddress::Broadcast);
    sendToTarget(QHostAddress::LocalHost);
}

void DeviceDiscovery::sendOfflineNotice()
{
    if (!m_online) return;
    QJsonObject obj{
        {QStringLiteral("magic"), QStringLiteral("LDRP")},
        {QStringLiteral("type"), QStringLiteral("offline")},
        {QStringLiteral("deviceId"), m_localDeviceId}
    };
    broadcastPayload(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void DeviceDiscovery::sendBroadcast()
{
    const QString latestIp = AndroidHelper::localIpAddress();
    if (latestIp != m_localIpAddress) {
        m_localIpAddress = latestIp;
        emit localIpAddressChanged();
    }
    if (!m_online || !m_discoverable) return;

    QJsonObject obj{
        {QStringLiteral("magic"), QStringLiteral("LDRP")},
        {QStringLiteral("deviceId"), m_localDeviceId},
        {QStringLiteral("deviceName"), m_localDeviceName},
        {QStringLiteral("platform"), m_localPlatform},
        {QStringLiteral("tcpPort"), static_cast<int>(m_tcpPort)}
    };
    if (!m_hostedGroups.isEmpty()) {
        obj.insert(QStringLiteral("groups"), m_hostedGroups);
    }
    broadcastPayload(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void DeviceDiscovery::onReadyRead()
{
    while (m_udpSocket.hasPendingDatagrams()) {
        QByteArray datagram(static_cast<qsizetype>(m_udpSocket.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress senderAddr;
        quint16 senderPort = 0;
        if (m_udpSocket.readDatagram(datagram.data(), datagram.size(), &senderAddr, &senderPort) <= 0) {
            continue;
        }

        const QJsonObject obj = QJsonDocument::fromJson(datagram).object();
        if (obj.value(QStringLiteral("magic")).toString() != QLatin1String("LDRP")) continue;

        const QString devId = obj.value(QStringLiteral("deviceId")).toString();
        if (devId.isEmpty() || devId == m_localDeviceId) continue;

        if (obj.value(QStringLiteral("type")).toString() == QLatin1String("offline")) {
            if (const int idx = indexOfDevice(devId); idx >= 0) {
                beginRemoveRows(QModelIndex(), idx, idx);
                m_devices.removeAt(idx);
                endRemoveRows();
                emit countChanged();
            }
            continue;
        }

        const int tcpPortInt = obj.value(QStringLiteral("tcpPort")).toInt(0);
        if (tcpPortInt <= 0 || tcpPortInt > 65535) continue;

        const QString devName = obj.value(QStringLiteral("deviceName")).toString(QStringLiteral("Unknown"));
        const QString platform = obj.value(QStringLiteral("platform")).toString(QStringLiteral("desktop"));
        bool converted = false;
        const quint32 ipv4 = senderAddr.toIPv4Address(&converted);
        const QString ipStr = converted ? QHostAddress(ipv4).toString() : senderAddr.toString();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        const int existingIdx = indexOfDevice(devId);
        if (existingIdx >= 0) {
            DiscoveredDevice &dev = m_devices[existingIdx];
            dev.lastSeenMs = nowMs;
            QVector<int> changed;
            if (dev.deviceName != devName) { dev.deviceName = devName; changed.append(DeviceNameRole); }
            if (dev.ipAddress != ipStr) { dev.ipAddress = ipStr; changed.append(IpAddressRole); }
            if (dev.tcpPort != static_cast<quint16>(tcpPortInt)) { dev.tcpPort = static_cast<quint16>(tcpPortInt); changed.append(TcpPortRole); }
            if (dev.platform != platform) { dev.platform = platform; changed.append(PlatformRole); }
            if (!changed.isEmpty()) emit dataChanged(index(existingIdx), index(existingIdx), changed);
        } else {
            DiscoveredDevice dev{devId, devName, ipStr, static_cast<quint16>(tcpPortInt), platform, nowMs};
            if (const auto it = m_cachedSessionStatus.constFind(devId); it != m_cachedSessionStatus.cend()) {
                dev.connectionState = it->first;
                dev.unreadCount = it->second;
            }
            const int row = static_cast<int>(m_devices.size());
            beginInsertRows(QModelIndex(), row, row);
            m_devices.append(std::move(dev));
            endInsertRows();
            emit countChanged();
        }

        parseAnnouncedGroups(obj.value(QStringLiteral("groups")).toArray(),
                             devId, devName, ipStr, static_cast<quint16>(tcpPortInt), nowMs);
    }
}

void DeviceDiscovery::parseAnnouncedGroups(const QJsonArray &groupsArr,
                                           const QString &hostId,
                                           const QString &hostName,
                                           const QString &hostIp,
                                           quint16 hostPort,
                                           qint64 nowMs)
{
    for (const QJsonValue &gVal : groupsArr) {
        const QJsonObject gObj = gVal.toObject();
        const QString gId = gObj.value(QStringLiteral("groupId")).toString();
        const QString gName = gObj.value(QStringLiteral("groupName")).toString();
        if (gId.isEmpty() || gName.isEmpty()) continue;

        DiscoveredGroup g;
        g.groupId = gId;
        g.groupName = gName;
        g.hostId = hostId;
        g.hostName = hostName;
        g.hostIp = hostIp;
        g.hostPort = hostPort;
        g.memberCount = gObj.value(QStringLiteral("memberCount")).toInt(1);
        g.hasPin = gObj.value(QStringLiteral("hasPin")).toBool(false);
        g.lastSeenMs = nowMs;
        m_groupModel.upsertGroup(std::move(g));
    }
}

void DeviceDiscovery::pruneTimedOutDevices()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (int i = static_cast<int>(m_devices.size()) - 1; i >= 0; --i) {
        if (nowMs - m_devices.at(i).lastSeenMs > kDeviceTimeoutMs) {
            beginRemoveRows(QModelIndex(), i, i);
            m_devices.removeAt(i);
            endRemoveRows();
            emit countChanged();
        }
    }
    m_groupModel.pruneTimedOut(nowMs, kDeviceTimeoutMs);
}

int DeviceDiscovery::indexOfDevice(const QString &deviceId) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).deviceId == deviceId) return i;
    }
    return -1;
}

} // namespace landrop
