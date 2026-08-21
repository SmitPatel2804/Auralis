#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/bluetooth/BlueZDbusClient.h>
#include <auralis/core/Logger.h>
#include <auralis/recovery/RecoveryManager.h>
#include <auralis/session/AuralisSession.h>
#include <auralis/session/SessionPersistence.h>
#include <auralis/session/SessionTypes.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using auralis::audio::PipeWireClientEvent;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::bluetooth::BlueZDbusClient;
using auralis::recovery::RecoveryManager;
using auralis::session::AuralisSession;
using auralis::session::SessionPersistence;
using auralis::session::SessionPersistenceDocument;
using auralis::session::SessionState;

namespace {

int stressIterations()
{
    static const int resolved = []() {
        constexpr int kDefaultModest = 100;
        constexpr int kExtended = 1000;
        constexpr int kMin = 1;
        constexpr int kMax = 100000;

        int iterations = kDefaultModest;
        if (qEnvironmentVariableIsSet("AURALIS_RUN_STRESS")
            && qEnvironmentVariableIntValue("AURALIS_RUN_STRESS") != 0) {
            // AURALIS_RUN_STRESS=1 means "run extended stress", not "1 iteration".
            iterations = kExtended;
        }
        if (qEnvironmentVariableIsSet("AURALIS_STRESS_ITERATIONS")) {
            iterations = qEnvironmentVariableIntValue("AURALIS_STRESS_ITERATIONS");
        }
        if (iterations < kMin) {
            iterations = kMin;
        } else if (iterations > kMax) {
            iterations = kMax;
        }
        qInfo("Phase8Stress iterations=%d", iterations);
        return iterations;
    }();
    return resolved;
}

PipeWireClientEvent stateEvent(PipeWireConnectionState state)
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::StateChanged;
    event.state = state;
    return event;
}

PipeWireClientEvent syncDoneEvent()
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::InitialSyncDone;
    event.state = PipeWireConnectionState::Connected;
    return event;
}

} // namespace

class TstPhase8Stress : public QObject {
    Q_OBJECT

private slots:
    void blueZBusChurnStress()
    {
        BlueZDbusClient client;
        client.setSystemBusConnectedOverrideForTesting(true);
        QVERIFY(client.initialize());
        const int cycles = stressIterations();
        for (int i = 0; i < cycles; ++i) {
            client.setSystemBusConnectedOverrideForTesting(false);
            client.pollSystemBusHealthForTesting();
            QVERIFY(!client.isSystemBusConnected());
            client.setSystemBusConnectedOverrideForTesting(true);
            client.pollSystemBusHealthForTesting();
            QVERIFY(client.isSystemBusConnected());
            client.setBlueZAvailableForTesting(true);
            client.requestSnapshot();
            client.requestSnapshot();
            const quint64 gen = static_cast<quint64>(client.busAttachGenerationForTesting());
            QVariantMap objects;
            objects.insert(QStringLiteral("/o"), QVariantMap{});
            client.injectSnapshotFinishedForTesting(gen, objects);
            if (client.snapshotInFlightForTesting()) {
                client.injectSnapshotFinishedForTesting(gen, objects);
            }
        }
        QVERIFY(client.busHealthTimerActiveForTesting());
        client.shutdown();
    }

    void pipeWireGraphChurnStress()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(8);
        manager.setInitialSyncTimeoutMsForTesting(5);
        const int cycles = stressIterations();
        for (int i = 0; i < cycles; ++i) {
            manager.simulateReconnectFailureForTesting(QStringLiteral("churn"));
            manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
            if ((i % 3) == 0) {
                manager.fireInitialSyncTimeoutForTesting();
            } else {
                manager.injectClientEventForTesting(syncDoneEvent());
            }
        }
        manager.shutdown();
        QCOMPARE(manager.reconnectAttempt(), 0);
    }

    void suspendResumeStress()
    {
        RecoveryManager manager;
        int pause = 0;
        int resume = 0;
        RecoveryManager::HostHooks hooks;
        hooks.pauseBluetoothReconnect = [&]() { ++pause; };
        hooks.resumeBluetoothReconnect = [&]() { ++resume; };
        hooks.refreshActiveSession = []() {};
        hooks.requestBlueZRefresh = []() {};
        hooks.isBlueZAvailable = []() { return true; };
        hooks.isPipeWireConnected = []() { return true; };
        hooks.isPipeWireGraphReady = []() { return true; };
        manager.setHooks(std::move(hooks));
        QVERIFY(manager.initialize());
        const int cycles = stressIterations();
        for (int i = 0; i < cycles; ++i) {
            manager.onPreparingForSleep(true);
            manager.onPreparingForSleep(true);
            manager.onPreparingForSleep(false);
            manager.onPreparingForSleep(false);
            manager.flushPendingReconcileForTesting();
        }
        QCOMPARE(pause, cycles);
        QCOMPARE(resume, cycles);
        manager.shutdown();
    }

    void sessionPersistenceStress()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionPersistence persistence(dir.filePath(QStringLiteral("sessions.json")));
        const int cycles = stressIterations();
        for (int i = 0; i < cycles; ++i) {
            SessionPersistenceDocument document;
            AuralisSession session;
            session.id = QStringLiteral("s-%1").arg(i % 7);
            session.name = QStringLiteral("Session");
            session.state = SessionState::Idle;
            document.sessions.push_back(session);
            QVERIFY(persistence.save(document));
            const SessionPersistenceDocument loaded = persistence.load();
            QCOMPARE(loaded.sessions.size(), 1);
            QCOMPARE(loaded.sessions.front().id, session.id);
        }
    }

    void loggerRotationStress()
    {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("stress.log"));
        auralis::core::Logger::Options options;
        options.enableConsole = false;
        options.enableFile = true;
        options.filePath = path;
        options.maxFileBytes = 128;
        options.retainRotatedFiles = 3;
        QVERIFY(auralis::core::Logger::initialize(options));
        if (!auralis::core::Logger::isFileLoggingActive()) {
            QVERIFY(auralis::core::Logger::enableFileLogging(path));
        }
        const int cycles = stressIterations() * 5;
        for (int i = 0; i < cycles; ++i) {
            qInfo("stress-line-%d-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", i);
        }
        auralis::core::Logger::shutdown();
        QVERIFY(QFile::exists(path) || QFile::exists(path + QStringLiteral(".1")));
#else
        QSKIP("File logging disabled in this build");
#endif
    }
};

QTEST_GUILESS_MAIN(TstPhase8Stress)
#include "tst_Phase8Stress.moc"
