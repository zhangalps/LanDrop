#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

namespace landrop {

class AndroidIntentBridge
{
public:
    static bool openWithViewIntent(const QString &path, const QString &mediaKind);
    static void launchFilePicker(std::function<void(const QList<QUrl> &)> onPicked);
};

} // namespace landrop
