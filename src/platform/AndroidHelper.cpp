#include "platform/AndroidHelper.h"
#include "platform/AndroidIntentBridge.h"
#include "platform/AndroidStorageBridge.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHostAddress>
#include <QMetaObject>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

namespace landrop {

namespace {

bool isPrivateLanIpv4(const QHostAddress &addr)
{
    bool ok = false;
    const quint32 ip = addr.toIPv4Address(&ok);
    if (!ok) return false;
    const quint8 a = static_cast<quint8>((ip >> 24) & 0xFFU);
    const quint8 b = static_cast<quint8>((ip >> 16) & 0xFFU);
    return (a == 192 && b == 168) || (a == 10) || (a == 172 && b >= 16 && b <= 31);
}

bool isVirtualInterface(const QNetworkInterface &iface)
{
    const QString name = (iface.humanReadableName() + QLatin1Char(' ') + iface.name()).toLower();
    static const QStringList kVirtualKeywords = {
        QStringLiteral("vethernet"), QStringLiteral("wsl"), QStringLiteral("hyper-v"),
        QStringLiteral("vmware"), QStringLiteral("virtualbox"), QStringLiteral("vbox"),
        QStringLiteral("docker"), QStringLiteral("virbr"), QStringLiteral("tun"),
        QStringLiteral("tap"), QStringLiteral("vpn"), QStringLiteral("bluetooth"),
        QStringLiteral("loopback"), QStringLiteral("npcap"), QStringLiteral("virtual")
    };
    for (const QString &kw : kVirtualKeywords) {
        if (name.contains(kw)) return true;
    }
    return false;
}

QString tryWritableLanDropDir(const QString &base)
{
    if (base.isEmpty()) return {};
    const QString target = QDir(base).filePath(QStringLiteral("LanDrop"));
    QDir dir(target);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) return {};

    QFile probe(dir.filePath(QStringLiteral(".landrop_write_probe")));
    if (probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        probe.close();
        probe.remove();
        return target;
    }
    return {};
}

} // namespace

AndroidHelper::AndroidHelper(QObject *parent)
    : QObject(parent)
{
}

AndroidHelper::~AndroidHelper()
{
    releaseMulticastLock();
}

void AndroidHelper::acquireMulticastLock() { m_networkBridge.acquireLocks(); }
void AndroidHelper::releaseMulticastLock() { m_networkBridge.releaseLocks(); }
void AndroidHelper::startKeepAliveService() { AndroidNetworkBridge::startKeepAliveService(); }
void AndroidHelper::stopKeepAliveService() { AndroidNetworkBridge::stopKeepAliveService(); }
void AndroidHelper::requestLocalNetworkPermission() { AndroidNetworkBridge::requestRuntimePermissions(); }

QString AndroidHelper::localDeviceName()
{
    const QString model = AndroidNetworkBridge::deviceModelName();
    if (!model.isEmpty()) return model;

    const QString host = QSysInfo::machineHostName().trimmed();
    if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0) {
        return host;
    }
    return QStringLiteral("LanDrop Device");
}

QString AndroidHelper::localIpAddress()
{
    const QString wifiIp = AndroidNetworkBridge::wifiIpAddress();
    if (!wifiIp.isEmpty()) return wifiIp;

    QString bestLanIp;
    QString virtualLanIp;
    QString fallbackIp;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const bool isVirt = isVirtualInterface(iface);
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol) continue;
            if (isPrivateLanIpv4(ip)) {
                if (!isVirt && bestLanIp.isEmpty()) bestLanIp = ip.toString();
                else if (isVirt && virtualLanIp.isEmpty()) virtualLanIp = ip.toString();
            } else if (!isVirt && fallbackIp.isEmpty()) {
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
    return AndroidNetworkBridge::wifiBroadcastAddress();
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
#ifdef Q_OS_ANDROID
    const QString appData = tryWritableLanDropDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!appData.isEmpty()) return appData;
#endif
    const QString preferred = tryWritableLanDropDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    if (!preferred.isEmpty()) return preferred;

    const QString fallback = tryWritableLanDropDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return fallback.isEmpty() ? QDir::tempPath() : fallback;
}

QString AndroidHelper::toOpenablePath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::FullyEncoded);
}

QString AndroidHelper::resolveFileDisplayName(const QUrl &url)
{
    if (url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).fileName();
    }
    const QString resolved = AndroidStorageBridge::resolveContentDisplayName(url);
    if (!resolved.isEmpty()) {
        return resolved;
    }
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
                return result;
            }
        }
    }

    if (url.scheme() == QLatin1String("content")) {
        const QString cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir cacheDir(QDir(cacheBase).filePath(QStringLiteral("landrop_send_tmp")));
        if (!cacheDir.exists()) cacheDir.mkpath(QStringLiteral("."));
        const QString ext = QFileInfo(result.displayName).suffix();
        const QString tmpName = QUuid::createUuid().toString(QUuid::WithoutBraces)
            + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
        const QString tmpPath = cacheDir.filePath(tmpName);

        if (AndroidStorageBridge::copyContentUriToFile(url.toString(QUrl::FullyEncoded), tmpPath)) {
            result.readablePath = tmpPath;
            result.fileSize = QFileInfo(tmpPath).size();
            result.isTemporary = true;
        }
    }
    return result;
}

QString AndroidHelper::exportToPublicStorage(const QString &localPath,
                                             const QString &fileName,
                                             const QString &mediaKind)
{
    return AndroidStorageBridge::exportToPublicStorage(localPath, fileName, mediaKind);
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
    AndroidIntentBridge::launchFilePicker([this](const QList<QUrl> &picked) {
        QMetaObject::invokeMethod(this, [this, picked]() {
            emit filesPicked(picked);
        }, Qt::QueuedConnection);
    });
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
    if (path.isEmpty()) return false;
#ifdef Q_OS_ANDROID
    return AndroidIntentBridge::openWithViewIntent(path, mediaKind);
#else
    Q_UNUSED(mediaKind)
    return path.startsWith(QLatin1String("content://"))
        ? QDesktopServices::openUrl(QUrl(path))
        : QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}

bool AndroidHelper::revealInFolder(const QString &path)
{
    if (path.isEmpty()) return false;
#ifdef Q_OS_WIN
    return QProcess::startDetached(
        QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

QString AndroidHelper::saveToPublicStorage(const QString &localPath,
                                           const QString &fileName,
                                           const QString &mediaKind)
{
    return exportToPublicStorage(localPath, fileName, mediaKind);
}

} // namespace landrop
