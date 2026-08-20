#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRoute.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/AudioSource.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/session/SelectedSessionViewModel.h>
#include <auralis/session/SessionManager.h>
#include <auralis/session/SessionTypes.h>

#include <QAbstractItemModel>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
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
using auralis::audio::RouteOwnerType;
using auralis::audio::RouteState;
using auralis::bluetooth::BluetoothDeviceListModel;
using auralis::bluetooth::BluetoothManager;
using auralis::session::SelectedSessionViewModel;
using auralis::session::SessionCommandResult;
using auralis::session::SessionManager;
using auralis::session::SessionState;

namespace {

constexpr auto kAppName = "AuralisPhase7Hardware";
constexpr int kRate = 48000;
constexpr int kChannels = 2;
constexpr float kAmplitude = 0.02f;

class TestToneStream {
public:
    ~TestToneStream() { stop(); }

    bool start()
    {
        pw_init(nullptr, nullptr);
        loop_ = pw_thread_loop_new("auralis-phase7-hw", nullptr);
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
            "auralis-phase7-hw",
            PW_KEY_NODE_LATENCY,
            "1024/48000",
            nullptr);
        stream_ = pw_stream_new(core_, "auralis-phase7-hw", props);
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
        const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
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
        const quint32 stride = sizeof(qint16) * kChannels;
        const quint32 nFrames = buffer->buffer->datas[0].maxsize / stride;
        auto* samples = static_cast<qint16*>(buffer->buffer->datas[0].data);
        for (quint32 i = 0; i < nFrames; ++i) {
            const auto v = static_cast<qint16>(std::sin(self->phase_) * kAmplitude * 32767.0f);
            samples[i * 2] = v;
            samples[i * 2 + 1] = v;
            self->phase_ += 2.0f * static_cast<float>(M_PI) * 440.0f / static_cast<float>(kRate);
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
    spa_hook listener_{};
    pw_stream_events events_{};
    float phase_ = 0.0f;
};

QVariant roleValue(QAbstractItemModel* model, const QString& objectPath, int role)
{
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->data(model->index(row, 0), BluetoothDeviceListModel::ObjectPathRole).toString() == objectPath) {
            return model->data(model->index(row, 0), role);
        }
    }
    return {};
}

QString pathForAddress(QAbstractItemModel* model, const QString& address)
{
    const QString expected = address.trimmed().toUpper();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString actual =
            model->data(model->index(row, 0), BluetoothDeviceListModel::AddressRole).toString().trimmed().toUpper();
        if (actual == expected) {
            return model->data(model->index(row, 0), BluetoothDeviceListModel::ObjectPathRole).toString();
        }
    }
    return {};
}

void autoAccept(BluetoothManager& manager)
{
    QObject* pending = manager.pendingPairingRequest();
    if (pending == nullptr || pending->property("needsInput").toBool()) {
        return;
    }
    const QString requestId = pending->property("requestId").toString();
    if (!requestId.isEmpty()) {
        manager.acceptPairingRequest(requestId);
    }
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs, BluetoothManager* manager = nullptr)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (manager != nullptr) {
            autoAccept(*manager);
        }
        if (predicate()) {
            return true;
        }
        QTest::qWait(50);
    }
    if (manager != nullptr) {
        autoAccept(*manager);
    }
    return predicate();
}

} // namespace

