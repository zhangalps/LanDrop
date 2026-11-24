#include "core/TransferSession.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>

namespace landrop {

namespace {

QString currentTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

QString sanitizeFileName(const QString &name)
{
    QString cleaned = QFileInfo(name).fileName().trimmed();
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char(':'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('*'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('?'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('"'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('<'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('>'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('|'), QLatin1Char('_'));
    return cleaned.isEmpty() ? QStringLiteral("unnamed_file") : cleaned;
}

} // namespace

// ============================================================================
// MessageListModel
// ============================================================================

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_messages.size());
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
    if (idx < 0) {
        return;
    }

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
    if (idx < 0) {
        return;
    }

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
    if (senderId.isEmpty() || newName.isEmpty()) {
        return;
    }
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
        if (m_messages.at(i).msgId == msgId) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// PeerSession
// ============================================================================

PeerSession::PeerSession(QString peerId,
                         QString peerName,
                         QString peerPlatform,
                         QString localDeviceId,
                         QString localDeviceName,
                         QString localPlatform,
                         QString downloadDir,
                         QObject *parent)
    : QObject(parent)
    , m_peerId(std::move(peerId))
    , m_peerName(std::move(peerName))
    , m_peerPlatform(std::move(peerPlatform))
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_downloadDir(std::move(downloadDir))
{
    connect(&m_codec, &PacketCodec::packetReceived, this, &PeerSession::onPacketReceived);
    connect(&m_codec, &PacketCodec::protocolError, this, &PeerSession::onProtocolError);
}

PeerSession::~PeerSession()
{
    abortAllActiveTransfers(QStringLiteral("canceled"));
    detachSocket();
}

void PeerSession::setPeerInfo(const QString &name, const QString &platform)
{
    if (!name.isEmpty()) {
        m_peerName = name;
    }
    if (!platform.isEmpty()) {
        m_peerPlatform = platform;
    }
}

void PeerSession::clearUnread()
{
    if (m_unreadCount != 0) {
        m_unreadCount = 0;
        emit unreadCountChanged(m_peerId, m_unreadCount);
    }
}

void PeerSession::connectOutgoing(const QString &ip, quint16 port)
{
    m_peerIp = ip;
    m_lastIp = ip;
    m_lastPort = port;
    m_shouldMaintainConnection = true;
    if (m_state == SessionState::Connected || m_state == SessionState::PendingOutgoing) {
        return;
    }

    detachSocket();
    auto *socket = new QTcpSocket(this);
    attachSocket(socket);
    setState(SessionState::PendingOutgoing);
    socket->connectToHost(ip, port);
}

void PeerSession::attachAcceptedSocket(QTcpSocket *socket)
{
    detachSocket();
    socket->setParent(this);
    const QHostAddress addr = socket->peerAddress();
    bool ok = false;
    const quint32 ipv4 = addr.toIPv4Address(&ok);
    m_peerIp = ok ? QHostAddress(ipv4).toString() : addr.toString();
    m_lastIp = m_peerIp;
    m_shouldMaintainConnection = true;
    attachSocket(socket);
    setState(SessionState::Connected);

    // Process any bytes that may have already arrived right after HandshakeReq.
    if (m_socket->bytesAvailable() > 0) {
        onSocketReadyRead();
    }
}

void PeerSession::disconnectFromPeer()
{
    m_shouldMaintainConnection = false;
    abortAllActiveTransfers(QStringLiteral("canceled"));
    detachSocket();
    setState(SessionState::Disconnected);
}

void PeerSession::reconnectIfDropped()
{
    if (m_shouldMaintainConnection && m_state == SessionState::Disconnected
        && !m_lastIp.isEmpty() && m_lastPort > 0) {
        connectOutgoing(m_lastIp, m_lastPort);
    }
}

void PeerSession::attachSocket(QTcpSocket *socket)
{
    m_socket = socket;
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    m_codec.reset();

    connect(m_socket, &QTcpSocket::connected, this, &PeerSession::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &PeerSession::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &PeerSession::onSocketBytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, &PeerSession::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PeerSession::onSocketError);
}

void PeerSession::detachSocket()
{
    if (!m_socket) {
        return;
    }
    m_socket->disconnect(this);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->deleteLater();
    m_socket = nullptr;
    m_codec.reset();
}

void PeerSession::setState(SessionState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_peerId, m_state);
    }
}

void PeerSession::onSocketConnected()
{
    if (!m_socket) {
        return;
    }

    QJsonObject req;
    req.insert(QStringLiteral("deviceId"), m_localDeviceId);
    req.insert(QStringLiteral("deviceName"), m_localDeviceName);
    req.insert(QStringLiteral("platform"), m_localPlatform);
    m_socket->write(PacketCodec::encodeJson(MsgType::HandshakeReq, req));
}

void PeerSession::onSocketReadyRead()
{
    if (!m_socket) {
        return;
    }
    const QByteArray chunk = m_socket->readAll();
    if (!chunk.isEmpty()) {
        m_codec.feed(chunk);
    }
}

void PeerSession::onSocketBytesWritten(qint64 /*bytes*/)
{
    if (m_hasActiveOutgoing) {
        pumpOutgoingFile();
    }
}

void PeerSession::onSocketDisconnected()
{
    abortAllActiveTransfers(QStringLiteral("failed"));
    detachSocket();
    setState(SessionState::Disconnected);
}

void PeerSession::onSocketError(QAbstractSocket::SocketError /*error*/)
{
    if (m_state == SessionState::PendingOutgoing) {
        const QString errStr = m_socket ? m_socket->errorString() : QStringLiteral("Connection failed");
        abortAllActiveTransfers(QStringLiteral("failed"));
        detachSocket();
        setState(SessionState::Disconnected);
        emit connectionFailed(m_peerId, errStr);
    }
}

void PeerSession::onProtocolError(const QString & /*reason*/)
{
    disconnectFromPeer();
}

void PeerSession::sendLocalInfoUpdate(const QString &newName)
{
    m_localDeviceName = newName;
    if (m_state == SessionState::Connected && m_socket) {
        QJsonObject obj;
        obj.insert(QStringLiteral("deviceId"), m_localDeviceId);
        obj.insert(QStringLiteral("deviceName"), newName);
        obj.insert(QStringLiteral("platform"), m_localPlatform);
        m_socket->write(PacketCodec::encodeJson(MsgType::DeviceInfoUpdate, obj));
    }
}

void PeerSession::sendText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || m_state != SessionState::Connected || !m_socket) {
        return;
    }

