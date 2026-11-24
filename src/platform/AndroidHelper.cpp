#include "platform/AndroidHelper.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QHostAddress>
#include <QMimeDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QtCore/private/qandroidextras_p.h>
#endif

namespace landrop {

namespace {

#ifdef Q_OS_ANDROID
int androidSdkInt()
{
    return QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
}

bool copyAndroidContentUriToFile(const QString &uriString, const QString &destPath)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return false;
    }

    const QJniObject resolver = context.callObjectMethod(
        "getContentResolver",
        "()Landroid/content/ContentResolver;");
    const QJniObject jUriStr = QJniObject::fromString(uriString);
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri",
        "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        jUriStr.object<jstring>());

    if (!resolver.isValid() || !uri.isValid()) {
        return false;
    }

    const QJniObject inputStream = resolver.callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        uri.object<jobject>());
    if (!inputStream.isValid()) {
        return false;
    }

    QFile outFile(destPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        inputStream.callMethod<void>("close", "()V");
        return false;
    }

    QJniEnvironment env;
    constexpr jsize kBufSize = 64 * 1024;
    jbyteArray jBuffer = env->NewByteArray(kBufSize);
    if (!jBuffer) {
        inputStream.callMethod<void>("close", "()V");
        outFile.close();
        outFile.remove();
        return false;
    }

    bool ok = true;
    QByteArray localBuf(kBufSize, Qt::Uninitialized);
    while (true) {
        const jint readCount = inputStream.callMethod<jint>("read", "([B)I", jBuffer);
        if (env.checkAndClearExceptions()) {
            ok = false;
            break;
        }
        if (readCount <= 0) {
            break;
        }
        env->GetByteArrayRegion(jBuffer, 0, readCount, reinterpret_cast<jbyte *>(localBuf.data()));
        if (outFile.write(localBuf.constData(), readCount) != readCount) {
            ok = false;
            break;
        }
    }

    env->DeleteLocalRef(jBuffer);
    inputStream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    outFile.flush();
    outFile.close();

    if (!ok) {
        outFile.remove();
    }
    return ok;
}

bool launchAndroidViewIntent(const QString &contentUriString, const QString &mimeType)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return false;
    }

    QJniEnvironment env;
    const QJniObject actionView = QJniObject::getStaticObjectField<jstring>(
        "android/content/Intent", "ACTION_VIEW");
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", actionView.object<jstring>());

    const QJniObject jUriStr = QJniObject::fromString(contentUriString);
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri",
        "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        jUriStr.object<jstring>());
    const QJniObject jMime = QJniObject::fromString(mimeType.isEmpty() ? QStringLiteral("*/*") : mimeType);

    intent.callObjectMethod(
        "setDataAndType",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
        uri.object<jobject>(),
        jMime.object<jstring>());

    const jint flagRead = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    const jint flagNewTask = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
    intent.callObjectMethod(
        "addFlags",
        "(I)Landroid/content/Intent;",
        flagRead | flagNewTask);

    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());
    if (env.checkAndClearExceptions()) {
        return false;
    }
    return true;
}
#endif

} // namespace

AndroidHelper::AndroidHelper(QObject *parent)
    : QObject(parent)
{
}

AndroidHelper::~AndroidHelper()
{
    releaseMulticastLock();
}

