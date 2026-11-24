#include "platform/AndroidNetworkBridge.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QStringList>
#endif

namespace landrop {

namespace {

#ifdef Q_OS_ANDROID
int androidSdkInt()
{
    return QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
}

QString ipv4FromLittleEndianInt(jint ip)
{
    if (ip == 0) {
        return {};
    }
    const auto u = static_cast<quint32>(ip);
    return QStringLiteral("%1.%2.%3.%4")
        .arg(u & 0xFFU)
        .arg((u >> 8) & 0xFFU)
        .arg((u >> 16) & 0xFFU)
        .arg((u >> 24) & 0xFFU);
}

QJniObject androidWifiManager()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return {};
    }
    const QJniObject appCtx = context.callObjectMethod(
        "getApplicationContext", "()Landroid/content/Context;");
    const QJniObject targetCtx = appCtx.isValid() ? appCtx : context;

    const QJniObject wifiServiceStr = QJniObject::getStaticObjectField<jstring>(
        "android/content/Context", "WIFI_SERVICE");
    if (!wifiServiceStr.isValid()) {
        return {};
    }
    return targetCtx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        wifiServiceStr.object<jstring>());
}
#endif

} // namespace

AndroidNetworkBridge::~AndroidNetworkBridge()
{
    releaseLocks();
}

void AndroidNetworkBridge::acquireLocks()
{
#ifdef Q_OS_ANDROID
    const QJniObject wifiManager = androidWifiManager();
    if (!wifiManager.isValid()) {
        return;
    }

    const QJniObject lockTag = QJniObject::fromString(QStringLiteral("LanDropMulticastLock"));
    if (!m_multicastLock.isValid()) {
        m_multicastLock = wifiManager.callObjectMethod(
            "createMulticastLock",
            "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
            lockTag.object<jstring>());
        if (m_multicastLock.isValid()) {
            m_multicastLock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
            m_multicastLock.callMethod<void>("acquire", "()V");
        }
    }

    if (!m_wifiLock.isValid()) {
        // WIFI_MODE_FULL_HIGH_PERF = 3
        m_wifiLock = wifiManager.callObjectMethod(
            "createWifiLock",
            "(ILjava/lang/String;)Landroid/net/wifi/WifiManager$WifiLock;",
            jint(3),
            lockTag.object<jstring>());
        if (m_wifiLock.isValid()) {
            m_wifiLock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
            m_wifiLock.callMethod<void>("acquire", "()V");
        }
    }
#endif
}

void AndroidNetworkBridge::releaseLocks()
{
#ifdef Q_OS_ANDROID
    if (m_multicastLock.isValid()) {
        if (m_multicastLock.callMethod<jboolean>("isHeld", "()Z")) {
            m_multicastLock.callMethod<void>("release", "()V");
        }
        m_multicastLock = QJniObject();
    }
    if (m_wifiLock.isValid()) {
        if (m_wifiLock.callMethod<jboolean>("isHeld", "()Z")) {
            m_wifiLock.callMethod<void>("release", "()V");
        }
        m_wifiLock = QJniObject();
    }
#endif
}

void AndroidNetworkBridge::startKeepAliveService()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;
    QJniEnvironment env;
    QJniObject::callStaticMethod<void>(
        "org/landrop/app/LanDropKeepAliveService",
        "start",
        "(Landroid/content/Context;)V",
        context.object<jobject>());
    env.checkAndClearExceptions();
#endif
}

void AndroidNetworkBridge::stopKeepAliveService()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;
    QJniEnvironment env;
    QJniObject::callStaticMethod<void>(
        "org/landrop/app/LanDropKeepAliveService",
        "stop",
        "(Landroid/content/Context;)V",
        context.object<jobject>());
    env.checkAndClearExceptions();
#endif
}

