#include "AudioTestFixtures.h"

#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/RoutePlanner.h>

#include <QtTest>

using auralis::audio::AudioEndpointRegistry;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::RouteError;
using auralis::audio::RoutePlanner;
using auralis::test::makeNode;
using auralis::test::makePlaybackEndpoint;
using auralis::test::makePort;

class TstRoutePlanner : public QObject {
    Q_OBJECT

private:
    static void addStereoStream(PipeWireObjectStore& store, quint32 nodeId, quint32 portFl, quint32 portFr)
    {
        store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                       {QStringLiteral("node.name"), QStringLiteral("player")},
                                       {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(portFl, nodeId, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(portFr, nodeId, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
    }

    static void addStereoSink(PipeWireObjectStore& store, AudioEndpointRegistry& endpoints, quint32 nodeId, quint32 inFl, quint32 inFr, const QString& endpointId)
    {
        store.upsert(makeNode(nodeId, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                       {QStringLiteral("node.name"), endpointId}}));
        store.upsert(makePort(inFl, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(inFr, nodeId, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        endpoints.upsert(makePlaybackEndpoint(endpointId, nodeId, endpointId));
    }

private slots:
    void mono()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("node.name"), QStringLiteral("mono")},
                                  {QStringLiteral("object.serial"), QStringLiteral("1")}}));
        store.upsert(makePort(10, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("MONO")}}));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}, {QStringLiteral("node.name"), QStringLiteral("spk")}}));
        store.upsert(makePort(20, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("MONO")}}));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-mono"), 2));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:1:Stream/Output/Audio"), {QStringLiteral("dest-mono")}, store, endpoints);
        QVERIFY(!plan.error.hasError());
        QCOMPARE(plan.pairs.size(), 1);
        QCOMPARE(plan.pairs.front().outputPortId, static_cast<quint32>(10));
        QCOMPARE(plan.pairs.front().inputPortId, static_cast<quint32>(20));
    }

    void stereo()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        addStereoStream(store, 1, 11, 12);
        addStereoSink(store, endpoints, 2, 21, 22, QStringLiteral("dest-a"));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:7:Stream/Output/Audio"), {QStringLiteral("dest-a")}, store, endpoints);
        QVERIFY(!plan.error.hasError());
        QCOMPARE(plan.pairs.size(), 2);
    }

    void oneSourceTwoDestinationsCreatesFourPairs()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        addStereoStream(store, 1, 11, 12);
        addStereoSink(store, endpoints, 2, 21, 22, QStringLiteral("dest-a"));
        addStereoSink(store, endpoints, 3, 31, 32, QStringLiteral("dest-b"));
        const auto plan =
            RoutePlanner{}.plan(QStringLiteral("src:7:Stream/Output/Audio"), {QStringLiteral("dest-a"), QStringLiteral("dest-b")}, store, endpoints);
        QVERIFY(!plan.error.hasError());
        QCOMPARE(plan.pairs.size(), 4);
    }

    void reversedRegistryStillMapsChannels()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("7")}}));
        store.upsert(makePort(12, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        store.upsert(makePort(22, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:7:Stream/Output/Audio"), {QStringLiteral("dest-a")}, store, endpoints);
        QCOMPARE(plan.pairs.size(), 2);
        for (const auto& pair : plan.pairs) {
            if (pair.channel == QLatin1String("FL")) {
                QCOMPARE(pair.outputPortId, static_cast<quint32>(11));
                QCOMPARE(pair.inputPortId, static_cast<quint32>(21));
            }
        }
    }

    void missingChannelMetadataFallsBackToIndex()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("8")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out")));
        store.upsert(makePort(12, 1, QStringLiteral("out")));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in")));
        store.upsert(makePort(22, 2, QStringLiteral("in")));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:8:Stream/Output/Audio"), {QStringLiteral("dest-a")}, store, endpoints);
        QVERIFY(!plan.error.hasError());
        QCOMPARE(plan.pairs.size(), 2);
    }

    void controlPortsIgnored()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("9")}}));
        store.upsert(makePort(11, 1, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(19, 1, QStringLiteral("out"), {{QStringLiteral("port.control"), QStringLiteral("true")}}));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        store.upsert(makePort(21, 2, QStringLiteral("in"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(29, 2, QStringLiteral("in"), {{QStringLiteral("port.control"), QStringLiteral("true")}}));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:9:Stream/Output/Audio"), {QStringLiteral("dest-a")}, store, endpoints);
        QCOMPARE(plan.pairs.size(), 1);
        QCOMPARE(plan.pairs.front().outputPortId, static_cast<quint32>(11));
    }

    void destinationGoneFails()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("1")}}));
        store.upsert(makePort(10, 1, QStringLiteral("out")));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:1:Stream/Output/Audio"), {QStringLiteral("missing")}, store, endpoints);
        QVERIFY(plan.error.category == RouteError::DestinationNotFound);
    }

    void noCompatiblePortsWhenDestinationHasNoInputs()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry endpoints;
        store.upsert(makeNode(1, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                  {QStringLiteral("object.serial"), QStringLiteral("1")}}));
        store.upsert(makePort(10, 1, QStringLiteral("out")));
        store.upsert(makeNode(2, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        endpoints.upsert(makePlaybackEndpoint(QStringLiteral("dest-a"), 2));
        const auto plan = RoutePlanner{}.plan(QStringLiteral("src:1:Stream/Output/Audio"), {QStringLiteral("dest-a")}, store, endpoints);
        QVERIFY(plan.error.category == RouteError::NoCompatiblePorts);
    }
};

QTEST_GUILESS_MAIN(TstRoutePlanner)
#include "tst_RoutePlanner.moc"
