#pragma once

#include "core/session/PeerSession.h"

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QUrl>
#include <QVector>

namespace landrop {

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

    struct SharedFileEntry {
        QString filePath;
        QString fileName;
        bool deleteOnFinish = false;
    };

    PeerSession *ensureSession(const QString &peerId, const QString &peerName, const QString &platform);
    void removePendingIncoming(const QString &peerId, bool closeSocket);
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
