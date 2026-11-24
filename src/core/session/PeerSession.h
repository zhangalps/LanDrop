#pragma once

#include "core/PacketCodec.h"
#include "core/Protocol.h"
#include "core/model/MessageListModel.h"

#include <QFile>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>

#include <memory>

namespace landrop {

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
    bool shouldMaintainConnection() const { return m_shouldMaintainConnection; }
    MessageListModel *messages() { return &m_messages; }

    void setPeerInfo(const QString &name, const QString &platform);
    void setDownloadDir(const QString &dir) { m_downloadDir = dir; }
    void clearUnread();

    void connectOutgoing(const QString &ip, quint16 port);
    void attachAcceptedSocket(QTcpSocket *socket);
    void disconnectFromPeer();
    void reconnectIfDropped();

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
    void handleIncomingFileMeta(const QJsonObject &obj);
    void handleIncomingFileChunk(const QByteArray &payload);
    void handleIncomingFileComplete(const QJsonObject &obj);
    void handleIncomingFileCancel(const QJsonObject &obj);
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

    QQueue<OutgoingFileTask> m_outgoingQueue;
    bool m_hasActiveOutgoing = false;
    OutgoingFileTask m_activeOutgoing;
    std::unique_ptr<QFile> m_activeOutgoingFile;
    qint64 m_activeOutgoingSent = 0;

    QHash<QByteArray, IncomingFileTask> m_incomingFiles;
};

} // namespace landrop
