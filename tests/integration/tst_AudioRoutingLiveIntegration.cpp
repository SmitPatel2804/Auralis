#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/AudioSource.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireObjectStore.h>
#include <auralis/bluetooth/BluetoothManager.h>

#include <QElapsedTimer>
#include <QtTest>

#include <cmath>
#include <cstdint>
#include <functional>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#pragma GCC diagnostic pop

using auralis::audio::AudioEndpointAvailability;
using auralis::audio::AudioEndpointDirection;
using auralis::audio::AudioRouter;
using auralis::audio::AudioSourceType;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::audio::RouteState;
using auralis::audio::toString;
using auralis::bluetooth::BluetoothManager;

namespace {

constexpr auto kAppName = "AuralisRoutingTest";
constexpr int kRate = 48000;
constexpr int kChannels = 2;
constexpr float kAmplitude = 0.02f;

class TestToneStream {
public:
    ~TestToneStream() { stop(); }

    bool start()
    {
        pw_init(nullptr, nullptr);
        loop_ = pw_thread_loop_new("auralis-routing-test", nullptr);
        if (loop_ == nullptr) {
            return false;
        }
        context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
        if (context_ == nullptr) {
            return false;
        }
        core_ = pw_context_connect(context_, nullptr, 0);
        if (core_ == nullptr) {
            return false;
        }

        pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_CLASS,
            "Stream/Output/Audio",
            PW_KEY_MEDIA_TYPE,
            "Audio",
            PW_KEY_MEDIA_CATEGORY,
            "Playback",
            PW_KEY_APP_NAME,
            kAppName,
            PW_KEY_NODE_NAME,
            "auralis-routing-test",
            PW_KEY_NODE_DESCRIPTION,
            "Auralis routing integration tone",
            nullptr);
        stream_ = pw_stream_new(core_, "auralis-routing-test", props);
        if (stream_ == nullptr) {
            return false;
        }

        events_ = {};
        events_.version = PW_VERSION_STREAM_EVENTS;
        events_.process = &TestToneStream::onProcess;
        pw_stream_add_listener(stream_, &listener_, &events_, this);

        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_S16;
        info.channels = kChannels;
        info.rate = kRate;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const spa_pod* params[1];
        params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info);

        pw_thread_loop_start(loop_);
        pw_thread_loop_lock(loop_);
        const int connected = pw_stream_connect(
            stream_,
            PW_DIRECTION_OUTPUT,
            PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
            params,
            1);
        pw_thread_loop_unlock(loop_);
        return connected >= 0;
    }

    void stop()
    {
        if (loop_ != nullptr) {
            pw_thread_loop_lock(loop_);
        }
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (loop_ != nullptr) {
            pw_thread_loop_unlock(loop_);
            pw_thread_loop_stop(loop_);
        }
        if (core_ != nullptr) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }
        if (context_ != nullptr) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }
        if (loop_ != nullptr) {
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
        }
    }

private:
    static void onProcess(void* data)
    {
        auto* self = static_cast<TestToneStream*>(data);
        pw_buffer* buffer = pw_stream_dequeue_buffer(self->stream_);
        if (buffer == nullptr || buffer->buffer == nullptr || buffer->buffer->datas[0].data == nullptr) {
            return;
        }
        const quint32 stride = static_cast<quint32>(kChannels * static_cast<int>(sizeof(int16_t)));
        const quint32 nFrames = buffer->buffer->datas[0].maxsize / stride;
        auto* samples = static_cast<int16_t*>(buffer->buffer->datas[0].data);
        const double step = 2.0 * 3.14159265358979323846 * 440.0 / static_cast<double>(kRate);
        for (quint32 i = 0; i < nFrames; ++i) {
            const auto sample = static_cast<int16_t>(std::sin(self->phase_) * kAmplitude * 32767.0);
            samples[i * 2] = sample;
            samples[i * 2 + 1] = sample;
            self->phase_ += step;
            if (self->phase_ > 2.0 * 3.14159265358979323846) {
                self->phase_ -= 2.0 * 3.14159265358979323846;
            }
        }
        buffer->buffer->datas[0].chunk->offset = 0;
        buffer->buffer->datas[0].chunk->stride = static_cast<int32_t>(stride);
        buffer->buffer->datas[0].chunk->size = nFrames * stride;
        pw_stream_queue_buffer(self->stream_, buffer);
    }

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_stream* stream_ = nullptr;
    pw_stream_events events_{};
    spa_hook listener_{};
    double phase_ = 0.0;
};

} // namespace

