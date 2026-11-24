#pragma once

#include "core/PacketCodec.h"
#include "core/Protocol.h"
#include "core/model/GroupMemberListModel.h"
#include "core/model/MessageListModel.h"
#include "core/session/TransferManager.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTcpSocket>
#include <QUrl>

namespace landrop {

class GroupRoom : public QObject
{
    Q_OBJECT

public:
    GroupRoom(QString groupId,
              QString groupName,
              QString hostId,
              QString hostName,
              QString pin,
              bool isHost,
              QString localDeviceId,
              QString localDeviceName,
              QString localPlatform,
              TransferManager *transferManager,
              QObject *parent = nullptr);
    ~GroupRoom() override;

    QString groupId() const { return m_groupId; }
    QString groupName() const { return m_groupName; }
    QString hostId() const { return m_hostId; }
    QString hostName() const { return m_hostName; }
    QString pin() const { return m_pin; }
    bool hasPin() const { return !m_pin.isEmpty(); }
    bool isHost() const { return m_isHost; }
    bool isJoined() const { return m_isJoined; }
    int memberCount() const { return m_memberModel.rowCount(); }
    int unreadCount() const { return m_unreadCount; }
    bool isKnownMember(const QString &deviceId) const { return m_knownMemberIds.contains(deviceId); }
    bool shouldMaintainConnection() const { return m_shouldMaintainConnection; }
    MessageListModel *messages() { return &m_messages; }
    GroupMemberListModel *memberModel() { return &m_memberModel; }

    void clearUnread();
    void updateLocalDeviceName(const QString &newName);

    void addMemberConnection(const GroupMemberInfo &member, QTcpSocket *socket, PacketCodec *codec);
    void disband();

    void connectToHost(const QString &hostIp, quint16 hostPort, const QString &pinAttempt);
    void reconnectIfDropped();
    void leave();

    void sendText(const QString &text);
    void sendFiles(const QList<QUrl> &urls);
    void downloadFile(const QString &transferId);

signals:
    void joinedChanged(const QString &groupId, bool joined);
    void joinRejected(const QString &groupId, const QString &reason);
    void memberCountChanged(const QString &groupId, int count);
    void unreadCountChanged(const QString &groupId, int unread);
    void disbanded(const QString &groupId, const QString &reason);

private:
    struct MemberConn {
        QString deviceId;
        QTcpSocket *socket = nullptr;
        PacketCodec *codec = nullptr;
    };

    void appendSystemMessage(const QString &text);
    void broadcastToMembers(const QByteArray &frame, const QString &excludeId = {});
    void handleHostPacket(const QString &senderId, MsgType type, const QByteArray &payload);
    void handleMemberPacket(MsgType type, const QByteArray &payload);
    void handleAnnounceMessage(const QJsonObject &obj);
    void applyMembersFromJson(const QJsonArray &arr);

    QString m_groupId;
    QString m_groupName;
    QString m_hostId;
    QString m_hostName;
    QString m_pin;
    bool m_isHost = false;
    bool m_isJoined = false;
    int m_unreadCount = 0;

    QString m_localDeviceId;
    QString m_localDeviceName;
    QString m_localPlatform;
    TransferManager *m_transferManager = nullptr;

    MessageListModel m_messages;
    GroupMemberListModel m_memberModel;

    QHash<QString, MemberConn> m_memberConns;
    QSet<QString> m_knownMemberIds;

    QTcpSocket *m_hostSocket = nullptr;
    PacketCodec m_hostCodec;
    QString m_hostIp;
    quint16 m_hostPort = 0;
    QString m_pinAttempt;
    bool m_shouldMaintainConnection = false;
};

} // namespace landrop