class TstPhase7HardwareLive : public QObject {
    Q_OBJECT

private:
    static QString firstApplicationSourceId(AudioRouter* router, const QString& appName)
    {
        for (const auto& source : router->sourceList()) {
            if (source.applicationName == appName) {
                return source.id;
            }
        }
        return {};
    }

private slots:
    void singleConnectedDeviceActivateShouldReachActiveOrDegraded()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_PHASE7_HARDWARE") != 1) {
            QSKIP("Set AURALIS_RUN_PHASE7_HARDWARE=1 to run live hardware");
        }
        const QString address =
            QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESS")).trimmed().toUpper();
        if (address.isEmpty()) {
            QSKIP("Set AURALIS_EXPECT_DEVICE_ADDRESS to a connected headset for single-device activate");
        }

        BluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        PipeWireManager pipeWire(bluetooth.deviceRegistry());
        QVERIFY(pipeWire.initialize());
        QTemporaryDir dir;
        SessionManager sessions(&bluetooth, &pipeWire, dir.filePath(QStringLiteral("sessions.json")));
        QVERIFY(sessions.initialize());
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth.agentRegistered(), 5000);
        auto* model = bluetooth.devices();
        QVERIFY(waitUntil([&]() { return !pathForAddress(model, address).isEmpty(); }, 8000, &bluetooth));
        const QString path = pathForAddress(model, address);
        bluetooth.connectDevice(path);
        QVERIFY2(
            waitUntil(
                [&]() { return roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool(); },
                60000,
                &bluetooth),
            "Single-device connect failed");
        QVERIFY(waitUntil(
            [&]() {
                return pipeWire.connectionState() == PipeWireConnectionState::Connected
                    && pipeWire.initialSyncComplete();
            },
            8000));
        QVERIFY2(
            waitUntil(
                [&]() {
                    for (const auto& endpoint : pipeWire.endpointRegistry()->mappedBluetoothEndpoints()) {
                        if (endpoint.bluetoothAddress.toUpper() == address
                            && endpoint.availability == AudioEndpointAvailability::Available) {
                            return true;
                        }
                    }
                    return false;
                },
                15000),
            qPrintable(pipeWire.diagnosticsText()));

        TestToneStream tone;
        QVERIFY(tone.start());
        AudioRouter* router = pipeWire.audioRouter();
        QVERIFY(waitUntil([&]() { return !firstApplicationSourceId(router, QLatin1String(kAppName)).isEmpty(); }, 8000));
        const QString sourceId = firstApplicationSourceId(router, QLatin1String(kAppName));

        const QString sessionId = sessions.createSession(QStringLiteral("Hardware Single"));
        QVERIFY(sessions.addDevice(sessionId, address, QStringLiteral("Center")) == SessionCommandResult::Accepted);
        QVERIFY(sessions.setSource(sessionId, sourceId) == SessionCommandResult::Accepted);
        QVERIFY(sessions.activateSession(sessionId) == SessionCommandResult::Accepted);

        const bool reached = waitUntil(
            [&]() {
                const auto session = sessions.sessionById(sessionId);
                return session.has_value()
                    && (session->state == SessionState::Active || session->state == SessionState::Degraded);
            },
            15000);
        const auto session = sessions.sessionById(sessionId);
        QVERIFY(session.has_value());
        qInfo("Single-device session state=%d error=%d detail=%s",
              static_cast<int>(session->state),
              static_cast<int>(session->error.category),
              qPrintable(session->error.detail));
        QVERIFY2(reached, "Single connected mapped device never reached Active or Degraded");
        QVERIFY(sessions.deactivateSession(sessionId) == SessionCommandResult::Accepted);
        tone.stop();
        sessions.shutdown();
        pipeWire.shutdown();
        bluetooth.shutdown();
    }

    void twoDevicePairConnectSessionAndRouteIsolation()
    {
        if (qEnvironmentVariableIntValue("AURALIS_RUN_PHASE7_HARDWARE") != 1) {
            QSKIP("Set AURALIS_RUN_PHASE7_HARDWARE=1 to run two-device live hardware");
        }

        QStringList addresses;
        const QString raw = QString::fromLocal8Bit(qgetenv("AURALIS_EXPECT_DEVICE_ADDRESSES")).trimmed();
        for (const QString& part : raw.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QString n = part.trimmed().toUpper();
            if (!n.isEmpty()) {
                addresses.push_back(n);
            }
        }
        QVERIFY2(addresses.size() >= 2, "Need AURALIS_EXPECT_DEVICE_ADDRESSES with two MAC addresses");

        BluetoothManager bluetooth;
        QVERIFY(bluetooth.initialize());
        PipeWireManager pipeWire(bluetooth.deviceRegistry());
        QVERIFY(pipeWire.initialize());
        QTemporaryDir dir;
        SessionManager sessions(&bluetooth, &pipeWire, dir.filePath(QStringLiteral("sessions.json")));
        QVERIFY(sessions.initialize());
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth.agentRegistered(), 5000);
        QVERIFY(bluetooth.canStartScan());

        auto* model = bluetooth.devices();
        QVERIFY(model != nullptr);
        bluetooth.startScan();
        QTRY_VERIFY_WITH_TIMEOUT(bluetooth.scanning() || !bluetooth.errorText().isEmpty(), 4000);
        QVERIFY2(bluetooth.scanning(), qPrintable(bluetooth.errorText()));

        QStringList connected;
        QStringList paths;
        for (const QString& address : addresses) {
            QVERIFY2(
                waitUntil([&]() { return !pathForAddress(model, address).isEmpty(); }, 25000, &bluetooth),
                qPrintable(QStringLiteral("Device %1 not observed during scan").arg(address)));
            const QString path = pathForAddress(model, address);
            QVERIFY(bluetooth.hasDevice(path));
            const QVariantMap details = bluetooth.deviceDetails(path);
            QVERIFY(!details.value(QStringLiteral("address")).toString().isEmpty());
            qInfo("Hardware details %s name=%s uuids=%d",
                  qPrintable(address),
                  qPrintable(details.value(QStringLiteral("displayName")).toString()),
                  static_cast<int>(details.value(QStringLiteral("uuids")).toStringList().size()));

            if (!roleValue(model, path, BluetoothDeviceListModel::PairedRole).toBool()) {
                bluetooth.pairDevice(path);
                QVERIFY2(
                    waitUntil(
                        [&]() { return roleValue(model, path, BluetoothDeviceListModel::PairedRole).toBool(); },
                        120000,
                        &bluetooth),
                    qPrintable(QStringLiteral("Pair failed %1").arg(address)));
            }
            if (!roleValue(model, path, BluetoothDeviceListModel::TrustedRole).toBool()) {
                bluetooth.trustDevice(path);
                waitUntil(
                    [&]() { return roleValue(model, path, BluetoothDeviceListModel::TrustedRole).toBool(); },
                    15000,
                    &bluetooth);
            }
            bluetooth.connectDevice(path);
            const bool ok = waitUntil(
                [&]() { return roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool(); },
                60000,
                &bluetooth);
            if (!ok) {
                QWARN(qPrintable(QStringLiteral("Connect failed %1").arg(address)));
                continue;
            }
            connected.push_back(address);
            paths.push_back(path);
        }
        QVERIFY2(!connected.isEmpty(), "Neither device connected through BluetoothManager");
        qInfo("Connected %d/%d devices", static_cast<int>(connected.size()), static_cast<int>(addresses.size()));

        QVERIFY(waitUntil(
            [&]() {
                return pipeWire.connectionState() == PipeWireConnectionState::Connected
                    && pipeWire.initialSyncComplete();
            },
            8000));

        int mapped = 0;
        waitUntil(
            [&]() {
                mapped = 0;
                for (const auto& endpoint : pipeWire.endpointRegistry()->mappedBluetoothEndpoints()) {
                    if (addresses.contains(endpoint.bluetoothAddress.toUpper())
                        && endpoint.availability == AudioEndpointAvailability::Available) {
                        ++mapped;
                    }
                }
                return mapped >= 1;
            },
            20000);
        qInfo("Mapped Bluetooth endpoints for expected devices: %d (pipewire mappedBt=%d)",
              mapped,
              pipeWire.mappedBluetoothCount());
        QVERIFY2(mapped >= 1, qPrintable(pipeWire.diagnosticsText()));

        TestToneStream tone;
        QVERIFY2(tone.start(), "Failed to start test tone source");
        AudioRouter* router = pipeWire.audioRouter();
        QVERIFY(router != nullptr);
        QVERIFY2(
            waitUntil(
                [router]() {
                    for (const auto& source : router->sourceList()) {
                        if (source.applicationName == QLatin1String(kAppName)) {
                            return true;
                        }
                    }
                    return false;
                },
                8000),
            "Test tone never appeared as AudioRouter source");
        QString sourceId;
        for (const auto& source : router->sourceList()) {
            if (source.applicationName == QLatin1String(kAppName)) {
                sourceId = source.id;
                break;
            }
        }

        const QString sessionId = sessions.createSession(QStringLiteral("Hardware Pair"));
        QVERIFY(!sessionId.isEmpty());
        QVERIFY(sessions.addDevice(sessionId, addresses.at(0), QStringLiteral("Left")) == SessionCommandResult::Accepted);
        QVERIFY(sessions.addDevice(sessionId, addresses.at(1), QStringLiteral("Right")) == SessionCommandResult::Accepted);
        QVERIFY(sessions.setSource(sessionId, sourceId) == SessionCommandResult::Accepted);
        QVERIFY(sessions.setGroupVolume(sessionId, 0.35) == SessionCommandResult::Accepted);
        QVERIFY(sessions.setRecoveryPolicy(sessionId, QStringLiteral("ReconnectAndRestore"))
                == SessionCommandResult::Accepted);
        QVERIFY(sessions.activateSession(sessionId) == SessionCommandResult::Accepted);

        QVERIFY2(
            waitUntil(
                [&]() {
                    const auto session = sessions.sessionById(sessionId);
                    return session.has_value()
                        && (session->state == SessionState::Active || session->state == SessionState::Degraded);
                },
                20000),
            "Session never reached Active or Degraded");
        const auto live = sessions.sessionById(sessionId);
        QVERIFY(live.has_value());
        qInfo("Session state=%d routes members=%d", static_cast<int>(live->state), static_cast<int>(live->devices.size()));
        QVERIFY(live->state != SessionState::Failed);

        QVector<auralis::audio::AudioRoute> sessionRoutes;
        for (const auto& route : router->routes()) {
            if (route.ownerType == RouteOwnerType::Session && route.ownerId == sessionId) {
                sessionRoutes.push_back(route);
            }
        }
        QVERIFY2(!sessionRoutes.isEmpty(), "No Session-owned routes after activate");

        QString spareDest;
        for (const auto& endpoint : pipeWire.endpointRegistry()->playbackEndpoints()) {
            if (endpoint.availability != AudioEndpointAvailability::Available) {
                continue;
            }
            if (endpoint.direction != AudioEndpointDirection::Playback
                && endpoint.direction != AudioEndpointDirection::Duplex) {
                continue;
            }
            bool used = false;
            for (const auto& route : sessionRoutes) {
                if (route.destinationIds.contains(endpoint.id)) {
                    used = true;
                    break;
                }
            }
            if (!used) {
                spareDest = endpoint.id;
                break;
            }
        }
        if (spareDest.isEmpty()) {
            QWARN("No spare playback destination; skipping extra manual route, still mutating planner if possible");
        } else {
            const QString snapshotSource = sessionRoutes.front().sourceId;
            const QStringList snapshotDests = sessionRoutes.front().destinationIds;
            const QString snapshotId = sessionRoutes.front().id;
            const QString manualId = router->createRoute(sourceId, {spareDest});
            QVERIFY(!manualId.isEmpty());
            QCOMPARE(router->currentRouteId(), manualId);
            router->setRouteVolume(manualId, 0.22);
            router->setRouteMuted(manualId, true);
            const auto sessionAfter = router->routeById(snapshotId);
            QVERIFY(sessionAfter.has_value());
            QCOMPARE(sessionAfter->sourceId, snapshotSource);
            QCOMPARE(sessionAfter->destinationIds, snapshotDests);
            QVERIFY(sessionAfter->ownerType == RouteOwnerType::Session);
            QSignalSpy errors(router, &AudioRouter::routeError);
            router->setRouteVolume(snapshotId, 0.05);
            QCOMPARE(router->routeById(snapshotId)->volume, sessionAfter->volume);
            QVERIFY(errors.count() >= 1);
        }

        auto* selected = qobject_cast<SelectedSessionViewModel*>(sessions.selectedSession());
        QVERIFY(selected != nullptr);
        const QString idleId = sessions.createSession(QStringLiteral("Idle Editor"));
        selected->setSessionId(idleId);
        QCOMPARE(selected->name(), QStringLiteral("Idle Editor"));
        QVERIFY(sessions.setGroupVolume(idleId, 0.12) == SessionCommandResult::Accepted);
        QCOMPARE(selected->groupVolume(), 0.12);
        QCOMPARE(sessions.sessionById(sessionId)->groupVolume, 0.35);
        QCOMPARE(sessions.currentSessionId(), sessionId);

        const QString copyId = sessions.duplicateSession(sessionId);
        QVERIFY(!copyId.isEmpty());
        QVERIFY(copyId != sessionId);
        QVERIFY(sessions.sessionById(copyId)->state == SessionState::Idle);
        QVERIFY(sessions.sessionById(copyId)->devices.front().runtime.routeId.isEmpty());
        QCOMPARE(sessions.sessionById(sessionId)->name, QStringLiteral("Hardware Pair"));

        QVERIFY(sessions.deactivateSession(sessionId) == SessionCommandResult::Accepted);
        QVERIFY(waitUntil(
            [&]() {
                const auto session = sessions.sessionById(sessionId);
                return session.has_value() && session->state == SessionState::Idle;
            },
            10000));

        tone.stop();
        bluetooth.stopScan();
        for (const QString& path : paths) {
            bluetooth.disconnectDevice(path);
            waitUntil(
                [&]() { return !roleValue(model, path, BluetoothDeviceListModel::ConnectedRole).toBool(); },
                15000,
                &bluetooth);
        }
        sessions.shutdown();
        pipeWire.shutdown();
        bluetooth.shutdown();
    }
};

QTEST_GUILESS_MAIN(TstPhase7HardwareLive)
#include "tst_Phase7HardwareLive.moc"
