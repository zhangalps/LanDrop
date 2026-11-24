#include "platform/AndroidStorageBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace landrop {

namespace {

#ifdef Q_OS_ANDROID
int sdkInt()
{
    return QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
}

QJniObject parseAndroidUri(const QString &uriString)
{
    const QJniObject jUriStr = QJniObject::fromString(uriString);
    return QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        jUriStr.object<jstring>());
}

bool isContentUriAccessible(const QString &uriString)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return false;
    }
    QJniEnvironment env;
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    const QJniObject uri = parseAndroidUri(uriString);
    if (!resolver.isValid() || !uri.isValid()) {
        return false;
    }
    const QJniObject mode = QJniObject::fromString(QStringLiteral("r"));
    const QJniObject pfd = resolver.callObjectMethod(
        "openFileDescriptor",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;",
        uri.object<jobject>(), mode.object<jstring>());
    if (!env.checkAndClearExceptions() && pfd.isValid()) {
        pfd.callMethod<void>("close", "()V");
        env.checkAndClearExceptions();
        return true;
    }
    env.checkAndClearExceptions();
    return false;
}
#endif

} // namespace

QString AndroidStorageBridge::resolveContentDisplayName(const QUrl &url)
{
#ifdef Q_OS_ANDROID
    if (url.scheme() != QLatin1String("content")) {
        return {};
    }
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return {};
    }
    QJniEnvironment env;
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    const QJniObject uri = parseAndroidUri(url.toString(QUrl::FullyEncoded));
    if (!resolver.isValid() || !uri.isValid()) {
        return {};
    }

    const QJniObject cursor = resolver.callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        uri.object<jobject>(), nullptr, nullptr, nullptr, nullptr);
    env.checkAndClearExceptions();
    if (!cursor.isValid()) {
        return {};
    }

    QString resolved;
    if (cursor.callMethod<jboolean>("moveToFirst", "()Z")) {
        const QJniObject colName = QJniObject::fromString(QStringLiteral("_display_name"));
        const jint colIdx = cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", colName.object<jstring>());
        if (colIdx >= 0) {
            resolved = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", colIdx).toString().trimmed();
        }
    }
    cursor.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    return resolved;
#else
    Q_UNUSED(url)
    return {};
#endif
}

bool AndroidStorageBridge::copyContentUriToFile(const QString &uriString, const QString &destPath)
{
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return false;
    }
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    const QJniObject uri = parseAndroidUri(uriString);
    if (!resolver.isValid() || !uri.isValid()) {
        return false;
    }

    const QJniObject inputStream = resolver.callObjectMethod(
        "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;", uri.object<jobject>());
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
        const jint n = inputStream.callMethod<jint>("read", "([B)I", jBuffer);
        if (env.checkAndClearExceptions()) { ok = false; break; }
        if (n <= 0) break;
        env->GetByteArrayRegion(jBuffer, 0, n, reinterpret_cast<jbyte *>(localBuf.data()));
        if (outFile.write(localBuf.constData(), n) != n) { ok = false; break; }
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
#else
    Q_UNUSED(uriString)
    Q_UNUSED(destPath)
    return false;
#endif
}

