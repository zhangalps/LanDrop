#include "core/PacketCodec.h"

#include <QJsonDocument>
#include <QtEndian>

#include <cstring>

namespace landrop {

PacketCodec::PacketCodec(QObject *parent)
    : QObject(parent)
{
}

QByteArray PacketCodec::encode(MsgType type, const QByteArray &payload)
{
    QByteArray frame;
    frame.resize(kHeaderSize + payload.size());

    char *raw = frame.data();
    qToBigEndian<quint32>(kProtocolMagic, raw);
    raw[4] = static_cast<char>(static_cast<quint8>(type));
    qToBigEndian<quint64>(static_cast<quint64>(payload.size()), raw + 5);

    if (!payload.isEmpty()) {
        std::memcpy(raw + kHeaderSize, payload.constData(), static_cast<size_t>(payload.size()));
    }
    return frame;
}

QByteArray PacketCodec::encodeJson(MsgType type, const QJsonObject &object)
{
    return encode(type, QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void PacketCodec::reset()
{
    m_state = ParseState::ReadingHeader;
    m_buffer.clear();
    m_expectedPayloadLength = 0;
}

void PacketCodec::feed(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    m_buffer.append(data);

    while (true) {
        if (m_state == ParseState::ReadingHeader) {
            if (m_buffer.size() < kHeaderSize) {
                return;
            }

            const char *raw = m_buffer.constData();
            const quint32 magic = qFromBigEndian<quint32>(raw);
            if (magic != kProtocolMagic) {
                reset();
                emit protocolError(QStringLiteral("Invalid protocol magic header"));
                return;
            }

            const auto typeVal = static_cast<quint8>(raw[4]);
            const quint64 payloadLen = qFromBigEndian<quint64>(raw + 5);
            if (payloadLen > kMaxFramePayload) {
                reset();
                emit protocolError(QStringLiteral("Frame payload exceeds maximum allowed size"));
                return;
            }

            m_currentType = static_cast<MsgType>(typeVal);
            m_expectedPayloadLength = payloadLen;
            m_buffer.remove(0, kHeaderSize);
            m_state = ParseState::ReadingPayload;
        }

        if (m_state == ParseState::ReadingPayload) {
            if (static_cast<quint64>(m_buffer.size()) < m_expectedPayloadLength) {
                return;
            }

            const auto len = static_cast<qsizetype>(m_expectedPayloadLength);
            const QByteArray payload = m_buffer.left(len);
            m_buffer.remove(0, len);
            m_state = ParseState::ReadingHeader;
            m_expectedPayloadLength = 0;

            emit packetReceived(m_currentType, payload);
        }
    }
}

} // namespace landrop
