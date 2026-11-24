#pragma once

#include <QString>
#include <QUrl>

namespace landrop {

class AndroidStorageBridge
{
public:
    static QString resolveContentDisplayName(const QUrl &url);
    static bool copyContentUriToFile(const QString &uriString, const QString &destPath);
    static QString exportToPublicStorage(const QString &localPath,
                                         const QString &fileName,
                                         const QString &mediaKind);
};

} // namespace landrop