    const QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QJsonObject obj;
    obj.insert(QStringLiteral("msgId"), msgId);
    obj.insert(QStringLiteral("text"), trimmed);
    obj.insert(QStringLiteral("timestamp"), nowMs);

    m_socket->write(PacketCodec::encodeJson(MsgType::TextMessage, obj));

    ChatMessage msg;
    msg.msgId = msgId;
    msg.isOutgoing = true;
    msg.kind = QStringLiteral("text");
    msg.text = trimmed;
    msg.status = QStringLiteral("completed");
    msg.progress = 1.0;
    msg.timestampText = currentTimeText();
    m_messages.appendMessage(std::move(msg));
}

void PeerSession::enqueueFile(const QUrl &url)
{
    if (m_state != SessionState::Connected || !m_socket) {
        return;
    }

    const PreparedFile prepared = AndroidHelper::prepareReadableFile(url);
    if (prepared.readablePath.isEmpty() || prepared.fileSize < 0) {
        return;
    }

    const QString fileName = sanitizeFileName(prepared.displayName);
    const QString mediaKind = classifyMediaKind(fileName);
    const QUuid uuid = QUuid::createUuid();
    const QString transferId = uuid.toString(QUuid::WithoutBraces);

    OutgoingFileTask task;
    task.transferUuid = uuid;
    task.transferId = transferId;
    task.sourcePath = prepared.readablePath;
    task.fileName = fileName;
    task.mediaKind = mediaKind;
    task.fileSize = prepared.fileSize;
    task.deleteSourceOnFinish = prepared.isTemporary;

    ChatMessage msg;
    msg.msgId = transferId;
    msg.isOutgoing = true;
    msg.kind = mediaKind;
    msg.fileName = fileName;
    msg.filePath = prepared.isTemporary ? AndroidHelper::toOpenablePath(url) : prepared.readablePath;
    msg.fileUrl = url.isLocalFile() ? url.toString() : AndroidHelper::toOpenablePath(url);
    msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
    if (msg.fileExtension.isEmpty()) {
        msg.fileExtension = QStringLiteral("FILE");
    }
    msg.fileSize = prepared.fileSize;
    msg.status = QStringLiteral("transferring");
    msg.progress = 0.0;
    msg.timestampText = currentTimeText();
    m_messages.appendMessage(std::move(msg));

    m_outgoingQueue.enqueue(std::move(task));
    if (!m_hasActiveOutgoing) {
        startNextOutgoingFile();
    }
}

void PeerSession::cancelTransfer(const QString &transferId)
{
    // Cancel active outgoing file
    if (m_hasActiveOutgoing && m_activeOutgoing.transferId == transferId) {
        if (m_activeOutgoingFile) {
            m_activeOutgoingFile->close();
            m_activeOutgoingFile.reset();
        }
        if (m_activeOutgoing.deleteSourceOnFinish) {
            QFile::remove(m_activeOutgoing.sourcePath);
        }
        m_hasActiveOutgoing = false;
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));