QString AndroidStorageBridge::exportToPublicStorage(const QString &localPath,
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
            if ((cached.startsWith(QLatin1String("content://")) && isContentUriAccessible(cached))
                || (!cached.startsWith(QLatin1String("content://")) && QFile::exists(cached))) {
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
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return {};
    }

    const QString mimeType = QMimeDatabase().mimeTypeForFile(fileName, QMimeDatabase::MatchExtension).name();
    QJniObject collectionUri;
    QString relativePath;
    if (mediaKind == QLatin1String("image")) {
        collectionUri = QJniObject::getStaticObjectField("android/provider/MediaStore$Images$Media", "EXTERNAL_CONTENT_URI", "Landroid/net/Uri;");
        relativePath = QStringLiteral("Pictures/LanDrop");
    } else if (mediaKind == QLatin1String("video")) {
        collectionUri = QJniObject::getStaticObjectField("android/provider/MediaStore$Video$Media", "EXTERNAL_CONTENT_URI", "Landroid/net/Uri;");
        relativePath = QStringLiteral("Movies/LanDrop");
    } else if (sdkInt() >= 29) {
        collectionUri = QJniObject::getStaticObjectField("android/provider/MediaStore$Downloads", "EXTERNAL_CONTENT_URI", "Landroid/net/Uri;");
        relativePath = QStringLiteral("Download/LanDrop");
    } else {
        QDir dir(QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(QStringLiteral("LanDrop")));
        if (!dir.exists()) dir.mkpath(QStringLiteral("."));
        const QString destPath = dir.filePath(fileName);
        if (QFile::exists(destPath)) QFile::remove(destPath);
        if (!srcFile.copy(destPath)) return {};
        QMutexLocker locker(&s_exportMutex);
        s_exportedUris.insert(localPath, destPath);
        return destPath;
    }

    if (!collectionUri.isValid()) return {};

    QJniObject values("android/content/ContentValues");
    auto putStr = [&](const char *k, const QString &v) {
        values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/String;)V",
                                QJniObject::fromString(QString::fromLatin1(k)).object<jstring>(),
                                QJniObject::fromString(v).object<jstring>());
    };
    putStr("_display_name", fileName);
    putStr("mime_type", mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType);
    if (sdkInt() >= 29) {
        putStr("relative_path", relativePath);
        values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/Integer;)V",
                                QJniObject::fromString(QStringLiteral("is_pending")).object<jstring>(),
                                QJniObject("java/lang/Integer", "(I)V", jint(1)).object<jobject>());
    }

    const QJniObject itemUri = resolver.callObjectMethod(
        "insert", "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        collectionUri.object<jobject>(), values.object<jobject>());
    if (env.checkAndClearExceptions() || !itemUri.isValid()) return {};

    const QJniObject outStream = resolver.callObjectMethod(
        "openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;", itemUri.object<jobject>());
    if (env.checkAndClearExceptions() || !outStream.isValid()) return {};

    constexpr qint64 kBufSize = 64 * 1024;
    jbyteArray jBuffer = env->NewByteArray(static_cast<jsize>(kBufSize));
    QByteArray chunk(kBufSize, Qt::Uninitialized);
    bool copyOk = true;
    while (!srcFile.atEnd()) {
        const qint64 n = srcFile.read(chunk.data(), kBufSize);
        if (n < 0) { copyOk = false; break; }
        if (n == 0) break;
        env->SetByteArrayRegion(jBuffer, 0, static_cast<jsize>(n), reinterpret_cast<const jbyte *>(chunk.constData()));
        outStream.callMethod<void>("write", "([BII)V", jBuffer, jint(0), static_cast<jint>(n));
        if (env.checkAndClearExceptions()) { copyOk = false; break; }
    }
    env->DeleteLocalRef(jBuffer);
    outStream.callMethod<void>("flush", "()V");
    outStream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    srcFile.close();

    if (!copyOk) {
        resolver.callMethod<jint>("delete", "(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I",
                                  itemUri.object<jobject>(), nullptr, nullptr);
        env.checkAndClearExceptions();
        return {};
    }

    if (sdkInt() >= 29) {
        QJniObject doneVals("android/content/ContentValues");
        doneVals.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/Integer;)V",
                                  QJniObject::fromString(QStringLiteral("is_pending")).object<jstring>(),
                                  QJniObject("java/lang/Integer", "(I)V", jint(0)).object<jobject>());
        resolver.callMethod<jint>("update", "(Landroid/net/Uri;Landroid/content/ContentValues;Ljava/lang/String;[Ljava/lang/String;)I",
                                  itemUri.object<jobject>(), doneVals.object<jobject>(), nullptr, nullptr);
        env.checkAndClearExceptions();
    }

    const QString resultUri = itemUri.toString();
    QMutexLocker locker(&s_exportMutex);
    s_exportedUris.insert(localPath, resultUri);
    return resultUri;
#else
    Q_UNUSED(fileName)
    Q_UNUSED(mediaKind)
    return localPath;
#endif
}

} // namespace landrop
