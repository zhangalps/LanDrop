#include "core/session/GroupFileFetcher.h"
#include "core/PacketCodec.h"
#include "core/Protocol.h"
#include "platform/AndroidHelper.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

#include <cstring>
#include <memory>

namespace landrop {

namespace {

QString uniqueSavePath(const QString &downloadDir, const QString &fileName)
{
    QDir dir(downloadDir);
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

} // namespace

void GroupFileFetcher::fetchFile(QObject *owner,
                                 MessageListModel *messages,
                                 const QString &requesterId,
                                 const QString &downloadDir,
                                 const QString &transferId)
{
    if (!owner || !messages) return;

    const ChatMessage *existing = messages->findMessage(transferId);
    if (!existing || existing->isOutgoing || existing->senderIp.isEmpty() || existing->senderPort == 0) {
        return;
    }
    if (existing->status == QLatin1String("transferring") || existing->status == QLatin1String("completed")) {
        return;
    }

    const QString fileName = existing->fileName;
    const QString mediaKind = existing->kind;
    const QString targetIp = existing->senderIp;
    const quint16 targetPort = existing->senderPort;

    messages->updateTransferStatus(transferId, QStringLiteral("transferring"));

    auto *sock = new QTcpSocket(owner);
    auto *codec = new PacketCodec(sock);
    sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    struct FetchState {
        QString savedPath;
        std::shared_ptr<QFile> file;
        qint64 received = 0;
        bool finished = false;
    };
    auto state = std::make_shared<FetchState>();

    QObject::connect(sock, &QTcpSocket::connected, sock, [sock, transferId, requesterId]() {
        QJsonObject req{
            {QStringLiteral("transferId"), transferId},
            {QStringLiteral("requesterId"), requesterId}
        };
        sock->write(PacketCodec::encodeJson(MsgType::GroupFileFetchReq, req));
    });

    QObject::connect(sock, &QTcpSocket::readyRead, codec, [sock, codec]() {
        const QByteArray chunk = sock->readAll();
        if (!chunk.isEmpty()) codec->feed(chunk);
    });

    QObject::connect(codec, &PacketCodec::packetReceived, owner,
                     [messages, sock, state, downloadDir, transferId, fileName, mediaKind](MsgType type, const QByteArray &payload) {
        if (type == MsgType::FileMeta) {
            state->savedPath = uniqueSavePath(downloadDir, fileName);
            state->file = std::make_shared<QFile>(state->savedPath);
            if (!state->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                state->file.reset();
                messages->updateTransferStatus(transferId, QStringLiteral("failed"));
                sock->abort();
            }
        } else if (type == MsgType::FileChunk) {
            if (!state->file || payload.size() < kTransferIdByteSize) return;
            const qint64 n = payload.size() - kTransferIdByteSize;
            if (n > 0) {
                state->file->write(payload.constData() + kTransferIdByteSize, n);
                state->received += n;
                messages->updateTransferProgress(transferId, state->received);
            }
        } else if (type == MsgType::FileComplete) {
            if (state->file) {
                state->file->flush();
                state->file->close();
                state->file.reset();
            }
            state->finished = true;
#ifdef Q_OS_ANDROID
            AndroidHelper::exportToPublicStorage(state->savedPath, fileName, mediaKind);
#endif
            messages->updateTransferStatus(transferId, QStringLiteral("completed"), state->savedPath);
            sock->disconnectFromHost();
        }
    });

    QObject::connect(sock, &QTcpSocket::disconnected, owner, [messages, sock, state, transferId]() {
        if (!state->finished) {
            if (state->file) {
                state->file->close();
                QFile::remove(state->savedPath);
            }
            messages->updateTransferStatus(transferId, QStringLiteral("failed"));
        }
        sock->deleteLater();
    });

    QObject::connect(sock, &QTcpSocket::errorOccurred, owner, [messages, sock, state, transferId](QAbstractSocket::SocketError) {
        if (!state->finished) {
            messages->updateTransferStatus(transferId, QStringLiteral("failed"));
            sock->deleteLater();
        }
    });

    sock->connectToHost(targetIp, targetPort);
}

void GroupFileFetcher::serveFile(QTcpSocket *socket,
                                 const QString &transferId,
                                 const QString &filePath,
                                 const QString &fileName)
{
    const QUuid uuid(transferId);
    auto file = std::make_shared<QFile>(filePath);
    if (uuid.isNull() || !file->open(QIODevice::ReadOnly)) {
        socket->abort();
        socket->deleteLater();
        return;
    }

    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    const QByteArray rawUuid = uuid.toRfc4122();
    QJsonObject meta{
        {QStringLiteral("transferId"), transferId},
        {QStringLiteral("fileName"), fileName},
        {QStringLiteral("fileSize"), file->size()},
        {QStringLiteral("mediaType"), classifyMediaKind(fileName)},
        {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
    };
    socket->write(PacketCodec::encodeJson(MsgType::FileMeta, meta));

    auto pump = [socket, file, rawUuid, transferId]() {
        while (socket->state() == QAbstractSocket::ConnectedState && socket->bytesToWrite() < kSocketWriteHighWater) {
            if (file->atEnd()) {
                file->close();
                socket->write(PacketCodec::encodeJson(MsgType::FileComplete, {{QStringLiteral("transferId"), transferId}}));
                socket->flush();
                socket->disconnectFromHost();
                return;
            }
            QByteArray chunk(kTransferIdByteSize + static_cast<qsizetype>(kStreamChunkSize), Qt::Uninitialized);
            std::memcpy(chunk.data(), rawUuid.constData(), kTransferIdByteSize);
            const qint64 n = file->read(chunk.data() + kTransferIdByteSize, kStreamChunkSize);
            if (n <= 0) break;
            chunk.resize(kTransferIdByteSize + static_cast<qsizetype>(n));
            socket->write(PacketCodec::encode(MsgType::FileChunk, chunk));
        }
    };

    QObject::connect(socket, &QTcpSocket::bytesWritten, socket, [pump](qint64) { pump(); });
    QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    pump();
}

} // namespace landrop
