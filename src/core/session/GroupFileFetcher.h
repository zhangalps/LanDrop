#pragma once

#include "core/model/MessageListModel.h"

#include <QObject>
#include <QString>
#include <QTcpSocket>

namespace landrop {

class GroupFileFetcher
{
public:
    static void fetchFile(QObject *owner,
                          MessageListModel *messages,
                          const QString &requesterId,
                          const QString &downloadDir,
                          const QString &transferId);

    static void serveFile(QTcpSocket *socket,
                          const QString &transferId,
                          const QString &filePath,
                          const QString &fileName);
};

} // namespace landrop