class TstAudioRoutingLiveIntegration : public QObject {
    Q_OBJECT

private:
    static QString diagnostics(const PipeWireManager& audio)
    {
        return audio.diagnosticsText();
    }

    static QString endpointDump(const PipeWireManager& audio)
    {
        QString text = QStringLiteral("Playback endpoints:\n");
        if (audio.endpointRegistry() == nullptr) {
            return text + QStringLiteral("(no registry)\n");
        }
        for (const auto& endpoint : audio.endpointRegistry()->playbackEndpoints()) {
            text += QStringLiteral("  id=%1 name=%2 addr=%3 avail=%4 transport=%5 node=%6\n")
                        .arg(endpoint.id,
                             endpoint.name,
                             endpoint.bluetoothAddress,
                             toString(endpoint.availability),
                             toString(endpoint.transport),
                             endpoint.nodeName);
        }
        return text;
    }

    static int auralisTaggedLinkCount(const PipeWireManager& audio)
    {
        const auto* store = audio.objectStore();
        if (store == nullptr) {
            return 0;
        }
        int count = 0;
        for (const auto& link : store->links()) {
            if (!link.properties.value(QStringLiteral("auralis.route.id")).isEmpty()) {
                ++count;
            }
        }
        return count;
    }

    static QString auralisLinkDump(const PipeWireManager& audio)
    {
        QString text = QStringLiteral("Links with auralis.route.id:\n");
        const auto* store = audio.objectStore();
        if (store == nullptr) {
            return text + QStringLiteral("(no object store)\n");
        }
        for (const auto& link : store->links()) {
            const QString routeId = link.properties.value(QStringLiteral("auralis.route.id"));
            if (routeId.isEmpty()) {
                continue;
            }
            text += QStringLiteral("  pw=%1 route=%2 out=%3:%4 in=%5:%6 state=%7\n")
                        .arg(link.globalId)
                        .arg(routeId)
                        .arg(link.outputNode.value_or(0))
                        .arg(link.outputPort.value_or(0))
                        .arg(link.inputNode.value_or(0))
                        .arg(link.inputPort.value_or(0))
                        .arg(toString(link.state));
        }
        return text;
    }

    static bool waitUntil(const std::function<bool()>& predicate, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (predicate()) {
                return true;
            }
            QTest::qWait(50);
        }
        return predicate();
    }

