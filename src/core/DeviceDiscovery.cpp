#include "core/DeviceDiscovery.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSet>

namespace landrop {

// ============================================================================
// NearbyGroupModel
// ============================================================================

NearbyGroupModel::NearbyGroupModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NearbyGroupModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_groups.size());
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
    if (idx < 0) {
        return;
    }

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
        if (g.isHost || g.isJoined) {
            continue;
        }
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
        if (m_groups.at(i).groupId == groupId) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// DeviceDiscovery
// ============================================================================

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
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_devices.size());
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
    if (trimmed.isEmpty()) {
        return;
    }
    if (trimmed.size() > 24) {
        trimmed = trimmed.left(24);
    }
    if (m_localDeviceName == trimmed) {
        return;
    }
    m_localDeviceName = trimmed;
    emit localDeviceNameChanged(m_localDeviceName);
    sendBroadcast();
}

void DeviceDiscovery::resetLocalDeviceName()
{
    setLocalDeviceName(m_defaultDeviceName);
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
        QHostAddress::AnyIPv4,
        m_discoveryPort,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    if (m_online != bound) {
        m_online = bound;
        emit onlineChanged();
    }

    if (bound) {
        sendBroadcast();
        m_broadcastTimer.start();
        m_pruneTimer.start();
    }
}

void DeviceDiscovery::stop()
{
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
    if (idx < 0) {
        return;
    }

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
    if (idx < 0 || deviceName.isEmpty()) {
        return;
    }
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

void DeviceDiscovery::sendBroadcast()
{
    const QString latestIp = AndroidHelper::localIpAddress();
    if (latestIp != m_localIpAddress) {
        m_localIpAddress = latestIp;
        emit localIpAddressChanged();
    }

    if (!m_online) {
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("magic"), QStringLiteral("LDRP"));
    obj.insert(QStringLiteral("deviceId"), m_localDeviceId);
    obj.insert(QStringLiteral("deviceName"), m_localDeviceName);
    obj.insert(QStringLiteral("platform"), m_localPlatform);
    obj.insert(QStringLiteral("tcpPort"), static_cast<int>(m_tcpPort));
    if (!m_hostedGroups.isEmpty()) {
        obj.insert(QStringLiteral("groups"), m_hostedGroups);
    }

    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QSet<QString> sentTargets;
    auto sendToTarget = [&](const QHostAddress &addr) {
        if (addr.isNull()) {
            return;
        }
        const QString key = addr.toString();
        if (sentTargets.contains(key)) {
            return;
        }
        sentTargets.insert(key);
        m_udpSocket.writeDatagram(payload, addr, m_discoveryPort);
    };

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)
            || !flags.testFlag(QNetworkInterface::CanBroadcast)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            sendToTarget(entry.broadcast());
        }
    }

    // Android 11+ fallback: also send to the exact Wi-Fi subnet broadcast from DhcpInfo
    const QString wifiBroadcast = AndroidHelper::androidWifiBroadcastAddress();
    if (!wifiBroadcast.isEmpty()) {
        sendToTarget(QHostAddress(wifiBroadcast));
    }

    if (sentTargets.isEmpty()) {
        sendToTarget(QHostAddress::Broadcast);
    }

    // Also announce on localhost so two instances on the same machine can discover each other.
    sendToTarget(QHostAddress::LocalHost);
}

void DeviceDiscovery::onReadyRead()
{
    while (m_udpSocket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<qsizetype>(m_udpSocket.pendingDatagramSize()));
        QHostAddress senderAddr;
        quint16 senderPort = 0;

        if (m_udpSocket.readDatagram(datagram.data(), datagram.size(), &senderAddr, &senderPort) <= 0) {
            continue;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (!doc.isObject()) {
            continue;
        }
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("magic")).toString() != QLatin1String("LDRP")) {
            continue;
        }

        const QString devId = obj.value(QStringLiteral("deviceId")).toString();
        if (devId.isEmpty() || devId == m_localDeviceId) {
            continue;
        }

        const QString devName = obj.value(QStringLiteral("deviceName")).toString(QStringLiteral("Unknown"));
        const QString platform = obj.value(QStringLiteral("platform")).toString(QStringLiteral("desktop"));
        const int tcpPortInt = obj.value(QStringLiteral("tcpPort")).toInt(0);
        if (tcpPortInt <= 0 || tcpPortInt > 65535) {
            continue;
        }

        bool converted = false;
        const quint32 ipv4 = senderAddr.toIPv4Address(&converted);
        const QString ipStr = converted ? QHostAddress(ipv4).toString() : senderAddr.toString();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        const int existingIdx = indexOfDevice(devId);
        if (existingIdx >= 0) {
            DiscoveredDevice &dev = m_devices[existingIdx];
            dev.lastSeenMs = nowMs;
            QVector<int> changedRoles;
            if (dev.deviceName != devName) {
                dev.deviceName = devName;
                changedRoles.append(DeviceNameRole);
            }
            if (dev.ipAddress != ipStr) {
                dev.ipAddress = ipStr;
                changedRoles.append(IpAddressRole);
            }
            if (dev.tcpPort != static_cast<quint16>(tcpPortInt)) {
                dev.tcpPort = static_cast<quint16>(tcpPortInt);
                changedRoles.append(TcpPortRole);
            }
            if (dev.platform != platform) {
                dev.platform = platform;
                changedRoles.append(PlatformRole);
            }
            if (!changedRoles.isEmpty()) {
                const QModelIndex modelIdx = index(existingIdx);
                emit dataChanged(modelIdx, modelIdx, changedRoles);
            }
        } else {
            DiscoveredDevice dev;
            dev.deviceId = devId;
            dev.deviceName = devName;
            dev.ipAddress = ipStr;
            dev.tcpPort = static_cast<quint16>(tcpPortInt);
            dev.platform = platform;
            dev.lastSeenMs = nowMs;
            if (const auto it = m_cachedSessionStatus.constFind(devId); it != m_cachedSessionStatus.cend()) {
                dev.connectionState = it->first;
                dev.unreadCount = it->second;
            }

            const int newRow = static_cast<int>(m_devices.size());
            beginInsertRows(QModelIndex(), newRow, newRow);
            m_devices.append(std::move(dev));
            endInsertRows();
            emit countChanged();
        }

        // Parse hosted groups announced by this peer
        const QJsonArray groupsArr = obj.value(QStringLiteral("groups")).toArray();
        for (const QJsonValue &gVal : groupsArr) {
            const QJsonObject gObj = gVal.toObject();
            const QString gId = gObj.value(QStringLiteral("groupId")).toString();
            const QString gName = gObj.value(QStringLiteral("groupName")).toString();
            if (gId.isEmpty() || gName.isEmpty()) {
                continue;
            }
            DiscoveredGroup g;
            g.groupId = gId;
            g.groupName = gName;
            g.hostId = devId;
            g.hostName = devName;
            g.hostIp = ipStr;
            g.hostPort = static_cast<quint16>(tcpPortInt);
            g.memberCount = gObj.value(QStringLiteral("memberCount")).toInt(1);
            g.hasPin = gObj.value(QStringLiteral("hasPin")).toBool(false);
            g.lastSeenMs = nowMs;
            m_groupModel.upsertGroup(std::move(g));
        }
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
        if (m_devices.at(i).deviceId == deviceId) {
            return i;
        }
    }
    return -1;
}

} // namespace landrop
