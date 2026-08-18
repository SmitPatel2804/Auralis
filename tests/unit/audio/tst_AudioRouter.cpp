#include "AudioTestFixtures.h"
#include "FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>

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
        QCOMPARE(h.backend.createCalls, 2);
        h.router.deactivateRoute(id);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Inactive);
        QVERIFY(route->ownedLinks.isEmpty());
        QVERIFY(h.backend.createdIds.isEmpty());
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
        QVERIFY(route->error.category == RouteError::LinkCreationFailed);
    }

    void sourceLossDegradesEnabledRoute()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        h.store.remove(11);
        h.store.remove(12);
        h.store.remove(1);
        h.router.handleGraphChanged();
        const auto route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Degraded);
        QVERIFY(route->enabled);
        QVERIFY(route->error.category == RouteError::SourceRemoved);
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
    }

    void rebindOnNewNodeId()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
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
    }

    void disconnectKeepsEnabledAndReplansAfterSync()
    {
        Harness h;
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Active);

        h.router.handleConnectionState(PipeWireConnectionState::Error, false);
        auto route = h.router.routeById(id);
        QVERIFY(route->enabled);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.front().globalId, static_cast<quint32>(0));
        QVERIFY(route->error.category == RouteError::PipeWireDisconnected);

        for (quint32 linkId : QVector<quint32>(h.backend.createdIds)) {
            h.store.remove(linkId);
        }
        h.backend.createdIds.clear();
        h.router.handleConnectionState(PipeWireConnectionState::Connected, true);
        route = h.router.routeById(id);
        QVERIFY(route->state == RouteState::Active);
        QCOMPARE(route->ownedLinks.size(), 2);
        QVERIFY(route->ownedLinks.front().globalId != 0);
    }

    void deactivateWinsOverInFlightActivation()
    {
        Harness h;
        h.backend.createdLinkState = QStringLiteral("init");
        h.addStereoStream(1, 11, 12);
        h.addStereoSink(2, 21, 22, QStringLiteral("dest-a"));
        const QString id = h.router.createRoute(h.sourceId(), {QStringLiteral("dest-a")});
        h.router.activateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Activating);

        h.router.deactivateRoute(id);
        QVERIFY(h.router.routeById(id)->state == RouteState::Inactive);
        QVERIFY(h.router.routeById(id)->ownedLinks.isEmpty());

        h.store.upsert(makeLink(900, 1, 11, 2, 21, QStringLiteral("active")));
        h.store.upsert(makeLink(901, 1, 12, 2, 22, QStringLiteral("active")));
        h.router.handleGraphChanged();
        QVERIFY(h.router.routeById(id)->state == RouteState::Inactive);
        QVERIFY(!h.router.routeById(id)->enabled);
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
};

QTEST_GUILESS_MAIN(TstAudioRouter)
#include "tst_AudioRouter.moc"
