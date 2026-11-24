#pragma once

#include <QString>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace landrop {

class AndroidNetworkBridge
{
public:
    AndroidNetworkBridge() = default;
    ~AndroidNetworkBridge();

    void acquireLocks();
    void releaseLocks();
    static void startKeepAliveService();
    static void stopKeepAliveService();
    static void requestRuntimePermissions();

    static QString wifiIpAddress();
    static QString wifiBroadcastAddress();
    static QString deviceModelName();

private:
#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
    QJniObject m_wifiLock;
#endif
};

} // namespace landrop