void AndroidNetworkBridge::requestRuntimePermissions()
{
#ifdef Q_OS_ANDROID
    const int sdk = androidSdkInt();
    if (sdk < 33) return;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([sdk]() {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;

        QJniEnvironment env;
        QStringList needed;
        if (sdk >= 33) needed.append(QStringLiteral("android.permission.POST_NOTIFICATIONS"));
        if (sdk >= 36) needed.append(QStringLiteral("android.permission.ACCESS_LOCAL_NETWORK"));

        QStringList missing;
        for (const QString &perm : needed) {
            const QJniObject permStr = QJniObject::fromString(perm);
            const jint granted = activity.callMethod<jint>(
                "checkSelfPermission", "(Ljava/lang/String;)I", permStr.object<jstring>());
            if (!env.checkAndClearExceptions() && granted != 0) {
                missing.append(perm);
            }
        }
        if (missing.isEmpty()) return;

        jclass stringClass = env->FindClass("java/lang/String");
        if (!stringClass) { env.checkAndClearExceptions(); return; }
        jobjectArray permArray = env->NewObjectArray(missing.size(), stringClass, nullptr);
        if (permArray) {
            for (int i = 0; i < missing.size(); ++i) {
                env->SetObjectArrayElement(permArray, i, QJniObject::fromString(missing.at(i)).object<jstring>());
            }
            activity.callMethod<void>("requestPermissions", "([Ljava/lang/String;I)V", permArray, jint(10455));
            env->DeleteLocalRef(permArray);
        }
        env->DeleteLocalRef(stringClass);
        env.checkAndClearExceptions();
    });
#endif
}

QString AndroidNetworkBridge::wifiIpAddress()
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject wifiManager = androidWifiManager();
    if (!wifiManager.isValid()) {
        return {};
    }

    const QJniObject wifiInfo = wifiManager.callObjectMethod(
        "getConnectionInfo", "()Landroid/net/wifi/WifiInfo;");
    if (!env.checkAndClearExceptions() && wifiInfo.isValid()) {
        const jint ipInt = wifiInfo.callMethod<jint>("getIpAddress", "()I");
        if (!env.checkAndClearExceptions() && ipInt != 0) {
            return ipv4FromLittleEndianInt(ipInt);
        }
    }

    const QJniObject dhcpInfo = wifiManager.callObjectMethod(
        "getDhcpInfo", "()Landroid/net/DhcpInfo;");
    if (!env.checkAndClearExceptions() && dhcpInfo.isValid()) {
        const jint ipInt = dhcpInfo.getField<jint>("ipAddress");
        if (!env.checkAndClearExceptions() && ipInt != 0) {
            return ipv4FromLittleEndianInt(ipInt);
        }
    }
    env.checkAndClearExceptions();
#endif
    return {};
}

QString AndroidNetworkBridge::wifiBroadcastAddress()
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject wifiManager = androidWifiManager();
    if (!wifiManager.isValid()) {
        return {};
    }
    const QJniObject dhcpInfo = wifiManager.callObjectMethod(
        "getDhcpInfo", "()Landroid/net/DhcpInfo;");
    if (!env.checkAndClearExceptions() && dhcpInfo.isValid()) {
        const jint ipInt = dhcpInfo.getField<jint>("ipAddress");
        const jint maskInt = dhcpInfo.getField<jint>("netmask");
        if (!env.checkAndClearExceptions() && ipInt != 0) {
            const quint32 ip = static_cast<quint32>(ipInt);
            const quint32 mask = (maskInt != 0) ? static_cast<quint32>(maskInt) : 0x00FFFFFFU;
            return ipv4FromLittleEndianInt(static_cast<jint>((ip & mask) | (~mask)));
        }
    }
    env.checkAndClearExceptions();
#endif
    return {};
}

QString AndroidNetworkBridge::deviceModelName()
{
#ifdef Q_OS_ANDROID
    const QJniObject model = QJniObject::getStaticObjectField<jstring>(
        "android/os/Build", "MODEL");
    if (model.isValid()) {
        return model.toString().trimmed();
    }
#endif
    return {};
}

} // namespace landrop
