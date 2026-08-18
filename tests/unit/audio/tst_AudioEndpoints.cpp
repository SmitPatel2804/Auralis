#include "AudioTestFixtures.h"

#include <auralis/audio/AudioEndpointClassifier.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/EndpointResolver.h>
#include <auralis/audio/PipeWireObjectStore.h>

#include <QSignalSpy>
#include <QtTest>

using auralis::audio::AudioEndpoint;
using auralis::audio::AudioEndpointDirection;
using auralis::audio::AudioEndpointRegistry;
using auralis::audio::AudioTransport;
using auralis::audio::PipeWireObjectKind;
using auralis::audio::PipeWireObjectStore;
using auralis::audio::classifyAudioEndpoint;
using auralis::audio::refreshEndpointsFromStore;
using auralis::test::makeDevice;
using auralis::test::makeNode;
using auralis::test::makeSnapshot;

class TstAudioEndpoints : public QObject {
    Q_OBJECT

private:
    static PipeWireObjectStore storeWith(const auralis::audio::PipeWireObjectSnapshot& snapshot)
    {
        PipeWireObjectStore store;
        store.upsert(snapshot);
        return store;
    }

private slots:
    void sinkBecomesPlayback()
    {
        PipeWireObjectStore store = storeWith(makeNode(10, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                                            {QStringLiteral("node.description"), QStringLiteral("Speakers")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(10), store);
        QVERIFY(endpoint.has_value());
        QVERIFY(endpoint->direction == AudioEndpointDirection::Playback);
        QCOMPARE(endpoint->name, QStringLiteral("Speakers"));
    }

    void sourceBecomesCapture()
    {
        PipeWireObjectStore store = storeWith(makeNode(11, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                                            {QStringLiteral("node.name"), QStringLiteral("mic")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(11), store);
        QVERIFY(endpoint.has_value());
        QVERIFY(endpoint->direction == AudioEndpointDirection::Capture);
    }

    void nonAudioIgnored()
    {
        PipeWireObjectStore store = storeWith(makeNode(12, {{QStringLiteral("media.class"), QStringLiteral("Video/Source")}}));
        QVERIFY(!classifyAudioEndpoint(*store.node(12), store).has_value());
    }

    void applicationStreamIgnored()
    {
        PipeWireObjectStore store = storeWith(makeNode(13, {{QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio")},
                                                            {QStringLiteral("node.name"), QStringLiteral("Firefox")}}));
        QVERIFY(!classifyAudioEndpoint(*store.node(13), store).has_value());
    }

    void bluetoothA2dpPlayback()
    {
        PipeWireObjectStore store = storeWith(makeNode(
            14,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
             {QStringLiteral("device.api"), QStringLiteral("bluez5")},
             {QStringLiteral("api.bluez5.profile"), QStringLiteral("a2dp-sink")},
             {QStringLiteral("api.bluez5.address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(14), store);
        QVERIFY(endpoint.has_value());
        QVERIFY(endpoint->direction == AudioEndpointDirection::Playback);
        QVERIFY(endpoint->transport == AudioTransport::BluetoothClassic);
        QCOMPARE(endpoint->profile, QStringLiteral("a2dp-sink"));
    }

    void bluetoothHfpCapture()
    {
        PipeWireObjectStore store = storeWith(makeNode(
            15,
            {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
             {QStringLiteral("device.api"), QStringLiteral("bluez5")},
             {QStringLiteral("api.bluez5.profile"), QStringLiteral("headset-head-unit")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(15), store);
        QVERIFY(endpoint.has_value());
        QVERIFY(endpoint->direction == AudioEndpointDirection::Capture);
    }

    void missingDescriptionUsesFallbackName()
    {
        PipeWireObjectStore store = storeWith(makeNode(16, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                                            {QStringLiteral("node.name"), QStringLiteral("alsa_output")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(16), store);
        QVERIFY(endpoint.has_value());
        QCOMPARE(endpoint->name, QStringLiteral("alsa_output"));
    }

    void missingSampleRateStillValid()
    {
        PipeWireObjectStore store = storeWith(makeNode(17, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")}}));
        const auto endpoint = classifyAudioEndpoint(*store.node(17), store);
        QVERIFY(endpoint.has_value());
        QVERIFY(!endpoint->sampleRate.has_value());
    }

    void registryDeduplicatesAndUpdates()
    {
        AudioEndpointRegistry registry;
        AudioEndpoint first;
        first.id = QStringLiteral("pw:1:playback:out");
        first.pipeWireObjectId = 40;
        first.name = QStringLiteral("Old");
        first.direction = AudioEndpointDirection::Playback;
        QVERIFY(registry.upsert(first));
        QCOMPARE(registry.count(), 1);

        AudioEndpoint same = first;
        QVERIFY(!registry.upsert(same));
        QCOMPARE(registry.count(), 1);

        same.name = QStringLiteral("New");
        QVERIFY(!registry.upsert(same));
        QCOMPARE(registry.findById(first.id)->name, QStringLiteral("New"));
        QCOMPARE(registry.findByPipeWireObjectId(40)->name, QStringLiteral("New"));
    }

    void registryRemoveAndQuery()
    {
        AudioEndpointRegistry registry;
        AudioEndpoint playback;
        playback.id = QStringLiteral("pw:1:playback:out");
        playback.pipeWireObjectId = 41;
        playback.direction = AudioEndpointDirection::Playback;
        playback.bluetoothDeviceId = QStringLiteral("/org/bluez/hci0/dev_AA");
        AudioEndpoint capture;
        capture.id = QStringLiteral("pw:1:capture:in");
        capture.pipeWireObjectId = 42;
        capture.direction = AudioEndpointDirection::Capture;
        capture.bluetoothDeviceId = QStringLiteral("/org/bluez/hci0/dev_AA");
        registry.upsert(playback);
        registry.upsert(capture);
        QCOMPARE(registry.playbackEndpoints().size(), 1);
        QCOMPARE(registry.endpointsForBluetoothDevice(QStringLiteral("/org/bluez/hci0/dev_AA")).size(), 2);
        QVERIFY(registry.removeByPipeWireObjectId(41));
        QCOMPARE(registry.count(), 1);
        registry.clear();
        QCOMPARE(registry.count(), 0);
    }

    void objectIdChurnKeepsLogicalIdentity()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry registry;
        auralis::audio::EndpointResolver resolver;
        store.upsert(makeNode(81, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.name"), QStringLiteral("built-in")},
                                   {QStringLiteral("object.serial"), QStringLiteral("9")}}));
        refreshEndpointsFromStore(store, registry, resolver);
        QCOMPARE(registry.count(), 1);
        const QString logicalId = registry.at(0).id;
        store.remove(81);
        store.upsert(makeNode(96, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.name"), QStringLiteral("built-in")},
                                   {QStringLiteral("object.serial"), QStringLiteral("9")}}));
        refreshEndpointsFromStore(store, registry, resolver);
        QCOMPARE(registry.count(), 1);
        QCOMPARE(registry.at(0).id, logicalId);
        QCOMPARE(registry.at(0).pipeWireObjectId, static_cast<quint32>(96));
    }

    void nonBluetoothAlsaHasNoBluetoothId()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry registry;
        auralis::audio::EndpointResolver resolver;
        store.upsert(makeNode(50, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("device.api"), QStringLiteral("alsa")},
                                   {QStringLiteral("device.bus"), QStringLiteral("pci")},
                                   {QStringLiteral("node.description"), QStringLiteral("Built-in Audio Analog Stereo")}}));
        refreshEndpointsFromStore(store, registry, resolver);
        QCOMPARE(registry.count(), 1);
        QVERIFY(registry.at(0).transport == AudioTransport::BuiltIn);
        QVERIFY(registry.at(0).bluetoothDeviceId.isEmpty());
    }

    void reusedGlobalIdEvictsPreviousKindAndEndpoint()
    {
        PipeWireObjectStore store;
        AudioEndpointRegistry registry;
        auralis::audio::EndpointResolver resolver;
        store.upsert(makeNode(70, {{QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
                                   {QStringLiteral("node.name"), QStringLiteral("bluez_output.88:08:94:9D:B4:22")}}));
        refreshEndpointsFromStore(store, registry, resolver);
        QCOMPARE(store.nodeCount(), 1);
        QCOMPARE(registry.count(), 1);

        store.upsert(makeSnapshot(70, PipeWireObjectKind::Port, {}));
        refreshEndpointsFromStore(store, registry, resolver);
        QCOMPARE(store.nodeCount(), 0);
        QCOMPARE(store.portCount(), 1);
        QVERIFY(store.node(70) == nullptr);
        QCOMPARE(registry.count(), 0);
    }

    void monitorAndPortAreNotEndpoints()
    {
        PipeWireObjectStore store;
        store.upsert(makeNode(70, {{QStringLiteral("media.class"), QStringLiteral("Audio/Source")},
                                   {QStringLiteral("stream.monitor"), QStringLiteral("true")}}));
        store.upsert(makeSnapshot(71, PipeWireObjectKind::Port, {}));
        QVERIFY(!classifyAudioEndpoint(*store.node(70), store).has_value());
        QCOMPARE(store.portCount(), 1);
    }

    void noOpUpdateDoesNotEmit()
    {
        AudioEndpointRegistry registry;
        QSignalSpy updated(&registry, &AudioEndpointRegistry::endpointUpdated);
        AudioEndpoint endpoint;
        endpoint.id = QStringLiteral("pw:x:playback:n");
        endpoint.pipeWireObjectId = 1;
        endpoint.name = QStringLiteral("A");
        registry.upsert(endpoint);
        registry.upsert(endpoint);
        QCOMPARE(updated.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TstAudioEndpoints)
#include "tst_AudioEndpoints.moc"
