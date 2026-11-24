#pragma once

#include "core/Protocol.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

namespace landrop {

class PacketCodec : public QObject
{
    Q_OBJECT

public:
    explicit PacketCodec(QObject *parent = nullptr);

    static QByteArray encode(MsgType type, const QByteArray &payload);
    static QByteArray encodeJson(MsgType type, const QJsonObject &object);

    void feed(const QByteArray &data);
    void reset();

signals:
    void packetReceived(landrop::MsgType type, const QByteArray &payload);
    void protocolError(const QString &reason);

private:
    enum class ParseState {
        ReadingHeader,
        ReadingPayload
    };

    ParseState m_state = ParseState::ReadingHeader;
    QByteArray m_buffer;
    MsgType m_currentType = MsgType::Heartbeat;
    quint64 m_expectedPayloadLength = 0;
};

} // namespace landrop
