#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/IAudioManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/core/ApplicationCore.h>
#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/Logger.h>
#include <auralis/core/QmlTypeRegistration.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionManager.h>

#include <QtGlobal>

#if defined(Q_OS_LINUX)
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#else
#include <auralis/audio/NativeAudioManager.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>
#endif

#include <QAbstractItemModel>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQmlExtensionPlugin>
#include <QSettings>
#include <QTemporaryDir>
#include <QtQml>
#include <QtTest>

#include <initializer_list>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

namespace {

void verifyProperties(QObject* object, std::initializer_list<const char*> propertyNames)
{
    QVERIFY(object != nullptr);
    for (const char* propertyName : propertyNames) {
        QVERIFY2(
            object->metaObject()->indexOfProperty(propertyName) >= 0,
            qPrintable(QStringLiteral("%1 is missing QML property %2")
                           .arg(QString::fromLatin1(object->metaObject()->className()), QString::fromLatin1(propertyName))));
    }
}

void verifyMethods(QObject* object, std::initializer_list<const char*> signatures)
{
    QVERIFY(object != nullptr);
    for (const char* signature : signatures) {
        const QByteArray normalized = QMetaObject::normalizedSignature(signature);
        QVERIFY2(
            object->metaObject()->indexOfMethod(normalized.constData()) >= 0,
            qPrintable(QStringLiteral("%1 is missing invokable QML endpoint %2")
                           .arg(QString::fromLatin1(object->metaObject()->className()), QString::fromLatin1(normalized))));
    }
}

void verifyRoles(QAbstractItemModel* model, std::initializer_list<const char*> roleNames)
{
    QVERIFY(model != nullptr);
    const QList<QByteArray> actual = model->roleNames().values();
    for (const char* roleName : roleNames) {
        QVERIFY2(
            actual.contains(QByteArray(roleName)),
            qPrintable(QStringLiteral("%1 is missing QML model role %2")
                           .arg(QString::fromLatin1(model->metaObject()->className()), QString::fromLatin1(roleName))));
    }
}

QAbstractItemModel* modelProperty(QObject* object, const char* propertyName)
{
    return object == nullptr ? nullptr : qvariant_cast<QAbstractItemModel*>(object->property(propertyName));
}

QObject* objectProperty(QObject* object, const char* propertyName)
{
    return object == nullptr ? nullptr : qvariant_cast<QObject*>(object->property(propertyName));
}

} // namespace

class TstQmlComponents final : public QObject {
    Q_OBJECT

private slots:
    void completeQmlEndpointContractAndPageLoad()
    {
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));
        auralis::core::Logger::initialize();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        auralis::core::ApplicationServices services;
        services.configuration = std::make_unique<auralis::core::ConfigurationManager>(
            std::make_unique<QSettings>(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat));
#if defined(Q_OS_LINUX)
        services.bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
        auto* bluetoothService = static_cast<auralis::bluetooth::BluetoothManager*>(services.bluetooth.get());
        services.pipeWire = std::make_unique<auralis::audio::PipeWireManager>(bluetoothService->deviceRegistry());
#else
        services.bluetooth = std::make_unique<auralis::bluetooth::NativeBluetoothManager>();
        auto* bluetoothService = static_cast<auralis::bluetooth::NativeBluetoothManager*>(services.bluetooth.get());
        services.pipeWire = std::make_unique<auralis::audio::NativeAudioManager>(bluetoothService->deviceRegistry());
