#include "../audio/AudioTestFixtures.h"
#include "../audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/session/VolumeCoordinator.h>

#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::session::AuralisSession;
using auralis::session::SessionDevice;
using auralis::session::VolumeCoordinator;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstVolumeCoordinator : public QObject {
    Q_OBJECT

private slots:
    void groupAndTrimCombine()
    {
        AuralisSession session;
        session.groupVolume = 0.5;
        SessionDevice device;
        device.volumeTrim = 0.8;
        QCOMPARE(VolumeCoordinator::effectiveVolume(session, device), 0.4);
    }

    void muteAppliesThroughRouter()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        VolumeCoordinator volume(&router);
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("dest-a")}}));
        auto endpoint = makePlaybackEndpoint(QStringLiteral("dest-a"), 2, QStringLiteral("dest-a"));
        endpoints.upsert(endpoint);

        AuralisSession session;
        session.groupVolume = 0.6;
        SessionDevice device;
        device.volumeTrim = 0.5;
        device.runtime.endpointId = QStringLiteral("dest-a");
        device.runtime.routeActive = true;
        session.devices.push_back(device);

        volume.applyMemberVolume(session, device);
        QCOMPARE(backend.lastVolume, 0.3);
        QVERIFY(!backend.lastMuted);
        QCOMPARE(backend.lastDelaySeconds, 0.0);

        session.muted = true;
        volume.applyMemberVolume(session, device);
        QVERIFY(backend.lastMuted);
    }

    void memberDelayIsAppliedThroughRouter()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        VolumeCoordinator volume(&router);
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("dest-a")}}));
        auto endpoint = makePlaybackEndpoint(QStringLiteral("dest-a"), 2, QStringLiteral("dest-a"));
        endpoints.upsert(endpoint);

        AuralisSession session;
        SessionDevice device;
        device.delayMs = 40.0;
        device.runtime.endpointId = QStringLiteral("dest-a");
        device.runtime.routeActive = true;

        volume.applyMemberVolume(session, device);
        QCOMPARE(backend.lastDelaySeconds, 0.04);
        QCOMPARE(backend.lastDelayBridgeSec.value(QStringLiteral("dest-a")), 0.04);
    }

    void unavailableMemberRetainsIntendedVolume()
    {
        AuralisSession session;
        session.groupVolume = 0.75;
        SessionDevice device;
        device.volumeTrim = 0.4;
        device.runtime.routeActive = false;
        session.devices.push_back(device);

        QCOMPARE(VolumeCoordinator::effectiveVolume(session, device), 0.3);
    }

    void reapplyOnRecover()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        VolumeCoordinator volume(&router);
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("dest-a")}}));
        auto endpoint = makePlaybackEndpoint(QStringLiteral("dest-a"), 2, QStringLiteral("dest-a"));
        endpoints.upsert(endpoint);

        AuralisSession session;
        session.groupVolume = 1.0;
        SessionDevice device;
        device.enabled = true;
        device.volumeTrim = 0.25;
        device.runtime.endpointId = QStringLiteral("dest-a");
        device.runtime.routeActive = false;
        session.devices.push_back(device);

        volume.applySessionVolumes(session);
        QCOMPARE(backend.lastVolumeNode, 0u);

        session.devices[0].runtime.routeActive = true;
        volume.applySessionVolumes(session);
        QCOMPARE(backend.lastVolume, 0.25);
    }
};

QTEST_GUILESS_MAIN(TstVolumeCoordinator)
#include "tst_VolumeCoordinator.moc"
