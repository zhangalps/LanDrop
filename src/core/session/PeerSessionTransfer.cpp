#include "core/session/PeerSession.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

#include <cstring>

namespace landrop {

namespace {

QString transferTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

QString sanitizeTransferFileName(const QString &name)
{
    QString cleaned = QFileInfo(name).fileName().trimmed();
    for (QChar bad : QStringLiteral("/\\:*?\"<>|")) {
        cleaned.replace(bad, QLatin1Char('_'));
    }
    return cleaned.isEmpty() ? QStringLiteral("unnamed_file") : cleaned;
}

} // namespace

void PeerSession::enqueueFile(const QUrl &url)
{
    if (m_state != SessionState::Connected || !m_socket) return;
    const PreparedFile prepared = AndroidHelper::prepareReadableFile(url);
    if (prepared.readablePath.isEmpty() || prepared.fileSize < 0) return;

    const QString fileName = sanitizeTransferFileName(prepared.displayName);
    const QString mediaKind = classifyMediaKind(fileName);
    const QUuid uuid = QUuid::createUuid();
    const QString transferId = uuid.toString(QUuid::WithoutBraces);

    m_outgoingQueue.enqueue(OutgoingFileTask{
        uuid, transferId, prepared.readablePath, fileName, mediaKind, prepared.fileSize, prepared.isTemporary
    });

    ChatMessage msg;
    msg.msgId = transferId;
    msg.isOutgoing = true;
    msg.kind = mediaKind;
    msg.fileName = fileName;
    msg.filePath = prepared.isTemporary ? AndroidHelper::toOpenablePath(url) : prepared.readablePath;
    msg.fileUrl = url.isLocalFile() ? url.toString() : AndroidHelper::toOpenablePath(url);
    msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
    if (msg.fileExtension.isEmpty()) msg.fileExtension = QStringLiteral("FILE");
    msg.fileSize = prepared.fileSize;
    msg.status = QStringLiteral("transferring");
    msg.timestampText = transferTimeText();
    m_messages.appendMessage(std::move(msg));

    if (!m_hasActiveOutgoing) startNextOutgoingFile();
}

void PeerSession::cancelTransfer(const QString &transferId)
{
    auto sendCancelFrame = [&]() {
        if (m_socket && m_state == SessionState::Connected) {
            m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, {{QStringLiteral("transferId"), transferId}}));
        }
    };

    if (m_hasActiveOutgoing && m_activeOutgoing.transferId == transferId) {
        if (m_activeOutgoingFile) { m_activeOutgoingFile->close(); m_activeOutgoingFile.reset(); }
        if (m_activeOutgoing.deleteSourceOnFinish) QFile::remove(m_activeOutgoing.sourcePath);
        m_hasActiveOutgoing = false;
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
        sendCancelFrame();
        startNextOutgoingFile();
        return;
    }

    for (int i = 0; i < m_outgoingQueue.size(); ++i) {
        if (m_outgoingQueue.at(i).transferId == transferId) {
            m_outgoingQueue.removeAt(i);
            m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
            return;
        }
    }

    const QByteArray rawId = QUuid(transferId).toRfc4122();
    if (auto it = m_incomingFiles.find(rawId); it != m_incomingFiles.end()) {
        if (it->file) { it->file->close(); QFile::remove(it->savedPath); }
        m_incomingFiles.erase(it);
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
        sendCancelFrame();
    }
}

