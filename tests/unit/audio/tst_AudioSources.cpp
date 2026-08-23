#include "AudioTestFixtures.h"

#include <auralis/audio/AudioSource.h>
#include <auralis/audio/PipeWireObjectStore.h>

#include <QtTest>

using auralis::audio::AudioSourceType;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::classifyAudioSource;
using auralis::audio::classifyAudioSources;
using auralis::test::makeNode;
using auralis::test::makePort;

class TstAudioSources : public QObject {
    Q_OBJECT

private slots:
    void identicalSnapshotsDoNotReportGraphChanges()
    {
        PipeWireObjectStore store;
        const auto original = makeNode(
            9,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("player")}});

        QVERIFY(store.upsert(original));
        QVERIFY(!store.upsert(original));

        const auto changed = makeNode(
            9,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("renamed-player")}});
        QVERIFY(store.upsert(changed));
        QVERIFY(!store.upsert(changed));
    }

    void applicationPlaybackStream()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(10, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                   {QStringLiteral("node.name"), QStringLiteral("Firefox")},
                                   {QStringLiteral("application.name"), QStringLiteral("Firefox")}}));
        store.upsert(makePort(100, 10, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        store.upsert(makePort(101, 10, QStringLiteral("out"), {{QStringLiteral("audio.channel"), QStringLiteral("FR")}}));
        const auto source = classifyAudioSource(*store.node(10), store);
        QVERIFY(source.has_value());
        QVERIFY(source->sourceType == AudioSourceType::ApplicationPlaybackStream);
        QCOMPARE(source->applicationName, QStringLiteral("Firefox"));
        QCOMPARE(source->portIds.size(), 2);
        QVERIFY(source->id.startsWith(QLatin1String("src:")));
    }

    void physicalAudioSource()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(11, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                   {QStringLiteral("node.name"), QStringLiteral("mic")},
                                   {QStringLiteral("device.api"), QStringLiteral("alsa")}}));
        store.upsert(makePort(110, 11, QStringLiteral("out")));
        const auto source = classifyAudioSource(*store.node(11), store);
        QVERIFY(source.has_value());
        QVERIFY(source->sourceType == AudioSourceType::PhysicalAudioSource);
    }

    void sinkIsNotOrdinarySource()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(12, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.name"), QStringLiteral("speakers")}}));
        store.upsert(makePort(120, 12, QStringLiteral("in")));
        QVERIFY(!classifyAudioSource(*store.node(12), store).has_value());
    }

    void captureStreamIgnored()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(13, {{QStringLiteral("media.class"), QStringLiteral("Stream/Input/Audio")},
                                   {QStringLiteral("node.name"), QStringLiteral("record")}}));
        store.upsert(makePort(130, 13, QStringLiteral("in")));
        QVERIFY(!classifyAudioSource(*store.node(13), store).has_value());
    }

    void sinkMonitorPortsBecomeSource()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(14, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.name"), QStringLiteral("alsa_output")},
                                   {QStringLiteral("object.serial"), QStringLiteral("44")}}));
        store.upsert(makePort(140, 14, QStringLiteral("in")));
        store.upsert(makePort(
            141,
            14,
            QStringLiteral("out"),
            {{QStringLiteral("port.monitor"), QStringLiteral("true")}, {QStringLiteral("audio.channel"), QStringLiteral("FL")}}));
        const auto source = classifyAudioSource(*store.node(14), store);
        QVERIFY(source.has_value());
        QVERIFY(source->sourceType == AudioSourceType::SinkMonitor);
        QVERIFY(source->monitorSource);
        QCOMPARE(source->id, QStringLiteral("src:44:SinkMonitor"));
        QCOMPARE(source->portIds.size(), 1);
    }

    void controlPortsAreNotRoutableOutputs()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(15, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                   {QStringLiteral("node.name"), QStringLiteral("app")}}));
        store.upsert(makePort(150, 15, QStringLiteral("out"), {{QStringLiteral("port.control"), QStringLiteral("true")}}));
        QVERIFY(!classifyAudioSource(*store.node(15), store).has_value());
        QCOMPARE(classifyAudioSources(store).size(), 0);
    }

    void operatingSystemDefaultSourceComesFirst()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(20, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                   {QStringLiteral("node.name"), QStringLiteral("alphabetical-first")},
                                   {QStringLiteral("node.description"), QStringLiteral("A microphone")}}));
        store.upsert(makePort(200, 20, QStringLiteral("out")));
        store.upsert(makeNode(21, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                   {QStringLiteral("node.name"), QStringLiteral("system-default")},
                                   {QStringLiteral("node.description"), QStringLiteral("Z microphone")},
                                   {QStringLiteral("device.default"), QStringLiteral("true")}}));
        store.upsert(makePort(210, 21, QStringLiteral("out")));

        const QVector sources = classifyAudioSources(store);
        QCOMPARE(sources.size(), 2);
        QCOMPARE(sources.constFirst().pipeWireNodeId, static_cast<quint32>(21));
    }

    void auralisVirtualSourceHasStableIdentityAndSinkIsHidden()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(
            30,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("sink")}}));
        store.upsert(makePort(300, 30, QStringLiteral("in")));
        store.upsert(makePort(
            301,
            30,
            QStringLiteral("out"),
            {{QStringLiteral("port.monitor"), QStringLiteral("true")}}));
        store.upsert(makeNode(
            31,
            {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
             {QStringLiteral("node.name"), QStringLiteral("auralis_virtual_output.source")},
             {QStringLiteral("application.name"), QStringLiteral("PipeWire")},
             {QStringLiteral("auralis.virtual.output"), QStringLiteral("true")},
             {QStringLiteral("auralis.virtual.role"), QStringLiteral("source")}}));
        store.upsert(makePort(310, 31, QStringLiteral("out")));

        QVERIFY(!classifyAudioSource(*store.node(30), store).has_value());
        const auto source = classifyAudioSource(*store.node(31), store);
        QVERIFY(source.has_value());
        QVERIFY(source->sourceType == AudioSourceType::VirtualAudioSource);
        QCOMPARE(source->id, QStringLiteral("src:auralis-system-audio"));
        QCOMPARE(source->description, QStringLiteral("Auralis System Audio"));
        QVERIFY(source->applicationName.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstAudioSources)
#include "tst_AudioSources.moc"