#endif
        services.devices = std::make_unique<auralis::devices::DeviceManager>();
        services.sessions = std::make_unique<auralis::session::SessionManager>(
            services.bluetooth.get(),
            services.pipeWire.get(),
            dir.filePath(QStringLiteral("sessions.json")));

        auralis::core::ApplicationCore core(std::move(services));
        QVERIFY(core.initialize());

        QObject* bluetooth = core.bluetooth();
        QObject* audio = core.audio();
        QObject* router = objectProperty(audio, "router");
        QObject* sessions = core.sessions();
        QObject* configuration = core.configuration();
        QObject* diagnostics = core.diagnostics();
        QObject* notifications = core.notifications();
        QObject* platform = core.platform();
        QObject* selectedSession = objectProperty(sessions, "selectedSession");

        verifyProperties(
            &core,
            {"bluetoothStatus", "bluetooth", "audioStatus", "audio", "coreStatus", "ready", "sessions",
             "configuration", "notifications", "diagnostics", "recovery", "platform", "currentPage",
             "warningCount", "lastErrorText", "activeSessionName", "activeSessionId"});
        verifyMethods(&core, {"navigateTo(int)"});

        verifyProperties(
            bluetooth,
            {"available", "adapterPowered", "scanning", "scanModeText", "deviceCount", "classicDeviceCount",
             "lowEnergyDeviceCount", "connectedDeviceCount", "statusText",
             "errorText", "adapterName", "adapterAddress", "devices", "classicDevices", "lowEnergyDevices",
             "pendingPairingRequest", "agentRegistered"});
        verifyMethods(
            bluetooth,
            {"startScan()", "startLowEnergyScan()", "stopScan()", "refresh()", "pairDevice(QString)", "cancelPairing(QString)",
             "cancelDeviceOperation(QString)", "trustDevice(QString)", "untrustDevice(QString)",
             "connectDevice(QString)", "disconnectDevice(QString)", "reconnectDevice(QString)",
             "forgetDevice(QString)", "setDeviceButtonPolicy(QString,bool)", "serviceFriendlyName(QString)",
             "deviceDetails(QString)", "acceptPairingRequest(QString)", "rejectPairingRequest(QString)",
             "submitPinCode(QString,QString)", "submitPasskey(QString,uint)"});
        verifyRoles(
            modelProperty(bluetooth, "devices"),
            {"objectPath", "displayName", "address", "addressType", "transportHint", "hasRssi", "rssi",
             "paired", "connected", "trusted", "servicesResolved", "operationText", "lastErrorMessage",
             "canPair", "canCancelPairing", "canTrust", "canUntrust", "canConnect", "canDisconnect",
             "canForget", "canReconnect", "canCancelOperation", "uuids", "buttonPolicyText",
             "canControlButtons", "buttonEffectiveStatus"});

        verifyProperties(
            audio,
            {"connectionStateText", "connected", "lastError", "endpointCount", "nodeCount",
             "mappedBluetoothCount", "initialSyncComplete", "graphRevision", "diagnosticsText",
             "virtualOutputAvailable", "virtualOutputSelected", "virtualOutputStatus", "endpoints", "router"});
        verifyMethods(
            audio,
            {"audioStatusForDevice(QString)", "refreshVirtualAudio()", "openWindowsSoundSettings()"});
        verifyRoles(
            modelProperty(audio, "endpoints"),
            {"endpointId", "name", "direction", "available", "transport", "profile", "codec",
             "pipeWireObjectId", "bluetoothAddress", "bluetoothDisplayName", "mapped"});

        verifyProperties(
            router,
            {"sources", "routes", "sourceCount", "currentRouteId", "routeStateText", "lastErrorText",
             "routeEnabled", "routeVolume", "routeMuted", "volumeCapable", "ownedLinkCount"});
        verifyMethods(
            router,
            {"createRoute(QString,QStringList)", "removeRoute(QString)", "activateRoute(QString)",
             "deactivateRoute(QString)", "setRouteSource(QString,QString)",
             "setRouteDestinations(QString,QStringList)", "setDestinationVolume(QString,double)",
             "setDestinationMuted(QString,bool)", "setRouteVolume(QString,double)",
             "setRouteMuted(QString,bool)"});
        verifyRoles(
            modelProperty(router, "sources"),
            {"sourceId", "name", "sourceType", "applicationName", "available", "monitorSource"});
        verifyRoles(
            modelProperty(router, "routes"),
            {"routeId", "sourceName", "destinationCount", "stateText", "errorText", "active", "editable",
             "ownerLabel"});

        verifyProperties(
            sessions,
            {"sessionCount", "currentSessionId", "sessionStateText", "groupVolume", "currentSourceId",
             "currentMuted", "sessionList", "sessionMembers", "currentMembers", "selectedSession"});
        verifyMethods(
            sessions,
            {"createSession(QString)", "deleteSession(QString)", "renameSession(QString,QString)",
             "addDevice(QString,QString)", "removeDevice(QString,QString)", "setSource(QString,QString)",
             "activateSession(QString)", "deactivateSession(QString)", "retrySession(QString)",
             "setGroupVolume(QString,double)", "setSessionMuted(QString,bool)",
             "setDeviceVolume(QString,QString,double)", "setRecoveryPolicy(QString,QString)",
             "restoreLastSession()", "duplicateSession(QString)", "commandResultText(int)"});
        verifyRoles(
            modelProperty(sessions, "sessionList"),
            {"sessionId", "name", "stateLabel", "active", "degraded", "deviceCount", "connectedDeviceCount"});
        verifyRoles(
            modelProperty(sessions, "sessionMembers"),
            {"deviceId", "displayName", "connected", "endpointAvailable", "volume", "muted", "memberEnabled"});
        verifyRoles(
            modelProperty(sessions, "currentMembers"),
            {"displayName", "connected", "recovering", "routeActive", "endpointAvailable"});
        verifyProperties(
            selectedSession,
            {"sessionId", "name", "stateLabel", "sourceId", "sourceName", "groupVolume", "muted",
             "recoveryPolicy", "exists"});

        verifyProperties(
            configuration,
            {"fileLoggingEnabled", "logFilePath", "showDeveloperStatus", "restoreLastSession",
             "autoRecoverServices", "restoreOnResume", "lastNavPage", "windowWidth", "windowHeight",
             "lastErrorText", "fileLoggingEnvLocked", "developerStatusEnvLocked"});
        verifyMethods(
            configuration,
            {"setFileLoggingEnabled(bool)", "setLogFilePath(QString)", "setShowDeveloperStatus(bool)",
             "setRestoreLastSession(bool)", "setAutoRecoverServices(bool)", "setRestoreOnResume(bool)",
             "setWindowWidth(int)", "setWindowHeight(int)", "resetToDefaults()"});

        verifyProperties(diagnostics, {"severityFilter", "categoryFilter", "visibleCount", "captureEnabled"});
        verifyMethods(diagnostics, {"copyVisibleToClipboard()", "clear()"});
        verifyRoles(qobject_cast<QAbstractItemModel*>(diagnostics), {"timestamp", "severity", "category", "message"});
        verifyMethods(
            notifications,
            {"postInfo(QString,QString)", "postWarning(QString,QString)", "postError(QString,QString)",
             "dismiss(QString)"});
        verifyRoles(
            qobject_cast<QAbstractItemModel*>(notifications),
            {"notificationId", "severity", "title", "message", "sticky", "timestamp"});
        verifyProperties(platform, {"operatingSystem", "bluetoothBackend", "audioBackend", "powerBackend", "desktop"});

        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QStringList qmlWarnings;
        QQmlApplicationEngine engine;
        connect(&engine, &QQmlEngine::warnings, &engine, [&qmlWarnings](const QList<QQmlError>& warnings) {
            for (const QQmlError& warning : warnings) {
                qmlWarnings.push_back(warning.toString());
            }
        });
        engine.loadFromModule("Auralis.Ui", "Main");
        QVERIFY2(!engine.rootObjects().isEmpty(), "Complete QML shell failed to load");

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(1100, 760);
        window->show();

        const QStringList pageNames{
            QStringLiteral("pageDashboard"),
            QStringLiteral("pageDevices"),
            QStringLiteral("pageSessions"),
            QStringLiteral("pageAudioRouting"),
            QStringLiteral("pageDiagnostics"),
            QStringLiteral("pageSettings"),
        };
        for (int page = 0; page < pageNames.size(); ++page) {
            core.navigateTo(page);
            QCOMPARE(core.currentPage(), page);
            QObject* pageObject = nullptr;
            QTRY_VERIFY_WITH_TIMEOUT(
                (pageObject = window->findChild<QObject*>(pageNames.at(page))) != nullptr,
                2000);
            QVERIFY(pageObject->property("width").toReal() > 0.0);
            QVERIFY(pageObject->property("height").toReal() > 0.0);
        }

        window->resize(880, 600);
        QTest::qWait(100);
        for (const QString& pageName : pageNames) {
            QObject* pageObject = window->findChild<QObject*>(pageName);
            QVERIFY(pageObject != nullptr);
            QVERIFY(pageObject->property("width").toReal() > 0.0);
            QVERIFY(pageObject->property("height").toReal() > 0.0);
        }

        auto* focusableAction =
            qobject_cast<QQuickItem*>(window->findChild<QObject*>(QStringLiteral("settingsResetButton")));
        QVERIFY(focusableAction != nullptr);
        focusableAction->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY_WITH_TIMEOUT(focusableAction->property("visualFocus").toBool(), 1000);

        QObject* routeActivate = window->findChild<QObject*>(QStringLiteral("routePlannerActivate"));
        QVERIFY(routeActivate != nullptr);
        QCOMPARE(routeActivate->property("enabled").toBool(), false);

        QObject* sourceSelector = window->findChild<QObject*>(QStringLiteral("routeSourceSelector"));
        const QList<QObject*> destinationOptions =
            window->findChildren<QObject*>(QStringLiteral("routeDestinationOption"));
        if (sourceSelector != nullptr && !destinationOptions.isEmpty()
            && router->property("sourceCount").toInt() > 0) {
            sourceSelector->setProperty("currentIndex", 0);
            destinationOptions.constFirst()->setProperty("checked", true);
            QTRY_COMPARE_WITH_TIMEOUT(routeActivate->property("enabled").toBool(), true, 1000);
            destinationOptions.constFirst()->setProperty("checked", false);
            QTRY_COMPARE_WITH_TIMEOUT(routeActivate->property("enabled").toBool(), false, 1000);
        }

        const QStringList comboNames{
            QStringLiteral("routeSourceSelector"),
            QStringLiteral("sessionPolicySelector"),
            QStringLiteral("diagnosticsSeveritySelector"),
        };
        for (const QString& comboName : comboNames) {
            QObject* combo = window->findChild<QObject*>(comboName);
            QVERIFY2(combo != nullptr, qPrintable(QStringLiteral("Missing styled dropdown %1").arg(comboName)));
            QObject* popup = qvariant_cast<QObject*>(combo->property("popup"));
            QVERIFY(popup != nullptr);
            QVERIFY(QMetaObject::invokeMethod(popup, "open"));
            QTest::qWait(30);
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        }

        QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.join(QLatin1Char('\n'))));

        core.shutdown();
        auralis::core::Logger::shutdown();
    }
};

QTEST_MAIN(TstQmlComponents)
#include "tst_QmlComponents.moc"
