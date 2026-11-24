#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace landrop {

struct ChatMessage {
    QString msgId;
    bool isOutgoing = false;
    QString kind; // "text" | "image" | "video" | "file" | "system"
    QString text;
    QString senderId;
    QString senderName;
    QString senderIp;
    quint16 senderPort = 0;
    QString fileName;
    QString filePath;
    QString fileUrl;
    QString fileExtension;
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    double progress = 0.0;
    QString status; // "available" | "transferring" | "completed" | "failed" | "canceled"
    QString timestampText;
};

class MessageListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        MsgIdRole = Qt::UserRole + 1,
        IsOutgoingRole,
        KindRole,
        TextRole,
        SenderIdRole,
        SenderNameRole,
        SenderIpRole,
        SenderPortRole,
        FileNameRole,
        FilePathRole,
        FileUrlRole,
        FileExtensionRole,
        FileSizeRole,
        FileSizeTextRole,
        BytesTransferredRole,
        ProgressRole,
        StatusRole,
        TimestampRole
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendMessage(ChatMessage msg);
    void updateTransferProgress(const QString &msgId, qint64 bytesTransferred, bool forceNotify = false);
    void updateTransferStatus(const QString &msgId, const QString &status, const QString &finalPath = {});
    void updateSenderName(const QString &senderId, const QString &newName);
    const ChatMessage *findMessage(const QString &msgId) const;

signals:
    void countChanged();

private:
    int indexOfMsg(const QString &msgId) const;

    QVector<ChatMessage> m_messages;
    QHash<QString, qint64> m_lastNotifyMs;
};

} // namespace landrop
