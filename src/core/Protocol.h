#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstdint>

namespace landrop {

inline constexpr quint32 kProtocolMagic = 0x4C445250U; // "LDRP"
inline constexpr int kHeaderSize = 13;                 // 4B Magic + 1B MsgType + 8B PayloadLength
inline constexpr qint64 kStreamChunkSize = 64 * 1024;  // 64 KB
inline constexpr qint64 kSocketWriteHighWater = 256 * 1024; // 256 KB backpressure threshold
inline constexpr quint64 kMaxFramePayload = 4ULL * 1024ULL * 1024ULL; // 4 MB safety guard
inline constexpr quint16 kDiscoveryPort = 45454;
inline constexpr quint16 kDefaultTcpPort = 45455;
inline constexpr int kDiscoveryIntervalMs = 2000;
inline constexpr qint64 kDeviceTimeoutMs = 6000;
inline constexpr int kTransferIdByteSize = 16;

enum class MsgType : quint8 {
    HandshakeReq     = 0x01,
    HandshakeAck     = 0x02,
    TextMessage      = 0x03,
    FileMeta         = 0x04,
    FileChunk        = 0x05,
    FileComplete     = 0x06,
    FileCancel       = 0x07,
    Heartbeat        = 0x08,
    DeviceInfoUpdate = 0x09,

    // Group chat & direct group-file fetch frames
    GroupJoinReq      = 0x10,
    GroupJoinAck      = 0x11,
    GroupMemberSync   = 0x12,
    GroupTextMessage  = 0x13,
    GroupFileAnnounce = 0x14,
    GroupFileFetchReq = 0x15,
    GroupLeave        = 0x16,
    GroupDisband      = 0x17
};

enum class SessionState : int {
    Disconnected    = 0,
    PendingOutgoing = 1,
    PendingIncoming = 2,
    Connected       = 3
};

inline QString classifyMediaKind(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    static const QStringList kImageExts = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif"),
        QStringLiteral("heic"), QStringLiteral("avif")
    };
    static const QStringList kVideoExts = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
        QStringLiteral("avi"), QStringLiteral("webm"), QStringLiteral("m4v"),
        QStringLiteral("wmv"), QStringLiteral("3gp")
    };

    if (kImageExts.contains(ext)) {
        return QStringLiteral("image");
    }
    if (kVideoExts.contains(ext)) {
        return QStringLiteral("video");
    }
    return QStringLiteral("file");
}

inline QString formatFileSize(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("0 B");
    }
    constexpr double kUnit = 1024.0;
    if (bytes < 1024) {
        return QString::number(bytes) + QStringLiteral(" B");
    }
    if (bytes < 1024 * 1024) {
        return QString::number(static_cast<double>(bytes) / kUnit, 'f', 1) + QStringLiteral(" KB");
    }
    if (bytes < 1024LL * 1024LL * 1024LL) {
        return QString::number(static_cast<double>(bytes) / (kUnit * kUnit), 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(static_cast<double>(bytes) / (kUnit * kUnit * kUnit), 'f', 2) + QStringLiteral(" GB");
}

} // namespace landrop