        if (m_socket && m_state == SessionState::Connected) {
            QJsonObject cancelObj;
            cancelObj.insert(QStringLiteral("transferId"), transferId);
            m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, cancelObj));
        }
        startNextOutgoingFile();
        return;
    }

    // Cancel queued outgoing file
    for (int i = 0; i < m_outgoingQueue.size(); ++i) {
        if (m_outgoingQueue.at(i).transferId == transferId) {
            m_outgoingQueue.removeAt(i);
            m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
            return;
        }
    }

    // Cancel active incoming file
    const QUuid uuid(transferId);
    const QByteArray rawId = uuid.toRfc4122();
    if (auto it = m_incomingFiles.find(rawId); it != m_incomingFiles.end()) {
        if (it->file) {
            it->file->close();
            QFile::remove(it->savedPath);
        }
        m_incomingFiles.erase(it);
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));

        if (m_socket && m_state == SessionState::Connected) {
            QJsonObject cancelObj;
            cancelObj.insert(QStringLiteral("transferId"), transferId);
            m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, cancelObj));
        }
    }
}

void PeerSession::startNextOutgoingFile()
{
    while (!m_outgoingQueue.isEmpty()) {
        if (!m_socket || m_state != SessionState::Connected) {
            return;
        }

        OutgoingFileTask task = m_outgoingQueue.dequeue();
        auto file = std::make_unique<QFile>(task.sourcePath);
        if (!file->open(QIODevice::ReadOnly)) {
            m_messages.updateTransferStatus(task.transferId, QStringLiteral("failed"));
            continue;
        }

        m_activeOutgoing = std::move(task);
        m_activeOutgoingFile = std::move(file);
        m_activeOutgoingSent = 0;
        m_hasActiveOutgoing = true;

        QJsonObject meta;
        meta.insert(QStringLiteral("transferId"), m_activeOutgoing.transferId);
        meta.insert(QStringLiteral("fileName"), m_activeOutgoing.fileName);
        meta.insert(QStringLiteral("fileSize"), m_activeOutgoing.fileSize);
        meta.insert(QStringLiteral("mediaType"), m_activeOutgoing.mediaKind);
        meta.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        m_socket->write(PacketCodec::encodeJson(MsgType::FileMeta, meta));

        pumpOutgoingFile();
        return;
    }
}

void PeerSession::pumpOutgoingFile()
{
    if (!m_hasActiveOutgoing || !m_activeOutgoingFile || !m_socket || m_state != SessionState::Connected) {
        return;
    }

    const QByteArray rawUuid = m_activeOutgoing.transferUuid.toRfc4122();

    while (m_socket->bytesToWrite() < kSocketWriteHighWater) {
        if (m_activeOutgoingFile->atEnd()) {
            const QString doneId = m_activeOutgoing.transferId;
            const QString donePath = m_activeOutgoing.sourcePath;
            const bool shouldDeleteTmp = m_activeOutgoing.deleteSourceOnFinish;
            m_activeOutgoingFile->close();
            m_activeOutgoingFile.reset();
            if (shouldDeleteTmp) {
                QFile::remove(donePath);
            }
            m_hasActiveOutgoing = false;

            QJsonObject doneObj;
            doneObj.insert(QStringLiteral("transferId"), doneId);
            m_socket->write(PacketCodec::encodeJson(MsgType::FileComplete, doneObj));

            m_messages.updateTransferStatus(doneId, QStringLiteral("completed"));
            emit fileTransferCompleted(m_peerId, doneId, donePath);

            startNextOutgoingFile();
            return;
        }

        QByteArray chunkPayload;
        chunkPayload.resize(kTransferIdByteSize + static_cast<qsizetype>(kStreamChunkSize));
        std::memcpy(chunkPayload.data(), rawUuid.constData(), kTransferIdByteSize);

        const qint64 bytesRead = m_activeOutgoingFile->read(
            chunkPayload.data() + kTransferIdByteSize,
            kStreamChunkSize);

        if (bytesRead < 0) {
            const QString failedId = m_activeOutgoing.transferId;
            m_activeOutgoingFile->close();
            m_activeOutgoingFile.reset();
            m_hasActiveOutgoing = false;
            m_messages.updateTransferStatus(failedId, QStringLiteral("failed"));

            QJsonObject cancelObj;
            cancelObj.insert(QStringLiteral("transferId"), failedId);
            m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, cancelObj));

            startNextOutgoingFile();
            return;
        }

        if (bytesRead == 0) {
            // Reached EOF cleanly
            continue;
        }

        chunkPayload.resize(kTransferIdByteSize + static_cast<qsizetype>(bytesRead));
        m_socket->write(PacketCodec::encode(MsgType::FileChunk, chunkPayload));

        m_activeOutgoingSent += bytesRead;
        m_messages.updateTransferProgress(m_activeOutgoing.transferId, m_activeOutgoingSent);
    }
}

