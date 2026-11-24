#pragma once

#include "platform/AndroidNetworkBridge.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

namespace landrop {

struct PreparedFile {
    QString readablePath;
    QString displayName;
    qint64 fileSize = -1;
    bool isTemporary = false;
};

class AndroidHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isWindows READ isWindows CONSTANT)
    Q_PROPERTY(bool isAndroid READ isAndroid CONSTANT)

public:
    explicit AndroidHelper(QObject *parent = nullptr);
    ~AndroidHelper() override;

    void acquireMulticastLock();
    void releaseMulticastLock();
    void startKeepAliveService();
    void stopKeepAliveService();
    void requestLocalNetworkPermission();

    static QString localDeviceName();
    static QString localIpAddress();
    static QString androidWifiBroadcastAddress();
    static QString platformName();
    static QString downloadDirectory();
    static QString toOpenablePath(const QUrl &url);
    static QString resolveFileDisplayName(const QUrl &url);
    static PreparedFile prepareReadableFile(const QUrl &url);
    static QString exportToPublicStorage(const QString &localPath,
                                         const QString &fileName,
                                         const QString &mediaKind);

    static bool isWindows();
    static bool isAndroid();

    Q_INVOKABLE void pickFiles();
    Q_INVOKABLE static void copyToClipboard(const QString &text);
    Q_INVOKABLE static QString clipboardText();
    Q_INVOKABLE static bool openFile(const QString &path, const QString &mediaKind = QStringLiteral("file"));
    Q_INVOKABLE static bool revealInFolder(const QString &path);
    Q_INVOKABLE static QString saveToPublicStorage(const QString &localPath,
                                                   const QString &fileName,
                                                   const QString &mediaKind);

signals:
    void filesPicked(const QList<QUrl> &urls);

private:
    AndroidNetworkBridge m_networkBridge;
};

} // namespace landrop