void PeerSession::startNextOutgoingFile()
{
    while (!m_outgoingQueue.isEmpty() && m_socket && m_state == SessionState::Connected) {
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

        QJsonObject meta{
            {QStringLiteral("transferId"), m_activeOutgoing.transferId},
            {QStringLiteral("fileName"), m_activeOutgoing.fileName},
            {QStringLiteral("fileSize"), m_activeOutgoing.fileSize},
            {QStringLiteral("mediaType"), m_activeOutgoing.mediaKind},
            {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
        };
        m_socket->write(PacketCodec::encodeJson(MsgType::FileMeta, meta));
        pumpOutgoingFile();
        return;
    }
}

void PeerSession::pumpOutgoingFile()
{
    if (!m_hasActiveOutgoing || !m_activeOutgoingFile || !m_socket || m_state != SessionState::Connected) return;
    const QByteArray rawUuid = m_activeOutgoing.transferUuid.toRfc4122();

    while (m_socket->bytesToWrite() < kSocketWriteHighWater) {
        if (m_activeOutgoingFile->atEnd()) {
            const QString doneId = m_activeOutgoing.transferId;
            const QString donePath = m_activeOutgoing.sourcePath;
            const bool delTmp = m_activeOutgoing.deleteSourceOnFinish;
            m_activeOutgoingFile->close();
            m_activeOutgoingFile.reset();
            if (delTmp) QFile::remove(donePath);
            m_hasActiveOutgoing = false;

            m_socket->write(PacketCodec::encodeJson(MsgType::FileComplete, {{QStringLiteral("transferId"), doneId}}));
            m_messages.updateTransferStatus(doneId, QStringLiteral("completed"));
            emit fileTransferCompleted(m_peerId, doneId, donePath);
            startNextOutgoingFile();
            return;
        }

        QByteArray chunkPayload(kTransferIdByteSize + static_cast<qsizetype>(kStreamChunkSize), Qt::Uninitialized);
        std::memcpy(chunkPayload.data(), rawUuid.constData(), kTransferIdByteSize);
        const qint64 n = m_activeOutgoingFile->read(chunkPayload.data() + kTransferIdByteSize, kStreamChunkSize);
        if (n < 0) { cancelTransfer(m_activeOutgoing.transferId); return; }
        if (n == 0) continue;

        chunkPayload.resize(kTransferIdByteSize + static_cast<qsizetype>(n));
        m_socket->write(PacketCodec::encode(MsgType::FileChunk, chunkPayload));
        m_activeOutgoingSent += n;
        m_messages.updateTransferProgress(m_activeOutgoing.transferId, m_activeOutgoingSent);
    }
}

void PeerSession::handleIncomingFileMeta(const QJsonObject &obj)
{
    const QString transferId = obj.value(QStringLiteral("transferId")).toString();
    const QUuid uuid(transferId);
    if (uuid.isNull()) return;

    const QString fileName = sanitizeTransferFileName(obj.value(QStringLiteral("fileName")).toString());
    const qint64 fileSize = static_cast<qint64>(obj.value(QStringLiteral("fileSize")).toDouble(0));
    const QString mediaType = obj.value(QStringLiteral("mediaType")).toString(classifyMediaKind(fileName));

    const QString savePath = makeUniqueSavePath(fileName);
    auto outFile = std::make_shared<QFile>(savePath);
    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (m_socket) m_socket->write(PacketCodec::encodeJson(MsgType::FileCancel, {{QStringLiteral("transferId"), transferId}}));
        return;
    }

    m_incomingFiles.insert(uuid.toRfc4122(), IncomingFileTask{transferId, fileName, mediaType, savePath, fileSize, 0, std::move(outFile)});

    ChatMessage msg;
    msg.msgId = transferId;
    msg.kind = mediaType;
    msg.fileName = fileName;
    msg.filePath = savePath;
    msg.fileUrl = QUrl::fromLocalFile(savePath).toString();
    msg.fileExtension = QFileInfo(fileName).suffix().toUpper();
    if (msg.fileExtension.isEmpty()) msg.fileExtension = QStringLiteral("FILE");
    msg.fileSize = fileSize;
    msg.status = QStringLiteral("transferring");
    msg.timestampText = transferTimeText();
    m_messages.appendMessage(std::move(msg));

    emit unreadCountChanged(m_peerId, ++m_unreadCount);
    emit messageReceived(m_peerId);
}