void AndroidHelper::acquireMulticastLock()
{
#ifdef Q_OS_ANDROID
    if (m_multicastLock.isValid()) {
        return;
    }

    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return;
    }

    const QJniObject wifiServiceStr = QJniObject::getStaticObjectField<jstring>(
        "android/content/Context", "WIFI_SERVICE");
    if (!wifiServiceStr.isValid()) {
        return;
    }

    const QJniObject wifiManager = context.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        wifiServiceStr.object<jstring>());
    if (!wifiManager.isValid()) {
        return;
    }

    const QJniObject lockTag = QJniObject::fromString(QStringLiteral("LanDropMulticastLock"));
    m_multicastLock = wifiManager.callObjectMethod(
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
        lockTag.object<jstring>());

    if (m_multicastLock.isValid()) {
        m_multicastLock.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
        m_multicastLock.callMethod<void>("acquire", "()V");
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

void AndroidHelper::releaseMulticastLock()
{
#ifdef Q_OS_ANDROID
    if (m_multicastLock.isValid()) {
        const jboolean held = m_multicastLock.callMethod<jboolean>("isHeld", "()Z");
        if (held) {
            m_multicastLock.callMethod<void>("release", "()V");
        }
        m_multicastLock = QJniObject();
    }
    if (m_wifiLock.isValid()) {
        const jboolean held = m_wifiLock.callMethod<jboolean>("isHeld", "()Z");
        if (held) {
            m_wifiLock.callMethod<void>("release", "()V");
        }
        m_wifiLock = QJniObject();
    }
#endif
}

void AndroidHelper::requestLocalNetworkPermission()
{
#ifdef Q_OS_ANDROID
    if (androidSdkInt() < 36) {
        return;
    }

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            return;
        }

        QJniEnvironment env;
        const QJniObject permStr = QJniObject::fromString(
            QStringLiteral("android.permission.ACCESS_LOCAL_NETWORK"));
        const jint granted = activity.callMethod<jint>(
            "checkSelfPermission",
            "(Ljava/lang/String;)I",
            permStr.object<jstring>());
        if (env.checkAndClearExceptions() || granted == 0) {
            return;
        }

        jclass stringClass = env->FindClass("java/lang/String");
        if (!stringClass) {
            env.checkAndClearExceptions();
            return;
        }
        jobjectArray permArray = env->NewObjectArray(1, stringClass, permStr.object<jstring>());
        if (permArray) {
            activity.callMethod<void>(
                "requestPermissions",
                "([Ljava/lang/String;I)V",
                permArray,
                jint(10455));
            env->DeleteLocalRef(permArray);
        }
        env->DeleteLocalRef(stringClass);
        env.checkAndClearExceptions();
    });
#endif
}

QString AndroidHelper::localDeviceName()
{
#ifdef Q_OS_ANDROID
    const QJniObject model = QJniObject::getStaticObjectField<jstring>(
        "android/os/Build", "MODEL");
    if (model.isValid()) {
        const QString modelStr = model.toString().trimmed();
        if (!modelStr.isEmpty()) {
            return modelStr;
        }
    }
#endif
    const QString host = QSysInfo::machineHostName().trimmed();
    if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0) {
        return host;
    }
    return QStringLiteral("LanDrop Device");
}

namespace {

#ifdef Q_OS_ANDROID
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

bool isPrivateLanIpv4(const QHostAddress &addr)
{
    bool ok = false;
    const quint32 ip = addr.toIPv4Address(&ok);
    if (!ok) {
        return false;
    }
    const quint8 a = static_cast<quint8>((ip >> 24) & 0xFFU);
    const quint8 b = static_cast<quint8>((ip >> 16) & 0xFFU);
    if (a == 192 && b == 168) return true;
    if (a == 10) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    return false;
}

} // namespace

QString AndroidHelper::localIpAddress()
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject wifiManager = androidWifiManager();
    if (wifiManager.isValid()) {
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
    }
    env.checkAndClearExceptions();
#endif

    QString bestLanIp;
    QString virtualLanIp;
    QString fallbackIp;

    auto isVirtualInterface = [](const QNetworkInterface &iface) -> bool {
        const QString name = (iface.humanReadableName() + QLatin1Char(' ') + iface.name()).toLower();
        static const QStringList kVirtualKeywords = {
            QStringLiteral("vethernet"), QStringLiteral("wsl"), QStringLiteral("hyper-v"),
            QStringLiteral("vmware"), QStringLiteral("virtualbox"), QStringLiteral("vbox"),
            QStringLiteral("docker"), QStringLiteral("virbr"), QStringLiteral("tun"),
            QStringLiteral("tap"), QStringLiteral("vpn"), QStringLiteral("bluetooth"),
            QStringLiteral("loopback"), QStringLiteral("npcap"), QStringLiteral("virtual")
        };
        for (const QString &kw : kVirtualKeywords) {
            if (name.contains(kw)) {
                return true;
            }
        }
        return false;
    };

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const bool isVirt = isVirtualInterface(iface);
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            if (isPrivateLanIpv4(ip)) {
                if (!isVirt) {
                    if (bestLanIp.isEmpty()) {
                        bestLanIp = ip.toString();
                    }
                } else if (virtualLanIp.isEmpty()) {
                    virtualLanIp = ip.toString();
                }
            } else if (fallbackIp.isEmpty() && !isVirt) {
                fallbackIp = ip.toString();
            }
        }
    }
    if (!bestLanIp.isEmpty()) return bestLanIp;
    if (!virtualLanIp.isEmpty()) return virtualLanIp;
    return fallbackIp;
}

