#include "core/DeviceDiscovery.h"
#include "core/PacketCodec.h"
#include "core/Protocol.h"
#include "core/session/GroupManager.h"
#include "core/session/TransferManager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

using namespace landrop;

namespace {

QByteArray sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

} // namespace

class LanDropTests : public QObject
{
    Q_OBJECT

private slots:
    void testCodecHeaderFormatAndSinglePacket()
    {
        const QByteArray payload = QByteArrayLiteral("hello landrop");
        const QByteArray frame = PacketCodec::encode(MsgType::TextMessage, payload);

        QCOMPARE(frame.size(), kHeaderSize + payload.size());
        QCOMPARE(qFromBigEndian<quint32>(frame.constData()), kProtocolMagic);
        QCOMPARE(static_cast<quint8>(frame.at(4)), static_cast<quint8>(MsgType::TextMessage));
        QCOMPARE(qFromBigEndian<quint64>(frame.constData() + 5), static_cast<quint64>(payload.size()));

        PacketCodec codec;
        QList<QPair<MsgType, QByteArray>> received;
        connect(&codec, &PacketCodec::packetReceived, this, [&](MsgType t, const QByteArray &p) {
            received.append({t, p});
        });

        codec.feed(frame);
        QCOMPARE(received.size(), 1);
        QCOMPARE(received.at(0).first, MsgType::TextMessage);
        QCOMPARE(received.at(0).second, payload);
    }

    void testCodecStickyAndByteByByteFragmentation()
    {
        const QByteArray f1 = PacketCodec::encode(MsgType::HandshakeReq, QByteArrayLiteral("{\"a\":1}"));
        const QByteArray f2 = PacketCodec::encode(MsgType::TextMessage, QByteArrayLiteral("{\"b\":2}"));
        const QByteArray f3 = PacketCodec::encode(MsgType::Heartbeat, QByteArray());

        const QByteArray combined = f1 + f2 + f3;

        // 1) All concatenated in one feed (sticky packets)
        {
            PacketCodec codec;
            QList<QPair<MsgType, QByteArray>> received;
            connect(&codec, &PacketCodec::packetReceived, this, [&](MsgType t, const QByteArray &p) {
                received.append({t, p});
            });
            codec.feed(combined);
            QCOMPARE(received.size(), 3);
            QCOMPARE(received.at(0).first, MsgType::HandshakeReq);
            QCOMPARE(received.at(1).first, MsgType::TextMessage);
            QCOMPARE(received.at(2).first, MsgType::Heartbeat);
            QVERIFY(received.at(2).second.isEmpty());
        }

        // 2) Byte-by-byte fragmentation
        {
            PacketCodec codec;
            QList<QPair<MsgType, QByteArray>> received;
            connect(&codec, &PacketCodec::packetReceived, this, [&](MsgType t, const QByteArray &p) {
                received.append({t, p});
            });
            for (int i = 0; i < combined.size(); ++i) {
                codec.feed(combined.mid(i, 1));
            }
            QCOMPARE(received.size(), 3);
            QCOMPARE(received.at(0).second, QByteArrayLiteral("{\"a\":1}"));
            QCOMPARE(received.at(1).second, QByteArrayLiteral("{\"b\":2}"));
            QCOMPARE(received.at(2).second, QByteArray());
        }
    }

    void testCodecInvalidMagicAndOversizedPayload()
    {
        {
            PacketCodec codec;
            QSignalSpy errSpy(&codec, &PacketCodec::protocolError);
            QByteArray bad(kHeaderSize, '\0');
            qToBigEndian<quint32>(0x12345678U, bad.data());
            codec.feed(bad);
            QCOMPARE(errSpy.count(), 1);
        }

        {
            PacketCodec codec;
            QSignalSpy errSpy(&codec, &PacketCodec::protocolError);
            QByteArray huge(kHeaderSize, '\0');
            qToBigEndian<quint32>(kProtocolMagic, huge.data());
            huge[4] = static_cast<char>(MsgType::FileChunk);
            qToBigEndian<quint64>(kMaxFramePayload + 1, huge.data() + 5);
            codec.feed(huge);
            QCOMPARE(errSpy.count(), 1);
        }
    }