void PeerSession::onPacketReceived(MsgType type, const QByteArray &payload)
{
    switch (type) {
    case MsgType::HandshakeAck: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const bool accepted = obj.value(QStringLiteral("accepted")).toBool(false);
        if (accepted) {
            setPeerInfo(
                obj.value(QStringLiteral("deviceName")).toString(),
                obj.value(QStringLiteral("platform")).toString());
            setState(SessionState::Connected);
        } else {
            const QString reason = obj.value(QStringLiteral("reason")).toString(QStringLiteral("Rejected"));
            m_shouldMaintainConnection = false;
            detachSocket();
            setState(SessionState::Disconnected);
            emit handshakeRejected(m_peerId, reason);
        }
        break;
    }

    case MsgType::TextMessage: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        ChatMessage msg;
        msg.msgId = obj.value(QStringLiteral("msgId")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        msg.isOutgoing = false;
        msg.kind = QStringLiteral("text");
        msg.text = obj.value(QStringLiteral("text")).toString();
        msg.status = QStringLiteral("completed");
        msg.progress = 1.0;
        msg.timestampText = currentTimeText();
        m_messages.appendMessage(std::move(msg));

        ++m_unreadCount;
        emit unreadCountChanged(m_peerId, m_unreadCount);
        emit messageReceived(m_peerId);
        break;
    }

    case MsgType::FileMeta: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString transferId = obj.value(QStringLiteral("transferId")).toString();
        const QString fileName = sanitizeFileName(obj.value(QStringLiteral("fileName")).toString());
        const qint64 fileSize = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble(0));
        const QString mediaType = obj.value(QStringLiteral("mediaType")).toString(classifyMediaKind(fileName));

        const QUuid uuid(transferId);
        if (uuid.isNull()) {
            return;
        }

        const QString savePath = makeUniqueSavePath(fileName);
        auto outFile = std::make_shared<QFile>(savePath);
        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonObject cancelObj;
            cancelObj.insert(QStringLiteral("transferId"), transferId);
            if (m_socket) {
                m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, cancelObj));
            }
            return;
        }

        IncomingFileTask incoming;
        incoming.transferId = transferId;
        incoming.fileName = fileName;
        incoming.mediaKind = mediaType;
        incoming.savedPath = savePath;
        incoming.expectedSize = fileSize;
        incoming.receivedBytes = 0;
        incoming.file = std::move(outFile);
        m_incomingFiles.insert(uuid.toRfc4122(), std::move(incoming));

        ChatMessage msg;
        msg.msgId = transferId;
        msg.isOutgoing = false;
        msg.kind = mediaType;
        msg.fileName = fileName;
        msg.filePath = savePath;
        msg.fileUrl = QUrl::fromLocalFile(savePath).toString();
        msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
        if (msg.fileExtension.isEmpty()) {
            msg.fileExtension = QStringLiteral("FILE");
        }
        msg.fileSize = fileSize;
        msg.status = QStringLiteral("transferring");
        msg.progress = 0.0;
        msg.timestampText = currentTimeText();
        m_messages.appendMessage(std::move(msg));

        ++m_unreadCount;
        emit unreadCountChanged(m_peerId, m_unreadCount);
        emit messageReceived(m_peerId);
        break;
    }

    case MsgType::FileChunk: {
        if (payload.size() < kTransferIdByteSize) {
            return;
        }
        const QByteArray rawUuid = payload.left(kTransferIdByteSize);
        auto it = m_incomingFiles.find(rawUuid);
        if (it == m_incomingFiles.end() || !it->file) {
            return;
        }

        const qint64 dataLen = payload.size() - kTransferIdByteSize;
        if (dataLen > 0) {
            const qint64 written = it->file->write(payload.constData() + kTransferIdByteSize, dataLen);
            if (written != dataLen) {
                const QString tid = it->transferId;
                it->file->close();
                QFile::remove(it->savedPath);
                m_incomingFiles.erase(it);
                m_messages.updateTransferStatus(tid, QStringLiteral("failed"));
                return;
            }
            it->receivedBytes += written;
            m_messages.updateTransferProgress(it->transferId, it->receivedBytes);
        }
        break;
    }

    case MsgType::FileComplete: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString transferId = obj.value(QStringLiteral("transferId")).toString();
        const QByteArray rawUuid = QUuid(transferId).toRfc4122();
        auto it = m_incomingFiles.find(rawUuid);
        if (it == m_incomingFiles.end()) {
            return;
        }

        const QString savedPath = it->savedPath;
        const QString fileName = it->fileName;
        const QString mediaKind = it->mediaKind;
        if (it->file) {
            it->file->flush();
            it->file->close();
        }
        m_incomingFiles.erase(it);

#ifdef Q_OS_ANDROID
        // Automatically export received file into public Android storage (Download/Pictures/Movies)
        AndroidHelper::exportToPublicStorage(savedPath, fileName, mediaKind);
#else
        Q_UNUSED(fileName)
        Q_UNUSED(mediaKind)