QString AndroidHelper::androidWifiBroadcastAddress()
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject wifiManager = androidWifiManager();
    if (wifiManager.isValid()) {
        const QJniObject dhcpInfo = wifiManager.callObjectMethod(
            "getDhcpInfo", "()Landroid/net/DhcpInfo;");
        if (!env.checkAndClearExceptions() && dhcpInfo.isValid()) {
            const jint ipInt = dhcpInfo.getField<jint>("ipAddress");
            const jint maskInt = dhcpInfo.getField<jint>("netmask");
            if (!env.checkAndClearExceptions() && ipInt != 0) {
                // If netmask is 0 on some Android versions, assume /24 (0x00FFFFFF in little-endian)
                const quint32 ip = static_cast<quint32>(ipInt);
                const quint32 mask = (maskInt != 0) ? static_cast<quint32>(maskInt) : 0x00FFFFFFU;
                const quint32 broadcast = (ip & mask) | (~mask);
                return ipv4FromLittleEndianInt(static_cast<jint>(broadcast));
            }
        }
    }
    env.checkAndClearExceptions();
#endif
    return {};
}

QString AndroidHelper::platformName()
{
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString AndroidHelper::downloadDirectory()
{
    auto tryDir = [](const QString &base) -> QString {
        if (base.isEmpty()) {
            return {};
        }
        const QString target = QDir(base).filePath(QStringLiteral("LanDrop"));
        QDir dir(target);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            return {};
        }
        const QString probePath = dir.filePath(QStringLiteral(".landrop_write_probe"));
        QFile probe(probePath);
        if (probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            probe.close();
            probe.remove();
            return target;
        }
        return {};
    };

#ifdef Q_OS_ANDROID
    // On Android 10+, always stream into writable app-private storage first,
    // then export cleanly to public Download/Pictures/Movies via MediaStore on completion.
    const QString appData = tryDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!appData.isEmpty()) {
        return appData;
    }
#endif

    const QString preferred = tryDir(
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    if (!preferred.isEmpty()) {
        return preferred;
    }

    const QString fallback = tryDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!fallback.isEmpty()) {
        return fallback;
    }

    return QDir::tempPath();
}

QString AndroidHelper::toOpenablePath(const QUrl &url)
{
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    // Preserve percent-encoding for Android content:// URIs (e.g. %3A).
    return url.toString(QUrl::FullyEncoded);
}

QString AndroidHelper::resolveFileDisplayName(const QUrl &url)
{
    if (url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).fileName();
    }

#ifdef Q_OS_ANDROID
    if (url.scheme() == QLatin1String("content")) {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (context.isValid()) {
            QJniEnvironment env;
            const QJniObject resolver = context.callObjectMethod(
                "getContentResolver",
                "()Landroid/content/ContentResolver;");
            const QJniObject uriStr = QJniObject::fromString(url.toString(QUrl::FullyEncoded));
            const QJniObject uri = QJniObject::callStaticObjectMethod(
                "android/net/Uri",
                "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;",
                uriStr.object<jstring>());

            if (resolver.isValid() && uri.isValid()) {
                const QJniObject cursor = resolver.callObjectMethod(
                    "query",
                    "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
                    uri.object<jobject>(),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr);
                env.checkAndClearExceptions();

                if (cursor.isValid()) {
                    const jboolean moved = cursor.callMethod<jboolean>("moveToFirst", "()Z");
                    if (moved) {
                        const QJniObject colName = QJniObject::fromString(QStringLiteral("_display_name"));
                        const jint colIdx = cursor.callMethod<jint>(
                            "getColumnIndex",
                            "(Ljava/lang/String;)I",
                            colName.object<jstring>());
                        if (colIdx >= 0) {
                            const QJniObject nameObj = cursor.callObjectMethod(
                                "getString",
                                "(I)Ljava/lang/String;",
                                colIdx);
                            const QString resolved = nameObj.toString().trimmed();
                            cursor.callMethod<void>("close", "()V");
                            env.checkAndClearExceptions();
                            if (!resolved.isEmpty()) {
                                return resolved;
                            }
                        }
                    }
                    cursor.callMethod<void>("close", "()V");
                    env.checkAndClearExceptions();
                }
            }
        }
    }
#endif

    const QString fallback = QFileInfo(url.path()).fileName();
    return fallback.isEmpty() ? QStringLiteral("file") : fallback;
}

