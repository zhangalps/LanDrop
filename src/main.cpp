#include "core/DeviceDiscovery.h"
#include "core/session/GroupManager.h"
#include "core/session/TransferManager.h"
#include "platform/AndroidHelper.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QTimer>
#include <QUuid>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LanDrop"));
    app.setOrganizationName(QStringLiteral("LanDrop"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/landrop.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    landrop::AndroidHelper platformHelper;
    platformHelper.acquireMulticastLock();

    QSettings settings;
    QString localId = settings.value(QStringLiteral("identity/deviceId")).toString();
    if (localId.isEmpty()) {
        localId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("identity/deviceId"), localId);
    }

    const QString defaultName = landrop::AndroidHelper::localDeviceName();
    QString localName = settings.value(QStringLiteral("identity/deviceName")).toString().trimmed();
    if (localName.isEmpty()) {
        localName = defaultName;
    }
    const QString localPlatform = landrop::AndroidHelper::platformName();

    landrop::TransferManager transferManager(localId, localName, localPlatform);
    if (!transferManager.startServer(landrop::kDefaultTcpPort)) {
        transferManager.startServer(0);
    }

    landrop::DeviceDiscovery discovery(
        localId, localName, defaultName, localPlatform, transferManager.serverPort());

    landrop::GroupManager groupManager(localId, localName, localPlatform, &transferManager);

    // 1v1 session status & live rename sync
    QObject::connect(
        &transferManager, &landrop::TransferManager::peerStatusUpdated,
        &discovery, &landrop::DeviceDiscovery::syncSessionStatus);
    QObject::connect(
        &transferManager, &landrop::TransferManager::peerInfoUpdated,
        &discovery, &landrop::DeviceDiscovery::updateDeviceInfo);

    // Ensure 1v1 and Group active selections stay mutually exclusive
    QObject::connect(
        &transferManager, &landrop::TransferManager::activePeerChanged,
        &groupManager, [&transferManager, &groupManager]() {
            if (!transferManager.activePeerId().isEmpty()) {
                groupManager.clearActiveGroup();
            }
        });

    // Local device rename -> persist + sync across Discovery, 1v1 Sessions, and Group Rooms
    QObject::connect(
        &discovery, &landrop::DeviceDiscovery::localDeviceNameChanged,
        &app, [&transferManager, &groupManager, defaultName](const QString &newName) {
            QSettings s;
            if (newName == defaultName) {
                s.remove(QStringLiteral("identity/deviceName"));
            } else {
                s.setValue(QStringLiteral("identity/deviceName"), newName);
            }
            transferManager.setLocalDeviceName(newName);
            groupManager.setLocalDeviceName(newName);
        });

    // Group discovery & incoming join routing
    QObject::connect(
        &transferManager, &landrop::TransferManager::incomingGroupJoin,
        &groupManager, &landrop::GroupManager::onIncomingGroupJoin);
    QObject::connect(
        &groupManager, &landrop::GroupManager::hostedGroupsUpdated,
        &discovery, &landrop::DeviceDiscovery::setHostedGroups);
    QObject::connect(
        &groupManager, &landrop::GroupManager::groupStatusUpdated,
        &discovery, &landrop::DeviceDiscovery::syncGroupMembership);
    QObject::connect(
        &groupManager, &landrop::GroupManager::groupRemoved,
        &discovery, &landrop::DeviceDiscovery::removeHostedGroup);

    discovery.start(landrop::kDiscoveryPort);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("deviceDiscovery"), &discovery);
    engine.rootContext()->setContextProperty(QStringLiteral("transferManager"), &transferManager);
    engine.rootContext()->setContextProperty(QStringLiteral("groupManager"), &groupManager);
    engine.rootContext()->setContextProperty(QStringLiteral("platformHelper"), &platformHelper);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("LanDrop"), QStringLiteral("Main"));

    QTimer::singleShot(300, &platformHelper, [&platformHelper]() {
        platformHelper.requestLocalNetworkPermission();
        platformHelper.startKeepAliveService();
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &platformHelper, &landrop::AndroidHelper::stopKeepAliveService);

    // When returning from background (e.g. after viewing a file in WPS), refresh discovery
    // and silently reconnect any 1v1/group sessions frozen by the mobile OS.
    QObject::connect(
        &app, &QGuiApplication::applicationStateChanged,
        &app, [&platformHelper, &discovery, &transferManager, &groupManager](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive) {
                platformHelper.acquireMulticastLock();
                platformHelper.startKeepAliveService();
                discovery.refreshNow();
                transferManager.reconnectActiveSessions();
                groupManager.reconnectDroppedGroups();
            }
        });

    return app.exec();
}