#endif

        m_messages.updateTransferStatus(transferId, QStringLiteral("completed"), savedPath);
        emit fileTransferCompleted(m_peerId, transferId, savedPath);
        break;
    }

    case MsgType::FileCancel: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString transferId = obj.value(QStringLiteral("transferId")).toString();
        const QByteArray rawUuid = QUuid(transferId).toRfc4122();

        if (auto it = m_incomingFiles.find(rawUuid); it != m_incomingFiles.end()) {
            if (it->file) {
                it->file->close();
                QFile::remove(it->savedPath);
            }
            m_incomingFiles.erase(it);
            m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
        } else if (m_hasActiveOutgoing && m_activeOutgoing.transferId == transferId) {
            if (m_activeOutgoingFile) {
                m_activeOutgoingFile->close();
                m_activeOutgoingFile.reset();
            }
            m_hasActiveOutgoing = false;
            m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
            startNextOutgoingFile();
        }
        break;
    }

    case MsgType::DeviceInfoUpdate: {
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        const QString newName = obj.value(QStringLiteral("deviceName")).toString().trimmed();
        const QString newPlat = obj.value(QStringLiteral("platform")).toString();
        if (!newName.isEmpty()) {
            setPeerInfo(newName, newPlat);
            emit peerInfoUpdated(m_peerId, m_peerName, m_peerPlatform);
        }
        break;
    }

    case MsgType::HandshakeReq:
    case MsgType::Heartbeat:
    default:
        break;
    }
}

void PeerSession::abortAllActiveTransfers(const QString &status)
{
    if (m_hasActiveOutgoing) {
        if (m_activeOutgoingFile) {
            m_activeOutgoingFile->close();
            m_activeOutgoingFile.reset();
        }
        m_messages.updateTransferStatus(m_activeOutgoing.transferId, status);
        m_hasActiveOutgoing = false;
    }

    while (!m_outgoingQueue.isEmpty()) {
        const OutgoingFileTask queued = m_outgoingQueue.dequeue();
        m_messages.updateTransferStatus(queued.transferId, status);
    }

    for (auto it = m_incomingFiles.begin(); it != m_incomingFiles.end(); ++it) {
        if (it->file) {
            it->file->close();
            QFile::remove(it->savedPath);
        }
        m_messages.updateTransferStatus(it->transferId, status);
    }
    m_incomingFiles.clear();
}

QString PeerSession::makeUniqueSavePath(const QString &fileName) const
{
    QDir dir(m_downloadDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString candidate = dir.filePath(fileName);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }

    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString ext = info.suffix();

    for (int counter = 1; counter < 10000; ++counter) {
        const QString nextName = ext.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(counter)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(counter).arg(ext);
        candidate = dir.filePath(nextName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return dir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('_') + fileName);
}

// ============================================================================
// TransferManager
// ============================================================================

TransferManager::TransferManager(QString localDeviceId,
                                 QString localDeviceName,
                                 QString localPlatform,
                                 QObject *parent)
    : QObject(parent)
    , m_localDeviceId(std::move(localDeviceId))
    , m_localDeviceName(std::move(localDeviceName))
    , m_localPlatform(std::move(localPlatform))
    , m_downloadDir(AndroidHelper::downloadDirectory())
{
    connect(&m_server, &QTcpServer::newConnection, this, &TransferManager::onNewTcpConnection);
}

TransferManager::~TransferManager()
{
    stopServer();
}

bool TransferManager::startServer(quint16 port)
{
    if (m_server.isListening()) {
        return true;
    }
    return m_server.listen(QHostAddress::AnyIPv4, port);
}

void TransferManager::stopServer()
{
    m_server.close();
    while (!m_pendingQueue.isEmpty()) {
        const auto item = m_pendingQueue.takeFirst();
        if (item.socket) {
            item.socket->disconnect();
            item.socket->abort();
            item.socket->deleteLater();
        }
        if (item.codec) {
            item.codec->deleteLater();
        }
    }
    for (PeerSession *session : m_sessions) {
        session->disconnectFromPeer();
    }
}

void TransferManager::setDownloadDirectory(const QString &dir)
{
    m_downloadDir = dir;
    for (PeerSession *session : m_sessions) {
        session->setDownloadDir(dir);
    }
}

void TransferManager::setLocalDeviceName(const QString &name)
{
    if (name.trimmed().isEmpty() || m_localDeviceName == name.trimmed()) {
        return;
    }
    m_localDeviceName = name.trimmed();
    for (PeerSession *session : m_sessions) {
        session->sendLocalInfoUpdate(m_localDeviceName);
    }
}

void TransferManager::registerSharedFile(const QString &transferId, const QString &filePath, const QString &fileName, bool deleteOnFinish)
{
    m_sharedFiles.insert(transferId, SharedFileEntry{filePath, fileName, deleteOnFinish});
}

void TransferManager::unregisterSharedFile(const QString &transferId)
{
    auto it = m_sharedFiles.find(transferId);
    if (it != m_sharedFiles.end()) {
        if (it->deleteOnFinish) {
            QFile::remove(it->filePath);
        }
        m_sharedFiles.erase(it);
    }
}

QString TransferManager::activePeerName() const
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        return s->peerName();
    }
    return {};
}