PreparedFile AndroidHelper::prepareReadableFile(const QUrl &url)
{
    PreparedFile result;
    result.displayName = resolveFileDisplayName(url);

    const QString directPath = toOpenablePath(url);
    {
        QFile directFile(directPath);
        if (directFile.open(QIODevice::ReadOnly)) {
            const qint64 sz = directFile.size();
            directFile.close();
            if (sz > 0 || url.isLocalFile()) {
                result.readablePath = directPath;
                result.fileSize = sz;
                result.isTemporary = false;
                return result;
            }
        }
    }

#ifdef Q_OS_ANDROID
    // Fallback for Android content:// URIs where QFile fails or reports 0 size:
    // stream 64KB chunks via ContentResolver.openInputStream into an app cache file.
    if (url.scheme() == QLatin1String("content")) {
        const QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir cacheDir(QDir(cacheBase).filePath(QStringLiteral("landrop_send_tmp")));
        if (!cacheDir.exists()) {
            cacheDir.mkpath(QStringLiteral("."));
        }
        const QString ext = QFileInfo(result.displayName).suffix();
        const QString tmpName = QUuid::createUuid().toString(QUuid::WithoutBraces)
            + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
        const QString tmpPath = cacheDir.filePath(tmpName);

        if (copyAndroidContentUriToFile(url.toString(QUrl::FullyEncoded), tmpPath)) {
            const QFileInfo info(tmpPath);
            result.readablePath = tmpPath;
            result.fileSize = info.size();
            result.isTemporary = true;
            return result;
        }
    }
#endif

    return result;
}