    void testHandshakeReject()
    {
        TransferManager alice(QStringLiteral("id-alice"), QStringLiteral("Alice"), QStringLiteral("windows"));
        TransferManager bob(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"));
        QVERIFY(alice.startServer(0));
        QVERIFY(bob.startServer(0));

        QSignalSpy incomingSpy(&bob, &TransferManager::incomingRequestChanged);
        QSignalSpy rejectSpy(&alice, &TransferManager::handshakeRejected);

        alice.connectToDevice(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"),
                              QStringLiteral("127.0.0.1"), bob.serverPort());

        QTRY_VERIFY_WITH_TIMEOUT(bob.hasIncomingRequest(), 3000);
        QCOMPARE(bob.incomingPeerId(), QStringLiteral("id-alice"));

        bob.rejectIncomingRequest(QStringLiteral("id-alice"));
        QTRY_COMPARE_WITH_TIMEOUT(rejectSpy.count(), 1, 3000);
        QCOMPARE(alice.activePeerState(), static_cast<int>(SessionState::Disconnected));
    }

    void testEndToEndTextAndStreamingLargeFile()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // Generate a 10 MB random file
        const QString sourcePath = tempDir.filePath(QStringLiteral("sample_10mb.bin"));
        {
            QFile src(sourcePath);
            QVERIFY(src.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QByteArray block(64 * 1024, '\0');
            auto *rng = QRandomGenerator::global();
            for (int i = 0; i < 160; ++i) { // 160 * 64KB = 10MB
                auto *words = reinterpret_cast<quint32 *>(block.data());
                for (int w = 0; w < block.size() / 4; ++w) {
                    words[w] = rng->generate();
                }
                QCOMPARE(src.write(block), block.size());
            }
            src.close();
        }
        const QByteArray expectedHash = sha256OfFile(sourcePath);
        QVERIFY(!expectedHash.isEmpty());

        const QString recvDir = tempDir.filePath(QStringLiteral("received"));

        TransferManager alice(QStringLiteral("id-alice"), QStringLiteral("Alice"), QStringLiteral("windows"));
        TransferManager bob(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"));
        bob.setDownloadDirectory(recvDir);

        QVERIFY(alice.startServer(0));
        QVERIFY(bob.startServer(0));

        alice.connectToDevice(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"),
                              QStringLiteral("127.0.0.1"), bob.serverPort());

        QTRY_VERIFY_WITH_TIMEOUT(bob.hasIncomingRequest(), 3000);
        bob.acceptIncomingRequest(QStringLiteral("id-alice"));

        QTRY_COMPARE_WITH_TIMEOUT(alice.activePeerState(), static_cast<int>(SessionState::Connected), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(bob.activePeerState(), static_cast<int>(SessionState::Connected), 3000);

        QSignalSpy bobFileDoneSpy(&bob, &TransferManager::fileTransferCompleted);

        // Start streaming 10MB file and interleave a text message during transfer
        alice.sendFiles({QUrl::fromLocalFile(sourcePath)});
        alice.sendText(QStringLiteral("Interleaved text during 10MB file transfer"));

        QTRY_COMPARE_WITH_TIMEOUT(bobFileDoneSpy.count(), 1, 15000);

        const QString savedPath = bobFileDoneSpy.at(0).at(2).toString();
        QVERIFY(QFile::exists(savedPath));
        QCOMPARE(QFileInfo(savedPath).size(), QFileInfo(sourcePath).size());
        QCOMPARE(sha256OfFile(savedPath), expectedHash);

        // Verify interleaved text message also arrived on Bob's session
        PeerSession *bobSession = bob.sessionForPeer(QStringLiteral("id-alice"));
        QVERIFY(bobSession != nullptr);
        auto *bobModel = bobSession->messages();
        QTRY_VERIFY_WITH_TIMEOUT(bobModel->rowCount() >= 2, 3000);

        bool foundText = false;
        for (int i = 0; i < bobModel->rowCount(); ++i) {
            const QModelIndex idx = bobModel->index(i);
            if (bobModel->data(idx, MessageListModel::KindRole).toString() == QLatin1String("text")
                && bobModel->data(idx, MessageListModel::TextRole).toString()
                       == QLatin1String("Interleaved text during 10MB file transfer")) {
                foundText = true;
            }
        }
        QVERIFY(foundText);
    }

    void testLiveDeviceRenameSync()
    {
        TransferManager alice(QStringLiteral("id-alice"), QStringLiteral("Alice"), QStringLiteral("windows"));
        TransferManager bob(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"));
        QVERIFY(alice.startServer(0));
        QVERIFY(bob.startServer(0));

        alice.connectToDevice(QStringLiteral("id-bob"), QStringLiteral("Bob"), QStringLiteral("android"),
                              QStringLiteral("127.0.0.1"), bob.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(bob.hasIncomingRequest(), 3000);
        bob.acceptIncomingRequest(QStringLiteral("id-alice"));

        QTRY_COMPARE_WITH_TIMEOUT(alice.activePeerState(), static_cast<int>(SessionState::Connected), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(bob.activePeerState(), static_cast<int>(SessionState::Connected), 3000);

        QSignalSpy bobRenameSpy(&bob, &TransferManager::peerInfoUpdated);
        alice.setLocalDeviceName(QStringLiteral("Alice-Office"));
        QTRY_COMPARE_WITH_TIMEOUT(bobRenameSpy.count(), 1, 3000);
        QCOMPARE(bob.activePeerName(), QStringLiteral("Alice-Office"));
    }

    void testGroupChatHostRelayAndDirectFileFetch()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString srcFilePath = tempDir.filePath(QStringLiteral("group_doc.bin"));
        {
            QFile f(srcFilePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QByteArray data(256 * 1024, 'G'); // 256 KB
            QCOMPARE(f.write(data), data.size());
            f.close();
        }
        const QByteArray expectedHash = sha256OfFile(srcFilePath);

        TransferManager hostTm(QStringLiteral("id-host"), QStringLiteral("HostPC"), QStringLiteral("windows"));
        TransferManager memberTm(QStringLiteral("id-mem"), QStringLiteral("PhoneA"), QStringLiteral("android"));
        memberTm.setDownloadDirectory(tempDir.filePath(QStringLiteral("member_dl")));
        QVERIFY(hostTm.startServer(0));
        QVERIFY(memberTm.startServer(0));

        GroupManager hostGm(QStringLiteral("id-host"), QStringLiteral("HostPC"), QStringLiteral("windows"), &hostTm);
        GroupManager memberGm(QStringLiteral("id-mem"), QStringLiteral("PhoneA"), QStringLiteral("android"), &memberTm);

        QObject::connect(&hostTm, &TransferManager::incomingGroupJoin,
                         &hostGm, &GroupManager::onIncomingGroupJoin);
        QObject::connect(&memberTm, &TransferManager::incomingGroupJoin,
                         &memberGm, &GroupManager::onIncomingGroupJoin);

        const QString gid = hostGm.createGroup(QStringLiteral("ProjectRoom"), QStringLiteral("1234"));
        QVERIFY(!gid.isEmpty());
        QCOMPARE(hostGm.activeGroupMemberCount(), 1);

        // Wrong PIN should be rejected
        QSignalSpy rejectSpy(&memberGm, &GroupManager::groupNotice);
        memberGm.joinGroup(gid, QStringLiteral("ProjectRoom"), QStringLiteral("id-host"), QStringLiteral("HostPC"),
                           QStringLiteral("127.0.0.1"), hostTm.serverPort(), QStringLiteral("0000"));
        QTRY_COMPARE_WITH_TIMEOUT(rejectSpy.count(), 1, 3000);
        QVERIFY(!memberGm.activeGroupIsJoined());

        // Correct PIN should join cleanly
        memberGm.joinGroup(gid, QStringLiteral("ProjectRoom"), QStringLiteral("id-host"), QStringLiteral("HostPC"),
                           QStringLiteral("127.0.0.1"), hostTm.serverPort(), QStringLiteral("1234"));
        QTRY_VERIFY_WITH_TIMEOUT(memberGm.activeGroupIsJoined(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(hostGm.activeGroupMemberCount(), 2, 3000);
        QCOMPARE(memberGm.activeGroupMemberCount(), 2);

        // Group text relay
        memberGm.sendText(QStringLiteral("Hello Group!"));
        auto *hostMsgs = qobject_cast<MessageListModel *>(hostGm.activeGroupMessageModel());
        QVERIFY(hostMsgs != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(hostMsgs->rowCount() >= 3, 3000); // created + joined + text

        // Group file announce + on-demand 64KB direct fetch
        hostGm.sendFiles({QUrl::fromLocalFile(srcFilePath)});
        auto *memberMsgs = qobject_cast<MessageListModel *>(memberGm.activeGroupMessageModel());
        QVERIFY(memberMsgs != nullptr);

        QString announcedTransferId;
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            for (int i = 0; i < memberMsgs->rowCount(); ++i) {
                const QModelIndex idx = memberMsgs->index(i);
                if (memberMsgs->data(idx, MessageListModel::KindRole).toString() == QLatin1String("file")) {
                    announcedTransferId = memberMsgs->data(idx, MessageListModel::MsgIdRole).toString();
                    return true;
                }
            }
            return false;
        }(), 3000);

        memberGm.downloadGroupFile(announcedTransferId);
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            const ChatMessage *m = memberMsgs->findMessage(announcedTransferId);
            return m && m->status == QLatin1String("completed");
        }(), 5000);

        const ChatMessage *downloaded = memberMsgs->findMessage(announcedTransferId);
        QVERIFY(downloaded != nullptr);
        QCOMPARE(sha256OfFile(downloaded->filePath), expectedHash);

        // Disband group
        QSignalSpy disbandSpy(&memberGm, &GroupManager::groupNotice);
        hostGm.leaveOrDisbandActiveGroup();
        QTRY_VERIFY_WITH_TIMEOUT(disbandSpy.count() >= 1, 3000);
        QVERIFY(!memberGm.activeGroupIsJoined());
    }

    void testDeviceOnlineOfflineToggle()
    {
        constexpr quint16 kTestPort = 45499;
        DeviceDiscovery alice(QStringLiteral("dev-a"), QStringLiteral("AlicePC"), QStringLiteral("AlicePC"),
                              QStringLiteral("windows"), 45501);
        DeviceDiscovery bob(QStringLiteral("dev-b"), QStringLiteral("BobPhone"), QStringLiteral("BobPhone"),
                            QStringLiteral("android"), 45502);
        alice.start(kTestPort);
        bob.start(kTestPort);

        QTRY_COMPARE_WITH_TIMEOUT(bob.rowCount(), 1, 3000);

        // Alice goes offline -> Bob removes Alice immediately without waiting 6s timeout
        alice.setDiscoverable(false);
        QVERIFY(!alice.isOnline());
        QTRY_COMPARE_WITH_TIMEOUT(bob.rowCount(), 0, 2000);

        // Alice goes back online -> Bob rediscovers Alice immediately
        alice.setDiscoverable(true);
        QVERIFY(alice.isOnline());
        QTRY_COMPARE_WITH_TIMEOUT(bob.rowCount(), 1, 3000);

        alice.stop();
        bob.stop();
    }
};

QTEST_MAIN(LanDropTests)
#include "test_landrop.moc"