QString TransferManager::activePeerIp() const
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        return s->peerIp();
    }
    return {};
}

int TransferManager::activePeerState() const
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        return static_cast<int>(s->state());
    }
    return static_cast<int>(SessionState::Disconnected);
}

QObject *TransferManager::activeMessageModel() const
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        return s->messages();
    }
    return nullptr;
}

QString TransferManager::incomingPeerId() const
{
    return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().peerId;
}

QString TransferManager::incomingPeerName() const
{
    return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().peerName;
}

QString TransferManager::incomingPeerPlatform() const
{
    return m_pendingQueue.isEmpty() ? QString{} : m_pendingQueue.first().platform;
}

PeerSession *TransferManager::sessionForPeer(const QString &peerId) const
{
    return m_sessions.value(peerId, nullptr);
}

PeerSession *TransferManager::ensureSession(const QString &peerId,
                                            const QString &peerName,
                                            const QString &platform)
{
    PeerSession *session = m_sessions.value(peerId, nullptr);
    if (!session) {
        session = new PeerSession(
            peerId, peerName, platform,
            m_localDeviceId, m_localDeviceName, m_localPlatform,
            m_downloadDir, this);
        m_sessions.insert(peerId, session);

        connect(session, &PeerSession::stateChanged, this, [this](const QString &pid, SessionState newState) {
            PeerSession *s = sessionForPeer(pid);
            emit peerStatusUpdated(pid, static_cast<int>(newState), s ? s->unreadCount() : 0);
            if (pid == m_activePeerId) {
                emit activePeerChanged();
                emit activePeerStateChanged();
            }
            if (newState == SessionState::Connected) {
                emit sessionConnected(pid);
            }
        });

        connect(session, &PeerSession::unreadCountChanged, this, [this](const QString &pid, int unread) {
            if (pid == m_activePeerId && unread > 0) {
                if (PeerSession *s = sessionForPeer(pid)) {
                    s->clearUnread();
                }
                return;
            }
            PeerSession *s = sessionForPeer(pid);
            emit peerStatusUpdated(pid, s ? static_cast<int>(s->state()) : 0, unread);
        });

        connect(session, &PeerSession::handshakeRejected, this, &TransferManager::handshakeRejected);
        connect(session, &PeerSession::peerInfoUpdated, this, [this](const QString &pid, const QString &name, const QString &) {
            emit peerInfoUpdated(pid, name);
            if (pid == m_activePeerId) {
                emit activePeerChanged();
            }
        });
        connect(session, &PeerSession::connectionFailed, this, [this](const QString & /*pid*/, const QString &reason) {
            emit connectionFailed(reason);
        });
        connect(session, &PeerSession::fileTransferCompleted, this, &TransferManager::fileTransferCompleted);
    } else {
        session->setPeerInfo(peerName, platform);
    }
    return session;
}

void TransferManager::connectToDevice(const QString &deviceId,
                                      const QString &deviceName,
                                      const QString &platform,
                                      const QString &ip,
                                      quint16 port)
{
    if (deviceId.isEmpty()) {
        return;
    }

    PeerSession *session = ensureSession(deviceId, deviceName, platform);
    selectPeer(deviceId);

    if (session->state() == SessionState::Disconnected) {
        session->connectOutgoing(ip, port);
    }
}

void TransferManager::connectByAddress(const QString &addressInput)
{
    QString cleaned = addressInput.trimmed();
    cleaned.replace(QChar(0xFF1A), QLatin1Char(':')); // support full-width colon
    if (cleaned.isEmpty()) {
        return;
    }

    QString host = cleaned;
    quint16 port = kDefaultTcpPort;
    const int colonIdx = cleaned.lastIndexOf(QLatin1Char(':'));
    if (colonIdx > 0) {
        bool ok = false;
        const int parsedPort = cleaned.mid(colonIdx + 1).toInt(&ok);
        if (ok && parsedPort > 0 && parsedPort <= 65535) {
            host = cleaned.left(colonIdx).trimmed();
            port = static_cast<quint16>(parsedPort);
        }
    }

    if (host.isEmpty()) {
        emit connectionFailed(QStringLiteral("Invalid IP address"));
        return;
    }

    const QString manualId = QStringLiteral("direct:%1:%2").arg(host).arg(port);
    connectToDevice(manualId, host, QStringLiteral("direct"), host, port);
}

void TransferManager::selectPeer(const QString &peerId)
{
    if (m_activePeerId == peerId) {
        if (PeerSession *s = sessionForPeer(peerId)) {
            s->clearUnread();
        }
        return;
    }

    m_activePeerId = peerId;
    if (PeerSession *s = sessionForPeer(peerId)) {
        s->clearUnread();
    }
    emit activePeerChanged();
    emit activePeerStateChanged();
}