QString AndroidHelper::exportToPublicStorage(const QString &localPath,
                                             const QString &fileName,
                                             const QString &mediaKind)
{
#ifdef Q_OS_ANDROID
    static QMutex s_exportMutex;
    static QHash<QString, QString> s_exportedUris;

    if (localPath.startsWith(QLatin1String("content://"))) {
        return localPath;
    }

    {
        QMutexLocker locker(&s_exportMutex);
        const QString cached = s_exportedUris.value(localPath);
        if (!cached.isEmpty()) {
            if (cached.startsWith(QLatin1String("content://"))) {
                // Verify the cached MediaStore URI is still valid
                const QJniObject context = QNativeInterface::QAndroidApplication::context();
                if (context.isValid()) {
                    QJniEnvironment env;
                    const QJniObject resolver = context.callObjectMethod(
                        "getContentResolver", "()Landroid/content/ContentResolver;");
                    const QJniObject jUriStr = QJniObject::fromString(cached);
                    const QJniObject uri = QJniObject::callStaticObjectMethod(
                        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
                        jUriStr.object<jstring>());
                    if (resolver.isValid() && uri.isValid()) {
                        const QJniObject mode = QJniObject::fromString(QStringLiteral("r"));
                        const QJniObject pfd = resolver.callObjectMethod(
                            "openFileDescriptor",
                            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;",
                            uri.object<jobject>(), mode.object<jstring>());
                        if (!env.checkAndClearExceptions() && pfd.isValid()) {
                            pfd.callMethod<void>("close", "()V");
                            env.checkAndClearExceptions();
                            return cached;
                        }
                    }
                    env.checkAndClearExceptions();
                }
            } else if (QFile::exists(cached)) {
                return cached;
            }
            s_exportedUris.remove(localPath);
        }
    }

    QFile srcFile(localPath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return {};
    }

    QJniEnvironment env;
    const QJniObject resolver = context.callObjectMethod(
        "getContentResolver",
        "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return {};
    }

    const QMimeDatabase mimeDb;
    const QString mimeType = mimeDb.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension).name();

    QJniObject collectionUri;
    QString relativePath;
    if (mediaKind == QLatin1String("image")) {
        collectionUri = QJniObject::getStaticObjectField(
            "android/provider/MediaStore$Images$Media",
            "EXTERNAL_CONTENT_URI",
            "Landroid/net/Uri;");
        relativePath = QStringLiteral("Pictures/LanDrop");
    } else if (mediaKind == QLatin1String("video")) {
        collectionUri = QJniObject::getStaticObjectField(
            "android/provider/MediaStore$Video$Media",
            "EXTERNAL_CONTENT_URI",
            "Landroid/net/Uri;");
        relativePath = QStringLiteral("Movies/LanDrop");
    } else if (androidSdkInt() >= 29) {
        collectionUri = QJniObject::getStaticObjectField(
            "android/provider/MediaStore$Downloads",
            "EXTERNAL_CONTENT_URI",
            "Landroid/net/Uri;");
        relativePath = QStringLiteral("Download/LanDrop");
    } else {
        // Android 9 and below fallback: copy directly into Download/LanDrop
        const QString dlBase = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QDir dir(QDir(dlBase).filePath(QStringLiteral("LanDrop")));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        const QString destPath = dir.filePath(fileName);
        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }
        if (srcFile.copy(destPath)) {
            return destPath;
        }
        return {};
    }

    if (!collectionUri.isValid()) {
        return {};
    }

    QJniObject values("android/content/ContentValues");
    auto putString = [&](const char *key, const QString &val) {
        const QJniObject jKey = QJniObject::fromString(QString::fromLatin1(key));
        const QJniObject jVal = QJniObject::fromString(val);
        values.callMethod<void>(
            "put",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            jKey.object<jstring>(),
            jVal.object<jstring>());
    };

    putString("_display_name", fileName);
    putString("mime_type", mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType);
    if (androidSdkInt() >= 29) {
        putString("relative_path", relativePath);
        const QJniObject jPendingKey = QJniObject::fromString(QStringLiteral("is_pending"));
        const QJniObject jOne("java/lang/Integer", "(I)V", jint(1));
        values.callMethod<void>(
            "put",
            "(Ljava/lang/String;Ljava/lang/Integer;)V",
            jPendingKey.object<jstring>(),
            jOne.object<jobject>());
    }

    const QJniObject itemUri = resolver.callObjectMethod(
        "insert",
        "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        collectionUri.object<jobject>(),
        values.object<jobject>());
    if (env.checkAndClearExceptions() || !itemUri.isValid()) {
        return {};
    }

    const QJniObject outputStream = resolver.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;)Ljava/io/OutputStream;",
        itemUri.object<jobject>());
    if (env.checkAndClearExceptions() || !outputStream.isValid()) {
        return {};
    }

    constexpr qint64 kBufSize = 64 * 1024;
    jbyteArray jBuffer = env->NewByteArray(static_cast<jsize>(kBufSize));
    QByteArray chunk(kBufSize, Qt::Uninitialized);
    bool copyOk = true;
    while (!srcFile.atEnd()) {
        const qint64 n = srcFile.read(chunk.data(), kBufSize);
        if (n < 0) {
            copyOk = false;
            break;
        }
        if (n == 0) {
            break;
        }
        env->SetByteArrayRegion(jBuffer, 0, static_cast<jsize>(n), reinterpret_cast<const jbyte *>(chunk.constData()));
        outputStream.callMethod<void>("write", "([BII)V", jBuffer, jint(0), static_cast<jint>(n));
        if (env.checkAndClearExceptions()) {
            copyOk = false;
            break;
        }
    }
    env->DeleteLocalRef(jBuffer);
    outputStream.callMethod<void>("flush", "()V");
    outputStream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    srcFile.close();

    if (!copyOk) {
        resolver.callMethod<jint>(
            "delete",
            "(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I",
            itemUri.object<jobject>(), nullptr, nullptr);
        env.checkAndClearExceptions();
        return {};
    }

    if (androidSdkInt() >= 29) {
        QJniObject doneValues("android/content/ContentValues");
        const QJniObject jPendingKey = QJniObject::fromString(QStringLiteral("is_pending"));
        const QJniObject jZero("java/lang/Integer", "(I)V", jint(0));
        doneValues.callMethod<void>(
            "put",
            "(Ljava/lang/String;Ljava/lang/Integer;)V",
            jPendingKey.object<jstring>(),
            jZero.object<jobject>());
        resolver.callMethod<jint>(
            "update",
            "(Landroid/net/Uri;Landroid/content/ContentValues;Ljava/lang/String;[Ljava/lang/String;)I",
            itemUri.object<jobject>(),
            doneValues.object<jobject>(),
            nullptr,
            nullptr);
        env.checkAndClearExceptions();
    }

    const QString resultUri = itemUri.toString();
    {
        QMutexLocker locker(&s_exportMutex);
        s_exportedUris.insert(localPath, resultUri);
    }
    return resultUri;
#else
    Q_UNUSED(fileName)
    Q_UNUSED(mediaKind)
    return localPath;
#endif
}

