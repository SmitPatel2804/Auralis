#include "../audio/AudioTestFixtures.h"
#include "../audio/FakePipeWireLinkBackend.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/audio/RouteListModel.h>
#include <auralis/bluetooth/DeviceRegistry.h>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioRouter;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireObjectStore;
using auralis::test::FakePipeWireLinkBackend;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstRouteListModel : public QObject {
    Q_OBJECT

private slots:
    void tracksRoutes()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        FakePipeWireLinkBackend backend{&store};
        AudioRouter router{&store, &endpoints, &backend};
        router.handleConnectionState(PipeWireConnectionState::Connected, true);

        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        router.refreshSources();
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("sink-a"), 2, QStringLiteral("Sink A")));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                  {QStringLiteral("node.name"), QStringLiteral("sink-a")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(22, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));

        auto* model = qobject_cast<QAbstractItemModel*>(router.routeModel());
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 0);

        QString sourceId;
        if (!router.sourceList().isEmpty()) {
            sourceId = router.sourceList().front().id;
        }
        QVERIFY(!sourceId.isEmpty());
        const QString routeId = router.createRoute(sourceId, {QStringLiteral("sink-a")});
        QVERIFY(!routeId.isEmpty());
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->data(model->index(0, 0), auralis::audio::RouteListModel::IdRole).toString(), routeId);
        QCOMPARE(model->data(model->index(0, 0), auralis::audio::RouteListModel::EditableRole).toBool(), true);
        QCOMPARE(
            model->data(model->index(0, 0), auralis::audio::RouteListModel::OwnerLabelRole).toString(),
            QStringLiteral("Manual"));

        router.removeRoute(routeId);
        QCOMPARE(model->rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TstRouteListModel)
#include "tst_RouteListModel.moc"