void TransferManager::clearActivePeer()
{
    if (!m_activePeerId.isEmpty()) {
        m_activePeerId.clear();
        emit activePeerChanged();
        emit activePeerStateChanged();
    }
}

void TransferManager::onNewTcpConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        auto *codec = new PacketCodec(socket);

        connect(socket, &QTcpSocket::readyRead, codec, [socket, codec]() {
            const QByteArray data = socket->readAll();
            if (!data.isEmpty()) {
                codec->feed(data);
            }
        });

        connect(codec, &PacketCodec::protocolError, socket, [socket]() {
            socket->abort();
            socket->deleteLater();
        });

        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            for (int i = 0; i < m_pendingQueue.size(); ++i) {
                if (m_pendingQueue.at(i).socket == socket) {
                    const QString pid = m_pendingQueue.at(i).peerId;
                    m_pendingQueue.removeAt(i);
                    emit incomingRequestChanged();
                    if (PeerSession *s = sessionForPeer(pid)) {
                        emit peerStatusUpdated(pid, static_cast<int>(s->state()), s->unreadCount());
                    } else {
                        emit peerStatusUpdated(pid, static_cast<int>(SessionState::Disconnected), 0);
                    }
                    break;
                }
            }
            socket->deleteLater();
        });

        connect(codec, &PacketCodec::packetReceived, this, [this, socket, codec](MsgType type, const QByteArray &payload) {
            if (type == MsgType::GroupJoinReq) {
                socket->disconnect(codec);
                socket->disconnect(this);
                codec->disconnect(this);
                emit incomingGroupJoin(socket, codec, payload);
                return;
            }
            if (type == MsgType::GroupFileFetchReq) {
                socket->disconnect(codec);
                socket->disconnect(this);
                codec->disconnect(this);
                codec->deleteLater();
                handleGroupFileFetch(socket, payload);
                return;
            }
            if (type != MsgType::HandshakeReq) {
                socket->abort();
                socket->deleteLater();
                return;
            }

            // Detach temporary handshake codec listeners
            socket->disconnect(codec);
            codec->disconnect(this);

            const QJsonObject obj = QJsonDocument::fromJson(payload).object();
            const QString peerId = obj.value(QStringLiteral("deviceId")).toString();
            const QString peerName = obj.value(QStringLiteral("deviceName")).toString(QStringLiteral("Unknown"));
            const QString platform = obj.value(QStringLiteral("platform")).toString(QStringLiteral("desktop"));

            if (peerId.isEmpty() || peerId == m_localDeviceId) {
                socket->abort();
                socket->deleteLater();
                return;
            }

            // If this peer had an active session that dropped (e.g. Android backgrounded to WPS),
            // auto-accept the silent reconnect without showing a duplicate prompt.
            if (PeerSession *existing = sessionForPeer(peerId);
                existing && existing->shouldMaintainConnection()) {
                socket->disconnect(this);
                codec->deleteLater();
                QJsonObject ack;
                ack.insert(QStringLiteral("accepted"), true);
                ack.insert(QStringLiteral("deviceId"), m_localDeviceId);
                ack.insert(QStringLiteral("deviceName"), m_localDeviceName);
                ack.insert(QStringLiteral("platform"), m_localPlatform);
                socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));
                existing->setPeerInfo(peerName, platform);
                existing->attachAcceptedSocket(socket);
                return;
            }

            // Replace any duplicate pending request from the same peer
            removePendingIncoming(peerId, true);

            PendingIncoming item;
            item.peerId = peerId;
            item.peerName = peerName;
            item.platform = platform;
            item.socket = socket;
            item.codec = codec;
            m_pendingQueue.append(item);

            emit peerStatusUpdated(peerId, static_cast<int>(SessionState::PendingIncoming), 0);
            emit incomingRequestChanged();
        });

        if (socket->bytesAvailable() > 0) {
            codec->feed(socket->readAll());
        }
    }
}

void TransferManager::acceptIncomingRequest(const QString &peerId)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId != peerId) {
            continue;
        }

        const PendingIncoming item = m_pendingQueue.takeAt(i);
        emit incomingRequestChanged();

        if (!item.socket || item.socket->state() != QAbstractSocket::ConnectedState) {
            if (item.socket) {
                item.socket->deleteLater();
            }
            return;
        }

        item.socket->disconnect(this);
        if (item.codec) {
            item.codec->deleteLater();
        }

        QJsonObject ack;
        ack.insert(QStringLiteral("accepted"), true);
        ack.insert(QStringLiteral("deviceId"), m_localDeviceId);
        ack.insert(QStringLiteral("deviceName"), m_localDeviceName);
        ack.insert(QStringLiteral("platform"), m_localPlatform);
        item.socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));

        PeerSession *session = ensureSession(item.peerId, item.peerName, item.platform);
        session->attachAcceptedSocket(item.socket);
        selectPeer(item.peerId);
        return;
    }
}