void PeerSession::handleIncomingFileChunk(const QByteArray &payload)
{
    if (payload.size() < kTransferIdByteSize) return;
    auto it = m_incomingFiles.find(payload.left(kTransferIdByteSize));
    if (it == m_incomingFiles.end() || !it->file) return;

    const qint64 dataLen = payload.size() - kTransferIdByteSize;
    if (dataLen <= 0) return;

    if (it->file->write(payload.constData() + kTransferIdByteSize, dataLen) != dataLen) {
        const QString tid = it->transferId;
        it->file->close();
        QFile::remove(it->savedPath);
        m_incomingFiles.erase(it);
        m_messages.updateTransferStatus(tid, QStringLiteral("failed"));
        return;
    }
    it->receivedBytes += dataLen;
    m_messages.updateTransferProgress(it->transferId, it->receivedBytes);
}

void PeerSession::handleIncomingFileComplete(const QJsonObject &obj)
{
    const QString transferId = obj.value(QStringLiteral("transferId")).toString();
    auto it = m_incomingFiles.find(QUuid(transferId).toRfc4122());
    if (it == m_incomingFiles.end()) return;

    const QString savedPath = it->savedPath;
    const QString fileName = it->fileName;
    const QString mediaKind = it->mediaKind;
    if (it->file) { it->file->flush(); it->file->close(); }
    m_incomingFiles.erase(it);

#ifdef Q_OS_ANDROID
    AndroidHelper::exportToPublicStorage(savedPath, fileName, mediaKind);
#else
    Q_UNUSED(fileName)
    Q_UNUSED(mediaKind)
#endif
    m_messages.updateTransferStatus(transferId, QStringLiteral("completed"), savedPath);
    emit fileTransferCompleted(m_peerId, transferId, savedPath);
}

void PeerSession::handleIncomingFileCancel(const QJsonObject &obj)
{
    const QString transferId = obj.value(QStringLiteral("transferId")).toString();
    if (auto it = m_incomingFiles.find(QUuid(transferId).toRfc4122()); it != m_incomingFiles.end()) {
        if (it->file) { it->file->close(); QFile::remove(it->savedPath); }
        m_incomingFiles.erase(it);
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
    } else if (m_hasActiveOutgoing && m_activeOutgoing.transferId == transferId) {
        if (m_activeOutgoingFile) { m_activeOutgoingFile->close(); m_activeOutgoingFile.reset(); }
        m_hasActiveOutgoing = false;
        m_messages.updateTransferStatus(transferId, QStringLiteral("canceled"));
        startNextOutgoingFile();
    }
}

void PeerSession::abortAllActiveTransfers(const QString &status)
{
    if (m_hasActiveOutgoing) {
        if (m_activeOutgoingFile) { m_activeOutgoingFile->close(); m_activeOutgoingFile.reset(); }
        m_messages.updateTransferStatus(m_activeOutgoing.transferId, status);
        m_hasActiveOutgoing = false;
    }
    while (!m_outgoingQueue.isEmpty()) {
        m_messages.updateTransferStatus(m_outgoingQueue.dequeue().transferId, status);
    }
    for (auto it = m_incomingFiles.begin(); it != m_incomingFiles.end(); ++it) {
        if (it->file) { it->file->close(); QFile::remove(it->savedPath); }
        m_messages.updateTransferStatus(it->transferId, status);
    }
    m_incomingFiles.clear();
}

QString PeerSession::makeUniqueSavePath(const QString &fileName) const
{
    QDir dir(m_downloadDir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
    QString candidate = dir.filePath(fileName);
    if (!QFileInfo::exists(candidate)) return candidate;

    const QFileInfo info(fileName);
    for (int i = 1; i < 10000; ++i) {
        const QString next = info.suffix().isEmpty()
            ? QStringLiteral("%1 (%2)").arg(info.completeBaseName()).arg(i)
            : QStringLiteral("%1 (%2).%3").arg(info.completeBaseName()).arg(i).arg(info.suffix());
        candidate = dir.filePath(next);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
    return dir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('_') + fileName);
}

} // namespace landrop
