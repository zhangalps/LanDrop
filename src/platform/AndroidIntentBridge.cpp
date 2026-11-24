#include "platform/AndroidIntentBridge.h"
#include "platform/AndroidStorageBridge.h"

#include <QFileInfo>
#include <QMimeDatabase>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>
#endif

namespace landrop {

namespace {

#ifdef Q_OS_ANDROID
bool launchViewIntentUri(const QString &contentUriString, const QString &mimeType)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return false;

    QJniEnvironment env;
    const QJniObject actionView = QJniObject::getStaticObjectField<jstring>("android/content/Intent", "ACTION_VIEW");
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", actionView.object<jstring>());

    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(contentUriString).object<jstring>());
    const QJniObject jMime = QJniObject::fromString(mimeType.isEmpty() ? QStringLiteral("*/*") : mimeType);

    intent.callObjectMethod("setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object<jobject>(), jMime.object<jstring>());

    const jint flagRead = QJniObject::getStaticField<jint>("android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    const jint flagNewTask = QJniObject::getStaticField<jint>("android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", flagRead | flagNewTask);

    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());
    return !env.checkAndClearExceptions();
}
#endif

} // namespace

bool AndroidIntentBridge::openWithViewIntent(const QString &path, const QString &mediaKind)
{
#ifdef Q_OS_ANDROID
    const QString mimeType = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    if (path.startsWith(QLatin1String("content://"))) {
        return launchViewIntentUri(path, mimeType);
    }
    const QString exportedUri = AndroidStorageBridge::exportToPublicStorage(path, QFileInfo(path).fileName(), mediaKind);
    if (exportedUri.startsWith(QLatin1String("content://"))) {
        return launchViewIntentUri(exportedUri, mimeType);
    }
    return false;
#else
    Q_UNUSED(path)
    Q_UNUSED(mediaKind)
    return false;
#endif
}

void AndroidIntentBridge::launchFilePicker(std::function<void(const QList<QUrl> &)> onPicked)
{
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    const QJniObject actionOpen = QJniObject::getStaticObjectField<jstring>("android/content/Intent", "ACTION_OPEN_DOCUMENT");
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", actionOpen.object<jstring>());

    intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::getStaticObjectField<jstring>("android/content/Intent", "CATEGORY_OPENABLE").object<jstring>());
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::fromString(QStringLiteral("*/*")).object<jstring>());
    intent.callObjectMethod("putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;",
                            QJniObject::getStaticObjectField<jstring>("android/content/Intent", "EXTRA_ALLOW_MULTIPLE").object<jstring>(),
                            jboolean(true));
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;",
                            QJniObject::getStaticField<jint>("android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION"));
    if (env.checkAndClearExceptions()) return;

    constexpr int kPickCode = 10454;
    QtAndroidPrivate::startActivity(intent, kPickCode, [cb = std::move(onPicked)](int req, int res, const QJniObject &data) {
        if (req != kPickCode || res != -1 || !data.isValid()) return;
        QJniEnvironment cbEnv;
        QList<QUrl> picked;
        const QJniObject clipData = data.callObjectMethod("getClipData", "()Landroid/content/ClipData;");
        if (!cbEnv.checkAndClearExceptions() && clipData.isValid()) {
            const jint count = clipData.callMethod<jint>("getItemCount", "()I");
            for (jint i = 0; i < count; ++i) {
                const QJniObject item = clipData.callObjectMethod("getItemAt", "(I)Landroid/content/ClipData$Item;", i);
                if (item.isValid()) {
                    const QString uriStr = item.callObjectMethod("getUri", "()Landroid/net/Uri;").toString();
                    if (!uriStr.isEmpty()) picked.append(QUrl::fromEncoded(uriStr.toUtf8()));
                }
            }
        } else {
            const QJniObject singleUri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
            if (!cbEnv.checkAndClearExceptions() && singleUri.isValid()) {
                const QString uriStr = singleUri.toString();
                if (!uriStr.isEmpty()) picked.append(QUrl::fromEncoded(uriStr.toUtf8()));
            }
        }
        cbEnv.checkAndClearExceptions();
        if (!picked.isEmpty() && cb) cb(picked);
    });
#else
    Q_UNUSED(onPicked)
#endif
}

} // namespace landrop
