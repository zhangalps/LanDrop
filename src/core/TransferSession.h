#pragma once

#include "core/PacketCodec.h"
#include "core/Protocol.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>
#include <QVector>

#include <memory>

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

class PeerSession : public QObject
{
    Q_OBJECT

public:
    PeerSession(QString peerId,
                QString peerName,
                QString peerPlatform,
                QString localDeviceId,
                QString localDeviceName,
                QString localPlatform,
                QString downloadDir,
                QObject *parent = nullptr);
    ~PeerSession() override;

    QString peerId() const { return m_peerId; }
    QString peerName() const { return m_peerName; }
    QString peerPlatform() const { return m_peerPlatform; }
    QString peerIp() const { return m_peerIp; }
    SessionState state() const { return m_state; }
    int unreadCount() const { return m_unreadCount; }
    MessageListModel *messages() { return &m_messages; }

    void setPeerInfo(const QString &name, const QString &platform);
    void setDownloadDir(const QString &dir) { m_downloadDir = dir; }
    void clearUnread();

    void connectOutgoing(const QString &ip, quint16 port);
    void attachAcceptedSocket(QTcpSocket *socket);
    void disconnectFromPeer();
    void reconnectIfDropped();
    bool shouldMaintainConnection() const { return m_shouldMaintainConnection; }

    void sendText(const QString &text);
    void enqueueFile(const QUrl &url);
    void cancelTransfer(const QString &transferId);
    void sendLocalInfoUpdate(const QString &newName);

signals:
    void stateChanged(const QString &peerId, landrop::SessionState newState);
    void unreadCountChanged(const QString &peerId, int unread);
    void peerInfoUpdated(const QString &peerId, const QString &peerName, const QString &peerPlatform);
    void handshakeRejected(const QString &peerId, const QString &reason);
    void connectionFailed(const QString &peerId, const QString &reason);
    void fileTransferCompleted(const QString &peerId, const QString &msgId, const QString &savedPath);
    void messageReceived(const QString &peerId);

private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketBytesWritten(qint64 bytes);
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onPacketReceived(landrop::MsgType type, const QByteArray &payload);
    void onProtocolError(const QString &reason);

private:
    struct OutgoingFileTask {
        QUuid transferUuid;
        QString transferId;
        QString sourcePath;
        QString fileName;
        QString mediaKind;
        qint64 fileSize = 0;
        bool deleteSourceOnFinish = false;
    };

    struct IncomingFileTask {
        QString transferId;
        QString fileName;
        QString mediaKind;
        QString savedPath;
        qint64 expectedSize = 0;
        qint64 receivedBytes = 0;
        std::shared_ptr<QFile> file;
    };

    void attachSocket(QTcpSocket *socket);
    void detachSocket();
    void setState(SessionState s);
    void startNextOutgoingFile();
    void pumpOutgoingFile();
    void abortAllActiveTransfers(const QString &status);
    QString makeUniqueSavePath(const QString &fileName) const;

    QString m_peerId;
    QString m_peerName;
    QString m_peerPlatform;
    QString m_peerIp;
    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_localPlatform;
    QString m_downloadDir;

    SessionState m_state = SessionState::Disconnected;
    int m_unreadCount = 0;
    bool m_shouldMaintainConnection = false;
    QString m_lastIp;
    quint16 m_lastPort = 0;

    QTcpSocket *m_socket = nullptr;
    PacketCodec m_codec;
    MessageListModel m_messages;

    // Outgoing file streaming state
    QQueue<OutgoingFileTask> m_outgoingQueue;
    bool m_hasActiveOutgoing = false;
    OutgoingFileTask m_activeOutgoing;
    std::unique_ptr<QFile> m_activeOutgoingFile;
    qint64 m_activeOutgoingSent = 0;

    // Incoming file streaming state (keyed by 16-byte raw UUID)
    QHash<QByteArray, IncomingFileTask> m_incomingFiles;
};

class TransferManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activePeerId READ activePeerId NOTIFY activePeerChanged)
    Q_PROPERTY(QString activePeerName READ activePeerName NOTIFY activePeerChanged)
    Q_PROPERTY(QString activePeerIp READ activePeerIp NOTIFY activePeerChanged)
    Q_PROPERTY(int activePeerState READ activePeerState NOTIFY activePeerStateChanged)
    Q_PROPERTY(QObject *activeMessageModel READ activeMessageModel NOTIFY activePeerChanged)
    Q_PROPERTY(int serverPort READ serverPort CONSTANT)
    Q_PROPERTY(bool hasIncomingRequest READ hasIncomingRequest NOTIFY incomingRequestChanged)
    Q_PROPERTY(QString incomingPeerId READ incomingPeerId NOTIFY incomingRequestChanged)
    Q_PROPERTY(QString incomingPeerName READ incomingPeerName NOTIFY incomingRequestChanged)
    Q_PROPERTY(QString incomingPeerPlatform READ incomingPeerPlatform NOTIFY incomingRequestChanged)

public:
    explicit TransferManager(QString localDeviceId,
                             QString localDeviceName,
                             QString localPlatform,
                             QObject *parent = nullptr);
    ~TransferManager() override;

    bool startServer(quint16 port = 0);
    void stopServer();
    quint16 serverPort() const { return m_server.serverPort(); }

    void setDownloadDirectory(const QString &dir);
    QString downloadDirectory() const { return m_downloadDir; }
    void setLocalDeviceName(const QString &name);

    void registerSharedFile(const QString &transferId, const QString &filePath, const QString &fileName, bool deleteOnFinish = false);
    void unregisterSharedFile(const QString &transferId);

    QString activePeerId() const { return m_activePeerId; }
    QString activePeerName() const;
    QString activePeerIp() const;
    int activePeerState() const;
    QObject *activeMessageModel() const;

    bool hasIncomingRequest() const { return !m_pendingQueue.isEmpty(); }
    QString incomingPeerId() const;
    QString incomingPeerName() const;
    QString incomingPeerPlatform() const;

    PeerSession *sessionForPeer(const QString &peerId) const;

    Q_INVOKABLE void connectToDevice(const QString &deviceId,
                                     const QString &deviceName,
                                     const QString &platform,
                                     const QString &ip,
                                     quint16 port);
    Q_INVOKABLE void connectByAddress(const QString &addressInput);
    Q_INVOKABLE void selectPeer(const QString &peerId);
    Q_INVOKABLE void clearActivePeer();
    Q_INVOKABLE void acceptIncomingRequest(const QString &peerId);
    Q_INVOKABLE void rejectIncomingRequest(const QString &peerId);
    Q_INVOKABLE void disconnectPeer(const QString &peerId);
    Q_INVOKABLE void reconnectActiveSessions();
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void sendFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void cancelTransfer(const QString &transferId);

signals:
    void activePeerChanged();
    void activePeerStateChanged();
    void incomingRequestChanged();
    void peerStatusUpdated(const QString &peerId, int connectionState, int unreadCount);
    void peerInfoUpdated(const QString &peerId, const QString &peerName);
    void sessionConnected(const QString &peerId);
    void handshakeRejected(const QString &peerId, const QString &reason);
    void connectionFailed(const QString &reason);
    void fileTransferCompleted(const QString &peerId, const QString &msgId, const QString &savedPath);
    void incomingGroupJoin(QTcpSocket *socket, landrop::PacketCodec *codec, const QByteArray &payload);

private slots:
    void onNewTcpConnection();

private:
    struct PendingIncoming {
        QString peerId;
        QString peerName;
        QString platform;
        QTcpSocket *socket = nullptr;
        PacketCodec *codec = nullptr;
    };

    PeerSession *ensureSession(const QString &peerId, const QString &peerName, const QString &platform);
    void removePendingIncoming(const QString &peerId, bool closeSocket);

    struct SharedFileEntry {
        QString filePath;
        QString fileName;
        bool deleteOnFinish = false;
    };

    void handleGroupFileFetch(QTcpSocket *socket, const QByteArray &payload);

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_localPlatform;
    QString m_downloadDir;

    QTcpServer m_server;
    QHash<QString, PeerSession *> m_sessions;
    QHash<QString, SharedFileEntry> m_sharedFiles;
    QVector<PendingIncoming> m_pendingQueue;
    QString m_activePeerId;
};

} // namespace landrop