bool AndroidHelper::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool AndroidHelper::isAndroid()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void AndroidHelper::pickFiles()
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject actionOpen = QJniObject::getStaticObjectField<jstring>(
        "android/content/Intent", "ACTION_OPEN_DOCUMENT");
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", actionOpen.object<jstring>());

    const QJniObject categoryOpenable = QJniObject::getStaticObjectField<jstring>(
        "android/content/Intent", "CATEGORY_OPENABLE");
    intent.callObjectMethod(
        "addCategory",
        "(Ljava/lang/String;)Landroid/content/Intent;",
        categoryOpenable.object<jstring>());

    const QJniObject allTypes = QJniObject::fromString(QStringLiteral("*/*"));
    intent.callObjectMethod(
        "setType",
        "(Ljava/lang/String;)Landroid/content/Intent;",
        allTypes.object<jstring>());

    const QJniObject extraMulti = QJniObject::getStaticObjectField<jstring>(
        "android/content/Intent", "EXTRA_ALLOW_MULTIPLE");
    intent.callObjectMethod(
        "putExtra",
        "(Ljava/lang/String;Z)Landroid/content/Intent;",
        extraMulti.object<jstring>(),
        jboolean(true));

    const jint flagRead = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    intent.callObjectMethod(
        "addFlags",
        "(I)Landroid/content/Intent;",
        flagRead);

    if (env.checkAndClearExceptions()) {
        return;
    }

    constexpr int kPickFilesRequestCode = 10454;
    QtAndroidPrivate::startActivity(
        intent,
        kPickFilesRequestCode,
        [this](int requestCode, int resultCode, const QJniObject &data) {
            constexpr int kResultOk = -1; // Activity.RESULT_OK
            if (requestCode != kPickFilesRequestCode || resultCode != kResultOk || !data.isValid()) {
                return;
            }

            QJniEnvironment cbEnv;
            QList<QUrl> picked;

            const QJniObject clipData = data.callObjectMethod(
                "getClipData", "()Landroid/content/ClipData;");
            if (!cbEnv.checkAndClearExceptions() && clipData.isValid()) {
                const jint count = clipData.callMethod<jint>("getItemCount", "()I");
                for (jint i = 0; i < count; ++i) {
                    const QJniObject item = clipData.callObjectMethod(
                        "getItemAt", "(I)Landroid/content/ClipData$Item;", i);
                    if (item.isValid()) {
                        const QJniObject uri = item.callObjectMethod("getUri", "()Landroid/net/Uri;");
                        if (uri.isValid()) {
                            const QString uriStr = uri.toString();
                            if (!uriStr.isEmpty()) {
                                picked.append(QUrl::fromEncoded(uriStr.toUtf8()));
                            }
                        }
                    }
                }
            } else {
                const QJniObject singleUri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
                if (!cbEnv.checkAndClearExceptions() && singleUri.isValid()) {
                    const QString uriStr = singleUri.toString();
                    if (!uriStr.isEmpty()) {
                        picked.append(QUrl::fromEncoded(uriStr.toUtf8()));
                    }
                }
            }
            cbEnv.checkAndClearExceptions();

            if (!picked.isEmpty()) {
                QMetaObject::invokeMethod(this, [this, picked]() {
                    emit filesPicked(picked);
                }, Qt::QueuedConnection);
            }
        });
#endif
}

void AndroidHelper::copyToClipboard(const QString &text)
{
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(text);
    }
}

QString AndroidHelper::clipboardText()
{
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return {};
}

bool AndroidHelper::openFile(const QString &path, const QString &mediaKind)
{
    if (path.isEmpty()) {
        return false;
    }

#ifdef Q_OS_ANDROID
    const QMimeDatabase mimeDb;
    const QString mimeType = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();

    if (path.startsWith(QLatin1String("content://"))) {
        return launchAndroidViewIntent(path, mimeType);
    }

    // Export local private file to MediaStore to obtain a safe content:// URI for ACTION_VIEW
    const QString exportedUri = exportToPublicStorage(path, QFileInfo(path).fileName(), mediaKind);
    if (exportedUri.startsWith(QLatin1String("content://"))) {
        return launchAndroidViewIntent(exportedUri, mimeType);
    }
    return false;
#else
    Q_UNUSED(mediaKind)
    if (path.startsWith(QLatin1String("content://"))) {
        return QDesktopServices::openUrl(QUrl(path));
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}

bool AndroidHelper::revealInFolder(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
#ifdef Q_OS_WIN
    const QString nativePath = QDir::toNativeSeparators(path);
    return QProcess::startDetached(
        QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), nativePath});
#else
    const QFileInfo info(path);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
#endif
}

QString AndroidHelper::saveToPublicStorage(const QString &localPath,
                                           const QString &fileName,
                                           const QString &mediaKind)
{
    return exportToPublicStorage(localPath, fileName, mediaKind);
}

} // namespace landrop
