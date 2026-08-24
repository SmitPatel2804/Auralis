#include "AudioTestFixtures.h"
#include "FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireVirtualOutput.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::RouteError;
using auralis::audio::RouteState;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeLink;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstAudioRouter : public QObject {
    Q_OBJECT

private:
    struct Harness {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend{&store};
        AudioRouter router{&store, &endpoints, &backend};

        Harness()
        {
            router.handleConnectionState(PipeWireConnectionState::Connected, true);
        }

        void addStereoStream(quint32 nodeId, quint32 portFl, quint32 portFr, const QString& serial = QStringLiteral("7"))
        {
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                           {QStringLiteral("node.name"), QStringLiteral("player")},
                                           {QStringLiteral("object.serial"), serial}}));
            store.upsert(makePort(portFl, nodeId, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(portFr, nodeId, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            router.refreshSources();
        }

        void addStereoSink(quint32 nodeId, quint32 inFl, quint32 inFr, const QString& endpointId)
        {
            store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                           {QStringLiteral("node.name"), endpointId}}));
            store.upsert(makePort(inFl, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
            store.upsert(makePort(inFr, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
            endpoints.upsert(makePlaybackEndpoint(endpointId, nodeId, endpointId));
        }

        QString sourceId(const QString& serial = QStringLiteral("7")) const
        {
            return QStringLiteral("src:%1:Stream/Output/Audio").arg(serial);
        }
    };

private slots:
    void unchangedSourceRefreshDoesNotEmit()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        QSignalSpy sourcesSpy(&h.router, &AudioRouter::sourcesChanged);

        h.router.refreshSources();

        QCOMPARE(sourcesSpy.count(), 0);
    }

    void createActivateDeactivate()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        QVERIFY(!id.isEmpty());
        QCOMPARE(h.router.routeStateText(), QStringLiteral("Inactive"));
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route.has_value());
        QVERIFY(route->state == RouteState::Active);
        QVERIFY(route->enabled);
        QCOMPARE(route->ownedLinks.size(), 2);
        QVERIFY(route->ownedLinks.front().ownershipToken != 0);
        QCOMPARE(h.backend.createCalls, 2);
        h.router.deactivateRoute(id);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Inactive);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.createdIds.isEmpty());
        QVERIFY(h.backend.createdTokens.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
    }

    void copyCaptureToWindowsDefaultIsRejectedToPreventEcho()
    {
        Harness h;
        h.store.upsert(makeNode(1, {
            {QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
            {QStringLiteral("node.name"), QStringLiteral("browser")},
            {QStringLiteral("object.serial"), QStringLiteral("7")},
            {QStringLiteral("auralis.capture.mode"), QStringLiteral("copy")},
        }));
        h.store.upsert(makePort(11, 1, QStringLiteral("out")));
        h.router.refreshSources();
        h.store.upsert(makeNode(2, {
            {QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
            {QStringLiteral("device.default"), QStringLiteral("true")},
        }));
        h.store.upsert(makePort(21, 2, QStringLiteral("in")));
        h.endpoints.upsert(makePlaybackEndpoint(QStringLiteral("default-output"), 2));

        QSignalSpy errorSpy(&h.router, &AudioRouter::routeError);
        const QString routeId = h.router.createRoute(h.sourceId(), {QStringLiteral("default-output")});
        QVERIFY(routeId.isEmpty());
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.front().at(1).value<RouteError>() == RouteError::UnsupportedDirection);
        QVERIFY(errorSpy.front().at(2).toString().contains(QStringLiteral("Duplicate audio prevented")));
        QCOMPARE(h.backend.createCalls, 0);
    }

    void nonCopyCaptureMayRouteToDefaultEndpoint()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.store.upsert(makeNode(2, {
            {QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
            {QStringLiteral("device.default"), QStringLiteral("true")},
        }));
        h.store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(22, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        h.endpoints.upsert(makePlaybackEndpoint(QStringLiteral("default-output"), 2));

        const QString routeId = h.router.createRoute(h.sourceId(), {QStringLiteral("default-output")});
        QVERIFY(!routeId.isEmpty());
    }

    void rollbackDestroysPartialLinks()
    {
        Harness h;
        h.backend.failOnCreate = 1;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Failed);
        QVERIFY(!route->enabled);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.createdIds.isEmpty());
        QVERIFY(h.backend.createdTokens.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
        QVERIFY(route->error.category == RouteError::LinkCreationFailed);
    }

    void pendingCreateThenFailDestroysPendingToken()
    {
        Harness h;
        h.backend.delayBind = true;
        h.backend.failOnCreate = 1;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Failed);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
        QVERIFY(h.backend.createdTokens.isEmpty());
        QCOMPARE(h.backend.destroyedTokens.size(), 1);
        QVERIFY(h.backend.destroyedTokens.front() != 0);
        QVERIFY(h.store.linkCount() == 0);
    }

    void sourceLossDegradesEnabledRoute()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(!h.router.routeById(id)->ownedLinks.isEmpty());
        h.store.remove(11);
        h.store.remove(12);
        h.store.remove(1);
        h.router.handleGraphChanged();
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->enabled);
        QVERIFY(route->error.category == RouteError::SourceRemoved);
        QCOMPARE(route->sourceId, h.sourceId());
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
    }

    void destinationLossDegradesEnabledRoute()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        h.endpoints.removeById(QStringLiteral("dest-a"));
        h.router.handleGraphChanged();
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->error.category == RouteError::DestinationRemoved);
        QCOMPARE(route->destinationIds, QStringList{QStringLiteral("dest-a")});
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
    }

    void rebindOnNewNodeId()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        const quint64 oldToken = h.router.routeById(id)->ownedLinks.front().ownershipToken;
        const quint32 oldLink = h.router.routeById(id)->ownedLinks.front().globalId;

        h.store.remove(11);
        h.store.remove(12);
        h.store.remove(1);
        h.store.remove(21);
        h.store.remove(22);
        h.store.remove(2);
        h.addStereoStream(100, 111, 112);
        h.addStereoSink(200, 221, 222, QStringLiteral("dest-a"));
        h.router.handleGraphChanged();

        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.size(), 2);
        QCOMPARE(route->ownedLinks.front().outputNodeId, static_cast<quint32>(100));
        QCOMPARE(route->ownedLinks.front().inputNodeId, static_cast<quint32>(200));
        QVERIFY(route->ownedLinks.front().globalId != oldLink);
        QVERIFY(route->ownedLinks.front().ownershipToken != oldToken);
    }

    void replanFailureKeepsOperationalLinks()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        const quint64 oldToken = route->ownedLinks.front().ownershipToken;
        const quint32 oldOutputPort = route->ownedLinks.front().outputPortId;

        h.backend.failOnCreate = h.backend.createCalls + 1;
        h.store.remove(21);
        h.store.remove(22);
        h.store.upsert(makePort(221, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(222, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        h.router.handleGraphChanged();

        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.front().ownershipToken, oldToken);
        QCOMPARE(route->ownedLinks.front().outputPortId, oldOutputPort);
    }

    void disconnectKeepsEnabledAndReplansAfterSync()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Active);
        const quint64 oldToken = h.router.routeById(id)->ownedLinks.front().ownershipToken;
        QVERIFY(oldToken != 0);

        h.router.handleConnectionState(PipeWireConnectionState::Error, false);
        auto route = h.router.routeById(id);
        QVERIFY(route->enabled);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
        QVERIFY(route->error.category == RouteError::PipeWireDisconnected);

        h.router.handleConnectionState(PipeWireConnectionState::Connected, false);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->ownedLinks.isEmpty());
        QCOMPARE(h.backend.createCalls, 2);

        h.router.handleConnectionState(PipeWireConnectionState::Connected, true);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.size(), 2);
        QVERIFY(route->ownedLinks.front().ownershipToken != 0);
        QVERIFY(route->ownedLinks.front().ownershipToken != oldToken);
        QVERIFY(route->ownedLinks.front().globalId != 0);
    }

    void foreignIdenticalPortLinkNeverOperational()
    {
        Harness h;
        h.backend.delayBind = true;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Activating);
        QCOMPARE(route->ownedLinks.size(), 2);
        QCOMPARE(route->ownedLinks.front().globalId, static_cast<quint32>(0));
        const quint64 token0 = route->ownedLinks.at(0).ownershipToken;
        const quint64 token1 = route->ownedLinks.at(1).ownershipToken;

        h.store.upsert(makeLink(50, 1, 11, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(51, 1, 12, 2, 22, QStringLiteral("active")));
        h.router.handleGraphChanged();
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Activating);
        QCOMPARE(route->ownedLinks.front().globalId, static_cast<quint32>(0));
        bool ownsForeign = false;
        for (const auto& created : h.backend.owned) {
            if (created.globalId == 50 || created.globalId == 51) {
                ownsForeign = true;
            }
        }
        QVERIFY(!ownsForeign);

        h.backend.completeBind(token0, 900);
        h.backend.completeBind(token1, 901);
        h.router.handleGraphChanged();
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.at(0).globalId, static_cast<quint32>(900));
        QCOMPARE(route->ownedLinks.at(1).globalId, static_cast<quint32>(901));
    }

    void sessionSyncDoesNotDuplicatePendingActivation()
    {
        Harness h;
        h.backend.delayBind = true;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString sessionId = QStringLiteral("session-a");
        const QString routeId = h.router.createSessionRoute(
            sessionId, h.sourceId(), {QStringLiteral("dest-a")});

        h.router.activateRoute(routeId);
        auto route = h.router.routeById(routeId);
        QVERIFY(route.has_value());
        QVERIFY(route->state == RouteState::Activating);
        QCOMPARE(route->ownedLinks.size(), 2);
        QCOMPARE(h.backend.createCalls, 2);
        const QVector<auralis::audio::OwnedLink> pendingLinks = route->ownedLinks;

        // SessionManager applies volume state immediately after route creation.
        // The resulting sync must not replan links whose PipeWire globals have
        // not appeared yet.
        h.router.syncSessionDestinations(sessionId);

        route = h.router.routeById(routeId);
        QVERIFY(route.has_value());
        QVERIFY(route->state == RouteState::Activating);
        QCOMPARE(route->ownedLinks.size(), pendingLinks.size());
        QCOMPARE(route->ownedLinks.at(0).ownershipToken, pendingLinks.at(0).ownershipToken);
        QCOMPARE(route->ownedLinks.at(1).ownershipToken, pendingLinks.at(1).ownershipToken);
        QCOMPARE(h.backend.createCalls, 2);
        QCOMPARE(h.backend.owned.size(), 2);

        h.backend.completeBind(pendingLinks.at(0).ownershipToken, 900);
        h.backend.completeBind(pendingLinks.at(1).ownershipToken, 901);
        h.router.handleGraphChanged();
        QVERIFY(h.router.routeById(routeId)->state == RouteState::Active);
    }

    void twoRoutesHaveIndependentTimeouts()
    {
        Harness h;
        h.backend.delayBind = true;
        h.router.setActivationTimeoutMs(80);
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        const QString idA = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        const QString idB = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(idA);
        h.router.activateRoute(idB);
        QVERIFY(h.router.routeById(idA)->state == RouteState::Activating);
        QVERIFY(h.router.routeById(idB)->state == RouteState::Activating);

        const auto tokensA = h.router.routeById(idA)->ownedLinks;
        QCOMPARE(tokensA.size(), 2);
        h.backend.completeBind(tokensA.at(0).ownershipToken, 910);
        h.backend.completeBind(tokensA.at(1).ownershipToken, 911);
        h.router.handleGraphChanged();
        QVERIFY(h.router.routeById(idA)->state == RouteState::Active);
        QVERIFY(h.router.routeById(idB)->state == RouteState::Activating);

        QTRY_VERIFY_WITH_TIMEOUT(h.router.routeById(idB)->state == RouteState::Failed, 1000);
        QVERIFY(h.router.routeById(idA)->state == RouteState::Active);
        QVERIFY(h.router.routeById(idA)->enabled);
        QVERIFY(h.router.routeById(idB)->state == RouteState::Failed);
        QVERIFY(h.router.routeById(idB)->error.category == RouteError::LinkCreationFailed);
    }

    void ownedLinkErrorDegradesRoute()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.size(), 2);
        const auto owned = route->ownedLinks.front();
        QVERIFY(owned.ownershipToken != 0);
        QVERIFY(owned.globalId != 0);

        h.store.upsert(makeLink(
            owned.globalId,
            owned.outputNodeId,
            owned.outputPortId,
            owned.inputNodeId,
            owned.inputPortId,
            QStringLiteral("error")));
        h.router.handleGraphChanged();
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->enabled);
        QVERIFY(route->error.category == RouteError::LinkEnteredErrorState);
        QCOMPARE(route->ownedLinks.front().globalId, owned.globalId);
        QCOMPARE(route->ownedLinks.front().ownershipToken, owned.ownershipToken);

        h.store.upsert(makeLink(50, owned.outputNodeId, owned.outputPortId, owned.inputNodeId, owned.inputPortId));
        h.router.handleGraphChanged();
        route = h.router.routeById(id);
        QVERIFY(route->state != RouteState::Active);
        QVERIFY(route->state == RouteState::Degraded);
        QCOMPARE(route->ownedLinks.front().globalId, owned.globalId);
        QVERIFY(route->ownedLinks.front().globalId != static_cast<quint32>(50));
    }

    void removeRouteWhileActivatingCannotResurrect()
    {
        Harness h;
        h.backend.delayBind = true;
        h.router.setActivationTimeoutMs(80);
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Activating);
        QCOMPARE(route->ownedLinks.size(), 2);
        const quint64 token0 = route->ownedLinks.at(0).ownershipToken;
        const quint64 token1 = route->ownedLinks.at(1).ownershipToken;
        QVERIFY(token0 != 0);
        QVERIFY(token1 != 0);

        QSignalSpy addedSpy(&h.router, &AudioRouter::routeAdded);
        QSignalSpy stateSpy(&h.router, &AudioRouter::routeStateChanged);
        h.router.removeRoute(id);
        QVERIFY(!h.router.routeById(id).has_value());
        QVERIFY(h.backend.owned.isEmpty());
        QVERIFY(h.backend.createdTokens.isEmpty());
        QVERIFY(h.router.routes().isEmpty());

        h.backend.completeBind(token0, 900);
        h.backend.completeBind(token1, 901);
        h.router.handleGraphChanged();
        QTest::qWait(200);
        QVERIFY(!h.router.routeById(id).has_value());
        QVERIFY(h.router.routes().isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
        for (int i = 0; i < stateSpy.size(); ++i) {
            QVERIFY(stateSpy.at(i).at(1).toInt() != static_cast<int>(RouteState::Active));
        }
        QCOMPARE(addedSpy.count(), 0);
    }

    void deactivateWhileActivatingCannotResurrect()
    {
        Harness h;
        h.backend.delayBind = true;
        h.router.setActivationTimeoutMs(80);
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Activating);
        const quint64 token0 = route->ownedLinks.at(0).ownershipToken;
        const quint64 token1 = route->ownedLinks.at(1).ownershipToken;

        h.router.deactivateRoute(id);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Inactive);
        QVERIFY(!route->enabled);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.owned.isEmpty());

        h.backend.completeBind(token0, 900);
        h.backend.completeBind(token1, 901);
        h.store.upsert(makeLink(900, 1, 11, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(901, 1, 12, 2, 22, QStringLiteral("active")));
        h.router.handleGraphChanged();
        QTest::qWait(200);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Inactive);
        QVERIFY(!route->enabled);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(route->state != RouteState::Ready);
        QVERIFY(route->state != RouteState::Activating);
        QVERIFY(route->state != RouteState::Active);
    }

    void staleActivationTimeoutGenerationIsIgnored()
    {
        Harness h;
        h.backend.delayBind = true;
        h.router.setActivationTimeoutMs(80);
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Activating);

        h.router.deactivateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Inactive);

        h.backend.delayBind = false;
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Active);
        const quint64 token = h.router.routeById(id)->ownedLinks.front().ownershipToken;
        QVERIFY(token != 0);

        QTest::qWait(200);
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QVERIFY(route->enabled);
        QVERIFY(route->error.category == RouteError::None);
        QCOMPARE(route->ownedLinks.front().ownershipToken, token);
    }

    void shutdownInvalidatesAllActivationTimers()
    {
        Harness h;
        h.backend.delayBind = true;
        h.router.setActivationTimeoutMs(80);
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        const QString idA = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        const QString idB = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(idA);
        h.router.activateRoute(idB);
        QVERIFY(h.router.routeById(idA)->state == RouteState::Activating);
        QVERIFY(h.router.routeById(idB)->state == RouteState::Activating);
        QVERIFY(!h.backend.owned.isEmpty());

        QSignalSpy errorSpy(&h.router, &AudioRouter::routeError);
        h.router.shutdown();
        QVERIFY(h.router.routes().isEmpty());
        QVERIFY(h.backend.owned.isEmpty());
        QCOMPARE(h.router.ownedLinkCount(), 0);
        QTest::qWait(200);
        QVERIFY(h.router.routeById(idA) == std::nullopt);
        QVERIFY(h.router.routeById(idB) == std::nullopt);
        QVERIFY(h.backend.owned.isEmpty());
        QCOMPARE(errorSpy.count(), 0);
    }

    void qmlPropertyNotifyFiresOnActivate()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        QSignalSpy idSpy(&h.router, &AudioRouter::currentRouteIdChanged);
        QSignalSpy stateSpy(&h.router, &AudioRouter::routeStateTextChanged);
        QSignalSpy enabledSpy(&h.router, &AudioRouter::routeEnabledChanged);
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        QVERIFY(idSpy.count() >= 1);
        h.router.activateRoute(id);
        QVERIFY(stateSpy.count() >= 1);
        QVERIFY(enabledSpy.count() >= 1);
        QCOMPARE(h.router.routeStateText(), QStringLiteral("Active"));
        QCOMPARE(h.router.currentRouteId(), id);
    }

    void volumeWriteFailureDoesNotTearDownRoute()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Active);
        h.backend.failVolumeNodes.insert(2);
        h.router.setRouteVolume(id, 0.2);
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QVERIFY(route->enabled);
        QCOMPARE(route->volume, 0.2);
        QVERIFY(!route->ownedLinks.isEmpty());
    }

    void activateWithoutConnectionFails()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out")));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in")));
        router.refreshSources();
        const QString id = router.createRoute(QStringLiteral("src:7:Stream/Output/Audio"), {QStringLiteral("dest-a")});
        router.activateRoute(id);
        QVERIFY(router.routeById(id)->state == RouteState::Failed);
        QVERIFY(router.routeById(id)->error.category == RouteError::PipeWireDisconnected);
    }

    void connectedWithoutSyncDoesNotActivate()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend(&store);
        AudioRouter router(&store, &endpoints, &backend);
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")},
                                  {QStringLiteral("node.name"), QStringLiteral("player")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("dest-a")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(22, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2, QStringLiteral("dest-a")));
        router.refreshSources();
        router.handleConnectionState(PipeWireConnectionState::Connected, false);
        const QString id = router.createRoute(QStringLiteral("src:7:Stream/Output/Audio"), {QStringLiteral("dest-a")});
        router.activateRoute(id);
        QVERIFY(router.routeById(id)->state == RouteState::Failed);
        QVERIFY(router.ownedLinkCount() == 0);
        QCOMPARE(backend.createCalls, 0);
    }

    void sessionOwnedRoutesAreIsolatedFromPlanner()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        const QString sessionRoute = h.router.createSessionRoute(
            QStringLiteral("session-a"), h.sourceId(), {QStringLiteral("dest-a")});
        QVERIFY(!sessionRoute.isEmpty());
        QVERIFY(h.router.currentRouteId().isEmpty());
        const QString sessionSource = h.router.routeById(sessionRoute)->sourceId;
        const QStringList sessionDests = h.router.routeById(sessionRoute)->destinationIds;

        const QString manual = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-b")});
        QVERIFY(!manual.isEmpty());
        QCOMPARE(h.router.currentRouteId(), manual);
        h.router.setRouteDestinations(manual, {QStringLiteral("dest-b")});
        h.router.setRouteVolume(manual, 0.4);
        h.router.setRouteMuted(manual, true);

        const auto session = h.router.routeById(sessionRoute);
        QVERIFY(session.has_value());
        QCOMPARE(session->sourceId, sessionSource);
        QCOMPARE(session->destinationIds, sessionDests);
        QCOMPARE(session->volume, 1.0);
        QCOMPARE(session->muted, false);
        QCOMPARE(static_cast<int>(session->ownerType), static_cast<int>(auralis::audio::RouteOwnerType::Session));
        QCOMPARE(session->ownerId, QStringLiteral("session-a"));

        QSignalSpy errorSpy(&h.router, &AudioRouter::routeError);
        h.router.setRouteSource(sessionRoute, h.sourceId());
        h.router.setRouteVolume(sessionRoute, 0.1);
        h.router.setRouteMuted(sessionRoute, true);
        QCOMPARE(h.router.routeById(sessionRoute)->volume, 1.0);
        QCOMPARE(h.router.routeById(sessionRoute)->muted, false);
        QVERIFY(errorSpy.count() >= 1);
    }

    void clearsForeignLinksBeforeActivation()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        // Foreign producers on the destination inputs must be cleared (input exclusivity).
        h.store.upsert(makeLink(50, 90, 91, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(51, 90, 92, 2, 22, QStringLiteral("active")));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        const auto route = h.router.routeById(id);
        QVERIFY(route.has_value());
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(h.backend.createCalls, 2);
        QVERIFY(h.store.link(50) == nullptr);
        QVERIFY(h.store.link(51) == nullptr);
    }

    void adoptsExistingLinksInsteadOfCreating()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.store.upsert(makeLink(60, 1, 11, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(61, 1, 12, 2, 22, QStringLiteral("active")));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        const auto route = h.router.routeById(id);
        QVERIFY(route.has_value());
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(h.backend.createCalls, 0);
        QCOMPARE(route->ownedLinks.size(), 2);
        QCOMPARE(route->ownedLinks.at(0).globalId, 60u);
        QCOMPARE(route->ownedLinks.at(1).globalId, 61u);
        QCOMPARE(route->ownedLinks.at(0).ownershipToken, 0u);
    }

    void twoDestinationsShareSourceWithoutTearingSiblingLinks()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));

        const QString routeA = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(routeA);
        const auto activeA = h.router.routeById(routeA);
        QVERIFY(activeA.has_value());
        QVERIFY(activeA->state == RouteState::Active);
        QCOMPARE(activeA->ownedLinks.size(), 2);
        const quint32 aFl = activeA->ownedLinks.at(0).globalId;
        const quint32 aFr = activeA->ownedLinks.at(1).globalId;
        QVERIFY(aFl != 0);
        QVERIFY(aFr != 0);

        const QString routeB = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(routeB);
        const auto activeB = h.router.routeById(routeB);
        QVERIFY(activeB.has_value());
        QVERIFY(activeB->state == RouteState::Active);
        QCOMPARE(activeB->ownedLinks.size(), 2);

        const auto stillA = h.router.routeById(routeA);
        QVERIFY(stillA.has_value());
        QVERIFY(stillA->state == RouteState::Active);
        QCOMPARE(stillA->ownedLinks.size(), 2);
        QCOMPARE(stillA->ownedLinks.at(0).globalId, aFl);
        QCOMPARE(stillA->ownedLinks.at(1).globalId, aFr);
        QVERIFY(h.store.link(aFl) != nullptr);
        QVERIFY(h.store.link(aFr) != nullptr);
        QCOMPARE(h.router.ownedLinkCount(), 4);
    }

    void outputFanOutIsNotAConflict()
    {
        PipeWireObjectStore store;
        store.upsert(makeLink(10, 1, 11, 2, 21, QStringLiteral("active")));
        // Same output, different input — fan-out, not a conflict.
        QVERIFY(!store.findConflictingLinkGlobalId(1, 11, 3, 31).has_value());
        // Different producer on the same input — conflict.
        store.upsert(makeLink(11, 9, 91, 3, 31, QStringLiteral("active")));
        QCOMPARE(store.findConflictingLinkGlobalId(1, 11, 3, 31).value_or(0u), 11u);
    }

    void secondRouteDoesNotDestroyAdoptedSibling()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        h.store.upsert(makeLink(60, 1, 11, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(61, 1, 12, 2, 22, QStringLiteral("active")));

        const QString routeA = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(routeA);
        QCOMPARE(h.backend.createCalls, 0);

        const QString routeB = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(routeB);
        QCOMPARE(h.backend.createCalls, 2);

        const auto stillA = h.router.routeById(routeA);
        QVERIFY(stillA.has_value());
        QVERIFY(stillA->state == RouteState::Active);
        QCOMPARE(stillA->ownedLinks.at(0).globalId, 60u);
        QCOMPARE(stillA->ownedLinks.at(1).globalId, 61u);
        QVERIFY(h.store.link(60) != nullptr);
        QVERIFY(h.store.link(61) != nullptr);
    }

    void auralisSystemAudioDisplayNameIsNotMic()
    {
        Harness h;
        h.store.upsert(makeNode(
            80,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("source")}}));
        h.store.upsert(makePort(800, 80, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(801, 80, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        h.router.refreshSources();
        QCOMPARE(h.router.sourceDisplayName(QStringLiteral("src:auralis-system-audio")),
                 QStringLiteral("Auralis Virtual Output"));
    }

    void sourcePickerShowsOnlyAuralisVirtualOutput()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.store.upsert(makeNode(
            80,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("source")}}));
        h.store.upsert(makePort(800, 80, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(801, 80, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        h.router.refreshSources();
        QCOMPARE(h.router.sourceList().size(), 2);
        QCOMPARE(h.router.sourceCount(), 1);
        auto* model = h.router.sources();
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->data(model->index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("Auralis Virtual Output"));
    }

    void manualDelayBridgePadsHeadset()
    {
        Harness h;
        h.backend.fanoutEnabled = true;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        h.store.upsert(makeNode(
            50,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_session_fanout")},
             {QStringLiteral("auralis.session.fanout"), QStringLiteral("true")}}));
        h.store.upsert(makePort(51, 50, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(52, 50, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        const QString routeA =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-a")});
        const QString routeB =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(routeA);
        h.router.activateRoute(routeB);
        h.router.setDestinationDelayMs(QStringLiteral("dest-a"), 40.0);
        h.router.setDestinationDelayMs(QStringLiteral("dest-b"), 0.0);
        QCOMPARE(h.backend.lastDelayBridgeSec.value(QStringLiteral("dest-a")), 0.04);
        QVERIFY(h.backend.lastDelayBridgeSec.value(QStringLiteral("dest-b")) < 0.0005);
        QVERIFY(h.backend.lastFanoutNames.contains(
            auralis::audio::auralisDelayBridgeCaptureNodeName(QStringLiteral("dest-a"))));
        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-b")));
        bool sawDelayLink = false;
        for (auto it = h.backend.owned.constBegin(); it != h.backend.owned.constEnd(); ++it) {
            if (!it->routeId.startsWith(QLatin1String("delay:"))) {
                continue;
            }
            sawDelayLink = true;
            QVERIFY(it->outputNode != it->inputNode);
            QCOMPARE(it->inputNode, quint32(2));
        }
        QVERIFY(sawDelayLink);
    }

    void delayPadKeepsHeadsetUntilBridgeIsReady()
    {
        Harness h;
        h.backend.fanoutEnabled = true;
        h.backend.omitDelayNodes = true;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        const QString routeA =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-a")});
        const QString routeB =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(routeA);
        h.router.activateRoute(routeB);
        h.router.setDestinationDelayMs(QStringLiteral("dest-a"), 40.0);
        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-a")));
        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-b")));
        QVERIFY(!h.backend.lastFanoutNames.contains(
            auralis::audio::auralisDelayBridgeCaptureNodeName(QStringLiteral("dest-a"))));
        h.backend.omitDelayNodes = false;
        h.backend.destroyDelayBridge(QStringLiteral("dest-a"));
        h.router.handleGraphChanged();
        QVERIFY(h.backend.lastFanoutNames.contains(
            auralis::audio::auralisDelayBridgeCaptureNodeName(QStringLiteral("dest-a"))));
        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-b")));
        QVERIFY(!h.backend.lastFanoutNames.contains(QStringLiteral("dest-a")));
    }

    void sessionFanoutLinksSourceToMixNotDestinations()
    {
        Harness h;
        h.backend.fanoutEnabled = true;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        h.addStereoSink(3, 31, 32, QStringLiteral("dest-b"));
        h.store.upsert(makeNode(
            50,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_session_fanout")},
             {QStringLiteral("auralis.session.fanout"), QStringLiteral("true")}}));
        h.store.upsert(makePort(51, 50, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        h.store.upsert(makePort(52, 50, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));

        const QString routeA =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-a")});
        const QString routeB =
            h.router.createSessionRoute(QStringLiteral("sess"), h.sourceId(), {QStringLiteral("dest-b")});
        h.router.activateRoute(routeA);
        h.router.activateRoute(routeB);

        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-a")));
        QVERIFY(h.backend.lastFanoutNames.contains(QStringLiteral("dest-b")));
        const auto first = h.router.routeById(routeA);
        const auto second = h.router.routeById(routeB);
        QVERIFY(first.has_value());
        QVERIFY(second.has_value());
        QVERIFY(first->state == RouteState::Active);
        QVERIFY(second->state == RouteState::Active);
        const bool firstLeads = !first->ownedLinks.isEmpty();
        const bool secondLeads = !second->ownedLinks.isEmpty();
        QVERIFY(firstLeads != secondLeads);
        const auto& leader = firstLeads ? *first : *second;
        QCOMPARE(leader.ownedLinks.size(), 2);
        QCOMPARE(leader.ownedLinks.front().inputNodeId, static_cast<quint32>(50));

        const int creates = h.backend.createCalls;
        h.router.handleGraphChanged();
        h.router.handleGraphChanged();
        QCOMPARE(h.backend.createCalls, creates);
        QVERIFY(h.router.routeById(firstLeads ? routeA : routeB)->state == RouteState::Active);
    }
};

QTEST_GUILESS_MAIN(TstAudioRouter)
#include "tst_AudioRouter.moc"