private slots:
    void liveAdditiveRouteRoundTrip()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_AUDIO_ROUTING_INTEGRATION") != 1) {
            QSKIP("Set AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 to run live audio routing tests");
        }

        BluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        PipeWireManager audio(bluetooth.deviceRegistry());
        QVERIFY(audio.initialize());
        QVERIFY2(
            waitUntil(
                [&audio]() {
                    return audio.connectionState() == PipeWireConnectionState::Connected
                        || audio.connectionState() == PipeWireConnectionState::Error;
                },
                5000),
            qPrintable(QStringLiteral("PipeWire did not settle: %1").arg(diagnostics(audio))));
        QVERIFY2(
            audio.connectionState() == PipeWireConnectionState::Connected,
            qPrintable(QStringLiteral("AURALIS_RUN_AUDIO_ROUTING_INTEGRATION=1 but PipeWire is not connected: %1")
                           .arg(diagnostics(audio))));
        QVERIFY2(
            waitUntil([&audio]() { return audio.initialSyncComplete(); }, 5000),
            qPrintable(QStringLiteral("Initial registry sync did not complete: %1").arg(diagnostics(audio))));
        QVERIFY2(
            waitUntil([&audio]() { return audio.endpointCount() > 0; }, 8000),
            qPrintable(QStringLiteral("No AudioEndpoint objects after live enumeration: %1").arg(diagnostics(audio))));

        const QString expectedAddress =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        QString destinationId;
        for (const auto& endpoint : audio.endpointRegistry()->playbackEndpoints()) {
            if (endpoint.availability != AudioEndpointAvailability::Available) {
                continue;
            }
            if (endpoint.direction != AudioEndpointDirection::Playback
                && endpoint.direction != AudioEndpointDirection::Duplex) {
                continue;
            }
            if (!expectedAddress.isEmpty()) {
                if (endpoint.bluetoothAddress.toUpper() == expectedAddress) {
                    destinationId = endpoint.id;
                    break;
                }
                continue;
            }
            destinationId = endpoint.id;
            break;
        }
        if (destinationId.isEmpty()) {
            const QString message = !expectedAddress.isEmpty()
                ? QStringLiteral("Expected Bluetooth destination %1 was not found. No speaker fallback.\n%2\n%3")
                      .arg(expectedAddress, endpointDump(audio), diagnostics(audio))
                : QStringLiteral("No available playback destination.\n%1\n%2")
                      .arg(endpointDump(audio), diagnostics(audio));
            QFAIL(qPrintable(message));
        }

        TestToneStream tone;
        QVERIFY2(tone.start(), "Failed to start test-only low-volume pw_stream sine source");

        AudioRouter* router = audio.audioRouter();
        QVERIFY(router != nullptr);
        QVERIFY2(
            waitUntil(
                [router]() {
                    for (const auto& source : router->sourceList()) {
                        if (source.sourceType == AudioSourceType::ApplicationPlaybackStream
                            && source.applicationName == QLatin1String(kAppName)) {
                            return true;
                        }
                    }
                    return false;
                },
                8000),
            qPrintable(QStringLiteral("Test playback stream never appeared as a source. %1").arg(diagnostics(audio))));

        QString sourceId;
        for (const auto& source : router->sourceList()) {
            if (source.applicationName == QLatin1String(kAppName)) {
                sourceId = source.id;
                break;
            }
        }
        QVERIFY(!sourceId.isEmpty());

        const QString routeId = router->createRoute(sourceId, {destinationId});
        QVERIFY2(!routeId.isEmpty(), qPrintable(router->lastErrorText()));
        router->activateRoute(routeId);
        QVERIFY2(
            waitUntil(
                [router, routeId]() {
                    const auto route = router->routeById(routeId);
                    return route.has_value() && route->state == RouteState::Active && !route->ownedLinks.isEmpty();
                },
                8000),
            qPrintable(QStringLiteral("Route did not become Active with owned links: %1 %2")
                           .arg(router->lastErrorText(), diagnostics(audio))));

        const int ownedWhileActive = router->ownedLinkCount();
        QVERIFY(ownedWhileActive > 0);

        router->deactivateRoute(routeId);
        QVERIFY2(
            waitUntil(
                [router, routeId]() {
                    const auto route = router->routeById(routeId);
                    return route.has_value() && route->state == RouteState::Inactive && route->ownedLinks.isEmpty();
                },
                5000),
            qPrintable(QStringLiteral("Owned links remained after deactivate: count=%1 %2")
                           .arg(router->ownedLinkCount())
                           .arg(diagnostics(audio))));
        QCOMPARE(router->ownedLinkCount(), 0);
        QVERIFY2(
            waitUntil([&audio]() { return auralisTaggedLinkCount(audio) == 0; }, 5000),
            qPrintable(QStringLiteral("PipeWire graph still has Auralis-owned links after deactivate.\n%1\n%2")
                           .arg(auralisLinkDump(audio), diagnostics(audio))));
        QCOMPARE(auralisTaggedLinkCount(audio), 0);

        tone.stop();
        audio.shutdown();
        bluetooth.shutdown();
        QVERIFY(audio.connectionState() == PipeWireConnectionState::Stopped);
    }
};

QTEST_GUILESS_MAIN(TstAudioRoutingLiveIntegration)
#include "tst_AudioRoutingLiveIntegration.moc"