void TransferManager::rejectIncomingRequest(const QString &peerId)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId != peerId) {
            continue;
        }

        const PendingIncoming item = m_pendingQueue.takeAt(i);
        emit incomingRequestChanged();

        if (item.socket) {
            item.socket->disconnect(this);
            if (item.socket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject ack;
                ack.insert(QStringLiteral("accepted"), false);
                ack.insert(QStringLiteral("reason"), QStringLiteral("Declined by user"));
                item.socket->write(PacketCodec::encodeJson(MsgType::HandshakeAck, ack));
                item.socket->flush();
                connect(item.socket, &QTcpSocket::disconnected, item.socket, &QObject::deleteLater);
                item.socket->disconnectFromHost();
            } else {
                item.socket->deleteLater();
            }
        }

        if (PeerSession *s = sessionForPeer(peerId)) {
            emit peerStatusUpdated(peerId, static_cast<int>(s->state()), s->unreadCount());
        } else {
            emit peerStatusUpdated(peerId, static_cast<int>(SessionState::Disconnected), 0);
        }
        return;
    }
}

void TransferManager::removePendingIncoming(const QString &peerId, bool closeSocket)
{
    for (int i = 0; i < m_pendingQueue.size(); ++i) {
        if (m_pendingQueue.at(i).peerId == peerId) {
            const PendingIncoming item = m_pendingQueue.takeAt(i);
            if (closeSocket && item.socket) {
                item.socket->disconnect(this);
                item.socket->abort();
                item.socket->deleteLater();
            }
            emit incomingRequestChanged();
            return;
        }
    }
}

void TransferManager::disconnectPeer(const QString &peerId)
{
    if (PeerSession *s = sessionForPeer(peerId)) {
        s->disconnectFromPeer();
    }
}

void TransferManager::reconnectActiveSessions()
{
    for (PeerSession *s : m_sessions) {
        s->reconnectIfDropped();
    }
}

void TransferManager::sendText(const QString &text)
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        s->sendText(text);
    }
}

void TransferManager::sendFiles(const QList<QUrl> &urls)
{
    PeerSession *s = sessionForPeer(m_activePeerId);
    if (!s) {
        return;
    }
    for (const QUrl &url : urls) {
        s->enqueueFile(url);
    }
}

void TransferManager::cancelTransfer(const QString &transferId)
{
    if (PeerSession *s = sessionForPeer(m_activePeerId)) {
        s->cancelTransfer(transferId);
    }
}

void TransferManager::handleGroupFileFetch(QTcpSocket *socket, const QByteArray &payload)
{
    const QJsonObject req = QJsonDocument::fromJson(payload).object();
    const QString transferId = req.value(QStringLiteral("transferId")).toString();
    const QUuid uuid(transferId);

    const auto it = m_sharedFiles.constFind(transferId);
    if (uuid.isNull() || it == m_sharedFiles.cend()) {
        socket->abort();
        socket->deleteLater();
        return;
    }

    auto file = std::make_shared<QFile>(it->filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        socket->abort();
        socket->deleteLater();
        return;
    }

    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    const QString fileName = it->fileName;
    const qint64 fileSize = file->size();
    const QByteArray rawUuid = uuid.toRfc4122();

    QJsonObject meta;
    meta.insert(QStringLiteral("transferId"), transferId);
    meta.insert(QStringLiteral("fileName"), fileName);
    meta.insert(QStringLiteral("fileSize"), fileSize);
    meta.insert(QStringLiteral("mediaType"), classifyMediaKind(fileName));
    meta.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    socket->write(PacketCodec::encodeJson(MsgType::FileMeta, meta));

    auto pump = [socket, file, rawUuid, transferId]() {
        while (socket->state() == QAbstractSocket::ConnectedState && socket->bytesToWrite() < kSocketWriteHighWater) {
            if (file->atEnd()) {
                file->close();
                QJsonObject doneObj;
                doneObj.insert(QStringLiteral("transferId"), transferId);
                socket->write(PacketCodec::encodeJson(MsgType::FileComplete, doneObj));
                socket->flush();
                socket->disconnectFromHost();
                return;
            }
            QByteArray chunkPayload;
            chunkPayload.resize(kTransferIdByteSize + static_cast<qsizetype>(kStreamChunkSize));
            std::memcpy(chunkPayload.data(), rawUuid.constData(), kTransferIdByteSize);
            const qint64 n = file->read(chunkPayload.data() + kTransferIdByteSize, kStreamChunkSize);
            if (n <= 0) {
                break;
            }
            chunkPayload.resize(kTransferIdByteSize + static_cast<qsizetype>(n));
            socket->write(PacketCodec::encode(MsgType::FileChunk, chunkPayload));
        }
    };

    connect(socket, &QTcpSocket::bytesWritten, socket, [pump](qint64) { pump(); });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    pump();
}

} // namespace landrop
