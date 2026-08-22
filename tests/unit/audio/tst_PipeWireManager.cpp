#include "AudioTestFixtures.h"

#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/core/ServiceStatus.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::audio::PipeWireClientEvent;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::core::ServiceStatus;
using auralis::test::makeNode;
using auralis::test::makePort;

namespace {

PipeWireClientEvent stateEvent(PipeWireConnectionState state, const QString& error = {})
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::StateChanged;
    event.state = state;
    event.error = error;
    return event;
}

PipeWireClientEvent syncDoneEvent()
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::InitialSyncDone;
    event.state = PipeWireConnectionState::Connected;
    return event;
}

PipeWireClientEvent graphEvent(const auralis::audio::PipeWireObjectSnapshot& snapshot)
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::GlobalAdded;
    event.snapshot = snapshot;
    return event;
}

PipeWireClientEvent defaultSinkEvent(const QString& nodeName)
{
    PipeWireClientEvent event;
    event.type = PipeWireClientEvent::Type::MetadataChanged;
    event.metadataName = QStringLiteral("default");
    event.metadataKey = QStringLiteral("default.audio.sink");
    event.metadataType = QStringLiteral("Spa:String:JSON");
    event.metadataValue = QStringLiteral(R"({"name":"%1"})").arg(nodeName);
    return event;
}

} // namespace

class TstPipeWireManager : public QObject {
    Q_OBJECT

private slots:
    void lifecycleWithoutRequiringLiveServer()
    {
        PipeWireManager manager;
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
        const bool started = manager.initialize();
        if (started) {
            QVERIFY(manager.status() != ServiceStatus::Uninitialized);
        } else {
            QVERIFY(manager.status() == ServiceStatus::Error);
            QVERIFY(manager.connectionState() == PipeWireConnectionState::Error);
            QVERIFY(!manager.lastError().isEmpty());
        }
        manager.shutdown();
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
        manager.shutdown();
    }

    void repeatedStartStopIsSafe()
    {
        PipeWireManager manager;
        manager.initialize();
        manager.shutdown();
        manager.initialize();
        manager.shutdown();
        QCOMPARE(manager.endpointCount(), 0);
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
        QVERIFY(manager.connectionState() == PipeWireConnectionState::Stopped);
    }

