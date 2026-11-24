#include "core/model/MessageListModel.h"
#include "core/Protocol.h"

#include <QDateTime>
#include <QUrl>

namespace landrop {

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_messages.size());
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return {};
    }

    const ChatMessage &m = m_messages.at(index.row());
    switch (role) {
    case MsgIdRole:            return m.msgId;
    case IsOutgoingRole:       return m.isOutgoing;
    case KindRole:             return m.kind;
    case TextRole:             return m.text;
    case SenderIdRole:         return m.senderId;
    case SenderNameRole:       return m.senderName;
    case SenderIpRole:         return m.senderIp;
    case SenderPortRole:       return m.senderPort;
    case FileNameRole:         return m.fileName;
    case FilePathRole:         return m.filePath;
    case FileUrlRole:          return m.fileUrl;
    case FileExtensionRole:    return m.fileExtension;
    case FileSizeRole:         return m.fileSize;
    case FileSizeTextRole:     return formatFileSize(m.fileSize);
    case BytesTransferredRole: return m.bytesTransferred;
    case ProgressRole:         return m.progress;
    case StatusRole:           return m.status;
    case TimestampRole:        return m.timestampText;
    default:                   return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {MsgIdRole,            "msgId"},
        {IsOutgoingRole,       "isOutgoing"},
        {KindRole,             "kind"},
        {TextRole,             "text"},
        {SenderIdRole,         "senderId"},
        {SenderNameRole,       "senderName"},
        {SenderIpRole,         "senderIp"},
        {SenderPortRole,       "senderPort"},
        {FileNameRole,         "fileName"},
        {FilePathRole,         "filePath"},
        {FileUrlRole,          "fileUrl"},
        {FileExtensionRole,    "fileExtension"},
        {FileSizeRole,         "fileSize"},
        {FileSizeTextRole,     "fileSizeText"},
        {BytesTransferredRole, "bytesTransferred"},
        {ProgressRole,         "progress"},
        {StatusRole,           "status"},
        {TimestampRole,        "timestamp"}
    };
}

void MessageListModel::appendMessage(ChatMessage msg)
{
    const int row = static_cast<int>(m_messages.size());
    beginInsertRows(QModelIndex(), row, row);
    m_messages.append(std::move(msg));
    endInsertRows();
    emit countChanged();
}

void MessageListModel::updateTransferProgress(const QString &msgId, qint64 bytesTransferred, bool forceNotify)
{
    const int idx = indexOfMsg(msgId);
    if (idx < 0) return;

    ChatMessage &m = m_messages[idx];
    m.bytesTransferred = bytesTransferred;
    const double nextProgress = (m.fileSize > 0)
        ? qBound(0.0, static_cast<double>(bytesTransferred) / static_cast<double>(m.fileSize), 1.0)
        : 1.0;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastMs = m_lastNotifyMs.value(msgId, 0);
    if (!forceNotify && (nowMs - lastMs < 50) && (nextProgress - m.progress < 0.01)) {
        return;
    }

    m.progress = nextProgress;
    m_lastNotifyMs.insert(msgId, nowMs);
    const QModelIndex modelIdx = index(idx);
    emit dataChanged(modelIdx, modelIdx, {BytesTransferredRole, ProgressRole});
}

void MessageListModel::updateTransferStatus(const QString &msgId, const QString &status, const QString &finalPath)
{
    const int idx = indexOfMsg(msgId);
    if (idx < 0) return;

    ChatMessage &m = m_messages[idx];
    m.status = status;
    if (status == QLatin1String("completed")) {
        m.progress = 1.0;
        m.bytesTransferred = m.fileSize;
    }
    if (!finalPath.isEmpty()) {
        m.filePath = finalPath;
        m.fileUrl = finalPath.startsWith(QLatin1String("content://"))
            ? finalPath
            : QUrl::fromLocalFile(finalPath).toString();
    }

    const QModelIndex modelIdx = index(idx);
    emit dataChanged(modelIdx, modelIdx, {StatusRole, ProgressRole, BytesTransferredRole, FilePathRole, FileUrlRole});
}

void MessageListModel::updateSenderName(const QString &senderId, const QString &newName)
{
    if (senderId.isEmpty() || newName.isEmpty()) return;

    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages.at(i).senderId == senderId && m_messages.at(i).senderName != newName) {
            m_messages[i].senderName = newName;
            const QModelIndex modelIdx = index(i);
            emit dataChanged(modelIdx, modelIdx, {SenderNameRole});
        }
    }
}

const ChatMessage *MessageListModel::findMessage(const QString &msgId) const
{
    const int idx = indexOfMsg(msgId);
    return idx >= 0 ? &m_messages.at(idx) : nullptr;
}

int MessageListModel::indexOfMsg(const QString &msgId) const
{
    for (int i = static_cast<int>(m_messages.size()) - 1; i >= 0; --i) {
        if (m_messages.at(i).msgId == msgId) return i;
    }
    return -1;
}

} // namespace landrop