    void reconnectAttemptNotResetAtTransportConnected()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(5);
        manager.simulateReconnectFailureForTesting(QStringLiteral("down"));
        QVERIFY(manager.reconnectAttempt() >= 1);
        const int before = manager.reconnectAttempt();
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QCOMPARE(manager.reconnectAttempt(), before);
    }

    void reconnectAttemptResetsOnlyAfterGraphReady()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        QVERIFY(manager.reconnectAttempt() >= 1);
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.reconnectAttempt() >= 1);
        manager.injectClientEventForTesting(syncDoneEvent());
        QCOMPARE(manager.reconnectAttempt(), 0);
        QVERIFY(manager.initialSyncComplete());
    }

    void boundedRetryExhaustsAfterConfiguredMaximum()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(3);
        QSignalSpy exhausted(&manager, &PipeWireManager::reconnectExhausted);

        for (int i = 0; i < 5; ++i) {
            manager.simulateReconnectFailureForTesting(QStringLiteral("fail"));
        }
        QCOMPARE(exhausted.count(), 1);
        QCOMPARE(manager.reconnectAttempt(), 3);
        manager.simulateReconnectFailureForTesting(QStringLiteral("fail"));
        QCOMPARE(exhausted.count(), 1);
    }

    void disableAutoReconnectStopsFurtherScheduling()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        const int attempts = manager.reconnectAttempt();
        manager.setAutoReconnectEnabled(false);
        manager.simulateReconnectFailureForTesting(QStringLiteral("y"));
        QCOMPARE(manager.reconnectAttempt(), attempts);
    }

    void manualReconnectAfterExhaustionStartsFreshEpisode()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(2);
        for (int i = 0; i < 4; ++i) {
            manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        }
        QCOMPARE(manager.reconnectAttempt(), 2);
        manager.setAutoReconnectEnabled(false);
        manager.requestReconnect();
        // Fresh episode: attempt counter cleared when starting manual reconnect after exhaustion.
        QCOMPARE(manager.reconnectAttempt(), 0);
    }

    void shutdownCancelsScheduledReconnect()
    {
        PipeWireManager manager;
        manager.initialize();
        manager.setAutoReconnectEnabled(true);
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        manager.shutdown();
        QCOMPARE(manager.reconnectAttempt(), 0);
        QVERIFY(manager.status() == ServiceStatus::Uninitialized);
    }

    void initialSyncTimeoutTriggersNextRetry()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(5);
        manager.setInitialSyncTimeoutMsForTesting(5);
        manager.simulateReconnectFailureForTesting(QStringLiteral("down"));
        const int before = manager.reconnectAttempt();
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.initialSyncTimeoutPendingForTesting());
        manager.fireInitialSyncTimeoutForTesting();
        QVERIFY(manager.reconnectAttempt() > before);
        QVERIFY(!manager.initialSyncComplete());
    }

    void graphNeverReadyEventuallyExhausts()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(2);
        manager.setInitialSyncTimeoutMsForTesting(5);
        QSignalSpy exhausted(&manager, &PipeWireManager::reconnectExhausted);

        manager.simulateReconnectFailureForTesting(QStringLiteral("a"));
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        manager.fireInitialSyncTimeoutForTesting();
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        manager.fireInitialSyncTimeoutForTesting();
        QCOMPARE(exhausted.count(), 1);
    }

    void initialSyncDoneCancelsTimeout()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setInitialSyncTimeoutMsForTesting(50);
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.initialSyncTimeoutPendingForTesting());
        manager.injectClientEventForTesting(syncDoneEvent());
        QVERIFY(!manager.initialSyncTimeoutPendingForTesting());
        QCOMPARE(manager.reconnectAttempt(), 0);
    }

    void shutdownCancelsInitialSyncTimeout()
    {
        PipeWireManager manager;
        manager.initialize();
        manager.setAutoReconnectEnabled(true);
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.initialSyncTimeoutPendingForTesting());
        manager.shutdown();
        QVERIFY(!manager.initialSyncTimeoutPendingForTesting());
    }

    void disableAutoReconnectCancelsInitialSyncTimeout()
    {
        PipeWireManager manager;
        manager.initialize();
        manager.setAutoReconnectEnabled(true);
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.initialSyncTimeoutPendingForTesting());
        manager.setAutoReconnectEnabled(false);
        QVERIFY(!manager.initialSyncTimeoutPendingForTesting());
    }

    void lateInitialSyncAfterTimeoutIsIgnored()
    {
        PipeWireManager manager;
        manager.setAutoReconnectEnabled(true);
        manager.initialize();
        manager.setMaxReconnectAttemptsForTesting(5);
        manager.simulateReconnectFailureForTesting(QStringLiteral("x"));
        const int attempts = manager.reconnectAttempt();
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        manager.fireInitialSyncTimeoutForTesting();
        manager.injectClientEventForTesting(syncDoneEvent());
        QVERIFY(!manager.initialSyncComplete());
        QVERIFY(manager.reconnectAttempt() >= attempts);
    }

    void packagedVirtualOutputBecomesReadyAndTracksDefaultMetadata()
    {
        PipeWireManager manager;
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        manager.injectClientEventForTesting(graphEvent(makeNode(
            60,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("sink")},
             {QStringLiteral("auralis.virtual.persistence"), QStringLiteral("package")}})));
        manager.injectClientEventForTesting(graphEvent(makePort(600, 60, QStringLiteral("in"))));
        manager.injectClientEventForTesting(graphEvent(makeNode(
            61,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("source")},
             {QStringLiteral("auralis.virtual.persistence"), QStringLiteral("package")}})));
        manager.injectClientEventForTesting(graphEvent(makePort(610, 61, QStringLiteral("out"))));
        manager.injectClientEventForTesting(syncDoneEvent());

        QVERIFY(manager.virtualOutputAvailable());
        QVERIFY(!manager.virtualOutputSelected());
        QCOMPARE(manager.virtualOutputStatus(), QStringLiteral("Ready (persistent)"));
        QCOMPARE(manager.audioRouter()->sourceDisplayName(QStringLiteral("src:auralis-system-audio")),
                 QStringLiteral("Auralis System Audio"));

        manager.injectClientEventForTesting(defaultSinkEvent(QStringLiteral("auralis_virtual_output")));
        QVERIFY(manager.virtualOutputSelected());
        QCOMPARE(manager.virtualOutputStatus(), QStringLiteral("Selected as system output"));
        QVERIFY(manager.diagnosticsText().contains(QStringLiteral("defaultSink=auralis_virtual_output")));
        manager.shutdown();
    }

    void initialSyncEventBeforeConnectedIsNotLost()
    {
        PipeWireManager manager;
        manager.injectClientEventForTesting(syncDoneEvent());
        QVERIFY(!manager.initialSyncComplete());

        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        QVERIFY(manager.initialSyncComplete());
        QVERIFY(!manager.initialSyncTimeoutPendingForTesting());
        manager.shutdown();
    }

    void ignoresDefaultSinkMetadataForNonCoreSubject()
    {
        PipeWireManager manager;
        manager.injectClientEventForTesting(stateEvent(PipeWireConnectionState::Connected));
        manager.injectClientEventForTesting(graphEvent(makeNode(
            70,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output")}})));
        manager.injectClientEventForTesting(graphEvent(makePort(700, 70, QStringLiteral("in"))));
        manager.injectClientEventForTesting(graphEvent(makeNode(
            71,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")}})));
        manager.injectClientEventForTesting(graphEvent(makePort(710, 71, QStringLiteral("out"))));
        manager.injectClientEventForTesting(syncDoneEvent());

        PipeWireClientEvent metadata = defaultSinkEvent(QStringLiteral("auralis_virtual_output"));
        metadata.metadataSubject = 99;
        manager.injectClientEventForTesting(metadata);
        QVERIFY(!manager.virtualOutputSelected());
        manager.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstPipeWireManager)
#include "tst_PipeWireManager.moc"
