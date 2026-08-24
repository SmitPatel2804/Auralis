#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
#include <auralis/audio/PipeWireVirtualOutput.h>
#include <auralis/bluetooth/BluetoothDeviceListModel.h>
#include <auralis/bluetooth/BluetoothManager.h>
#include <auralis/session/SessionManager.h>
#include <auralis/session/SessionTypes.h>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <functional>

using auralis::audio::AudioEndpointAvailability;
using auralis::audio::AudioEndpointDirection;
using auralis::audio::PipeWireConnectionState;
using auralis::audio::PipeWireManager;
using auralis::bluetooth::BluetoothDeviceListModel;
using auralis::bluetooth::BluetoothManager;
using auralis::session::SessionCommandResult;
using auralis::session::SessionManager;
using auralis::session::SessionState;

namespace {

constexpr auto kBuds = "88:08:94:9D:B4:22";
constexpr auto kRockerz = "EE:D0:0D:A4:1D:DA";
constexpr auto kSound = "/usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga";
constexpr auto kPwPlayApp = "pw-play";

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

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents();
        QThread::msleep(40);
    }
    return predicate();
}

bool bluetoothSinksHaveInputPorts()
{
    QProcess probe;
    probe.start(QStringLiteral("python3"), {QStringLiteral("-c"), QStringLiteral(
        "import json,subprocess,sys\n"
        "need={'88:08:94:9D:B4:22','EE:D0:0D:A4:1D:DA'}\n"
        "objs=json.loads(subprocess.check_output(['pw-dump'],text=True))\n"
        "ok=set()\n"
        "for o in objs:\n"
        " info=o.get('info') or {}\n"
        " p=info.get('props') or {}\n"
        " addr=(p.get('api.bluez5.address') or '').upper()\n"
        " if addr in need and int(info.get('n-input-ports') or info.get('n_input_ports') or 0)>=1:\n"
        "  ok.add(addr)\n"
        "sys.exit(0 if ok==need else 1)\n")});
    if (!probe.waitForFinished(8000)) {
        probe.kill();
        return false;
    }
    return probe.exitCode() == 0;
}

void printLatencySnapshot()
{
    std::fprintf(stderr,
                 "Latency snapshot (graph clock.quantum=1024 @ 48kHz = 21.3ms; AAC A2DP advertised ~189ms):\n");
    QProcess dump;
    dump.start(QStringLiteral("python3"), {QStringLiteral("-c"), QStringLiteral(
        "import json,subprocess\n"
        "objs=json.loads(subprocess.check_output(['pw-dump'],text=True))\n"
        "for o in objs:\n"
        " info=o.get('info') or {}\n"
        " p=info.get('props') or {}\n"
        " name=p.get('node.name','')\n"
        " if 'pw-play' not in name and 'bluez_output' not in name:\n"
        "  continue\n"
        " ns=None\n"
        " lat=info.get('params',{}).get('Latency')\n"
        " if isinstance(lat,list):\n"
        "  for e in lat:\n"
        "   if e.get('direction')=='Input' and e.get('minNs'): ns=e.get('minNs')\n"
        " print(f\"  {name} node.latency={p.get('node.latency')} codec={p.get('api.bluez5.codec')} "
        "advertised_in_ms={(ns/1e6) if ns else None}\")\n")});
    if (!dump.waitForFinished(8000)) {
        dump.kill();
        std::fprintf(stderr, "  (pw-dump snapshot timed out)\n");
        return;
    }
    std::fprintf(stderr, "%s", dump.readAllStandardOutput().constData());
    const QByteArray err = dump.readAllStandardError();
    if (!err.isEmpty()) {
        std::fprintf(stderr, "%s", err.constData());
    }
}

void stopPlayer(QProcess& player)
{
    if (player.state() == QProcess::NotRunning) {
        return;
    }
    // GNU timeout owns a process group. Stop that group so its shell/pw-play
    // descendants cannot outlive an early assertion or the helper itself.
    const qint64 processGroup = player.processId();
    if (processGroup > 0) {
        ::kill(-static_cast<pid_t>(processGroup), SIGTERM);
    }
    if (player.waitForFinished(2000)) {
        return;
    }
    if (processGroup > 0) {
        ::kill(-static_cast<pid_t>(processGroup), SIGKILL);
    }
    player.kill();
    player.waitForFinished(2000);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const bool systemAudio = app.arguments().contains(QStringLiteral("--system-audio"));
    const bool probeButtonPolicy = app.arguments().contains(QStringLiteral("--probe-button-policy"));
    const bool probeServiceRecovery = app.arguments().contains(QStringLiteral("--probe-service-recovery"));

    BluetoothManager bluetooth;
    if (!bluetooth.initialize()) {
        std::fprintf(stderr, "BluetoothManager failed to initialize\n");
        return 1;
    }
    PipeWireManager pipeWire(bluetooth.deviceRegistry());
    if (!pipeWire.initialize()) {
        std::fprintf(stderr, "PipeWireManager failed to initialize\n");
        return 1;
    }
    QTemporaryDir dir;
    SessionManager sessions(&bluetooth, &pipeWire, dir.filePath(QStringLiteral("sessions.json")));
    if (!sessions.initialize()) {
        std::fprintf(stderr, "SessionManager failed to initialize\n");
        return 1;
    }

    if (!waitUntil([&]() { return bluetooth.agentRegistered(); }, 5000)) {
        std::fprintf(stderr, "Bluetooth agent not registered\n");
        return 1;
    }

    auto* model = bluetooth.devices();
    const QString budsPath = pathForAddress(model, QString::fromLatin1(kBuds));
    const QString rockerzPath = pathForAddress(model, QString::fromLatin1(kRockerz));
    if (budsPath.isEmpty() || rockerzPath.isEmpty()) {
        std::fprintf(stderr, "Need both Smokin' Buds and Rockerz in BlueZ\n");
        return 1;
    }
    bluetooth.connectDevice(budsPath);
    bluetooth.connectDevice(rockerzPath);

    if (!waitUntil(
            [&]() {
                return pipeWire.connectionState() == PipeWireConnectionState::Connected
                    && pipeWire.initialSyncComplete();
            },
            8000)) {
        std::fprintf(stderr, "PipeWire not connected\n");
        return 1;
    }

    const QStringList wanted{QString::fromLatin1(kBuds).toUpper(), QString::fromLatin1(kRockerz).toUpper()};
    if (!waitUntil(
            [&]() {
                int mappedPlayback = 0;
                for (const auto& endpoint : pipeWire.endpointRegistry()->mappedBluetoothEndpoints()) {
                    if (endpoint.availability != AudioEndpointAvailability::Available) {
                        continue;
                    }
                    if (endpoint.direction != AudioEndpointDirection::Playback
                        && endpoint.direction != AudioEndpointDirection::Duplex) {
                        continue;
                    }
                    if (wanted.contains(endpoint.bluetoothAddress.toUpper())) {
                        ++mappedPlayback;
                    }
                }
                return mappedPlayback >= 2;
            },
            20000)) {
        std::fprintf(stderr, "Both Bluetooth playback endpoints did not map:\n%s\n",
                     qPrintable(pipeWire.diagnosticsText()));
        return 1;
    }

    if (!waitUntil([]() { return bluetoothSinksHaveInputPorts(); }, 20000)) {
        std::fprintf(stderr, "Bluetooth sinks mapped but A2DP input ports were not ready\n");
        return 1;
    }

    QProcess player;
    player.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    player.start(
        QStringLiteral("timeout"),
        {QStringLiteral("--kill-after=2s"),
         probeServiceRecovery ? QStringLiteral("45s") : QStringLiteral("14s"),
         QStringLiteral("bash"),
         QStringLiteral("-lc"),
         QStringLiteral("for i in $(seq 1 %3); do pw-play --target '%1' --latency 21ms --volume 1.0 '%2'; done")
             .arg(systemAudio ? QStringLiteral("auralis_virtual_output") : QStringLiteral("0"),
                  QString::fromLatin1(kSound),
                  probeServiceRecovery ? QStringLiteral("20") : QStringLiteral("5"))});
    if (!player.waitForStarted(3000)) {
        std::fprintf(stderr, "Failed to start pw-play\n");
        return 1;
    }

    auto* router = pipeWire.audioRouter();
    QString sourceId;
    if (!waitUntil(
            [&]() {
                sourceId.clear();
                for (const auto& source : router->sourceList()) {
                    if (systemAudio
                        && source.nodeName == QLatin1String(auralis::audio::kAuralisVirtualSourceNodeName)) {
                        sourceId = source.id;
                        return true;
                    }
                    if (source.nodeName.contains(QLatin1String("pw-play"), Qt::CaseInsensitive)
                        || source.applicationName.contains(QLatin1String("pw-play"), Qt::CaseInsensitive)
                        || source.description.contains(QLatin1String("pw-play"), Qt::CaseInsensitive)) {
                        sourceId = source.id;
                        return true;
                    }
                }
                return false;
            },
            8000)) {
        std::fprintf(stderr, "pw-play stream never appeared as an AudioRouter source\n");
        stopPlayer(player);
        return 1;
    }

    const QString sessionId = sessions.createSession(QStringLiteral("Both headsets live"));
    if (sessions.addDevice(sessionId, QString::fromLatin1(kBuds), QStringLiteral("Left"))
            != SessionCommandResult::Accepted
        || sessions.addDevice(sessionId, QString::fromLatin1(kRockerz), QStringLiteral("Right"))
            != SessionCommandResult::Accepted
        || sessions.setSource(sessionId, sourceId) != SessionCommandResult::Accepted
        || sessions.setGroupVolume(sessionId, 0.9) != SessionCommandResult::Accepted
        || sessions.activateSession(sessionId) != SessionCommandResult::Accepted) {
        std::fprintf(stderr, "Session setup/activate rejected\n");
        stopPlayer(player);
        return 1;
    }

    if (!waitUntil(
            [&]() {
                const auto session = sessions.sessionById(sessionId);
                return session.has_value()
                    && (session->state == SessionState::Active || session->state == SessionState::Degraded);
            },
            15000)) {
        const auto session = sessions.sessionById(sessionId);
        std::fprintf(stderr, "Session did not go Active/Degraded (state=%d)\n",
                     session ? static_cast<int>(session->state) : -1);
        stopPlayer(player);
        sessions.deactivateSession(sessionId);
        return 1;
    }

    const auto live = sessions.sessionById(sessionId);
    std::printf("Session state=%d members=%d source=%s\n",
                static_cast<int>(live->state),
                static_cast<int>(live->devices.size()),
                qPrintable(sourceId));
    int activeMembers = 0;
    for (const auto& device : live->devices) {
        std::printf("  member %s connected=%d endpoint=%s routeActive=%d\n",
                    qPrintable(device.deviceId),
                    device.runtime.connected ? 1 : 0,
                    qPrintable(device.runtime.endpointId),
                    device.runtime.routeActive ? 1 : 0);
        if (device.runtime.routeActive) {
            ++activeMembers;
        }
    }
    if (activeMembers < 2) {
        std::fprintf(stderr, "Expected both members to have an Active route (got %d)\n", activeMembers);
        stopPlayer(player);
        sessions.deactivateSession(sessionId);
        return 1;
    }

    if (probeButtonPolicy) {
        for (const QString& path : {budsPath, rockerzPath}) {
            bluetooth.setDeviceButtonPolicy(path, true);
            QCoreApplication::processEvents();
            const QVariantMap details = bluetooth.deviceDetails(path);
            const bool controllable = details.value(QStringLiteral("canControlButtons")).toBool();
            const QString effective = details.value(QStringLiteral("buttonEffectiveStatus")).toString();
            std::printf(
                "  button probe device=%s policy=%s controllable=%d effective=%s\n",
                qPrintable(details.value(QStringLiteral("displayName")).toString()),
                qPrintable(details.value(QStringLiteral("buttonPolicy")).toString()),
                controllable ? 1 : 0,
                qPrintable(effective));
            if (controllable || !effective.contains(QLatin1String("Unsupported"))) {
                std::fprintf(stderr, "Expected an honestly Unsupported button path on this host\n");
                bluetooth.setDeviceButtonPolicy(path, false);
                stopPlayer(player);
                sessions.deactivateSession(sessionId);
                return 1;
            }
            bluetooth.setDeviceButtonPolicy(path, false);
        }
        const auto afterProbe = sessions.sessionById(sessionId);
        if (!afterProbe.has_value() || afterProbe->state != SessionState::Active) {
            std::fprintf(stderr, "Button-policy probe disturbed the active audio session\n");
            stopPlayer(player);
            sessions.deactivateSession(sessionId);
            return 1;
        }
    }

    std::fprintf(
        stderr,
        "Playing freedesktop alarm on BOTH headsets for ~10s via %s. Listen now.\n",
        systemAudio ? "Auralis System Audio" : "direct application capture");
    std::fflush(stderr);

    if (probeServiceRecovery) {
        if (!systemAudio) {
            std::fprintf(stderr, "Service-recovery probe requires --system-audio for stable source identity\n");
            stopPlayer(player);
            sessions.deactivateSession(sessionId);
            return 1;
        }

        bool probeArmed = true;
        bool disruptionObserved = false;
        const auto sessionRecovered = [&]() {
            const auto current = sessions.sessionById(sessionId);
            if (!current.has_value() || current->state != SessionState::Active
                || pipeWire.connectionState() != PipeWireConnectionState::Connected || !pipeWire.initialSyncComplete()) {
                return false;
            }
            const auto sources = router->sourceList();
            const bool stableSource = std::any_of(
                sources.cbegin(),
                sources.cend(),
                [&](const auto& source) { return source.id == sourceId && source.available; });
            if (!stableSource) {
                return false;
            }
            return std::count_if(
                       current->devices.cbegin(), current->devices.cend(), [](const auto& device) {
                           return device.runtime.connected && device.runtime.endpointAvailable
                               && device.runtime.routeActive;
                       })
                == 2;
        };
        const auto observeDisruption = [&]() {
            if (probeArmed && !sessionRecovered()) {
                disruptionObserved = true;
            }
        };
        QObject probeContext;
        const QMetaObject::Connection connectionStateConnection =
            QObject::connect(&pipeWire, &PipeWireManager::connectionStateChanged, &probeContext, observeDisruption);
        const QMetaObject::Connection graphRevisionConnection =
            QObject::connect(&pipeWire, &PipeWireManager::graphRevisionChanged, &probeContext, observeDisruption);
        const QMetaObject::Connection sessionStateConnection = QObject::connect(
            &sessions,
            &SessionManager::sessionStateChanged,
            &probeContext,
            [&](const QString& changedId, SessionState, SessionState) {
                if (changedId == sessionId) {
                    observeDisruption();
                }
            });

        std::fprintf(stderr, "SERVICE_RECOVERY_PROBE_READY\n");
        std::fflush(stderr);
        if (!waitUntil([&]() { return disruptionObserved; }, 15000)) {
            std::fprintf(stderr, "No service/graph disruption was observed within 15s\n");
            stopPlayer(player);
            sessions.deactivateSession(sessionId);
            return 1;
        }
        std::fprintf(stderr, "SERVICE_RECOVERY_DISRUPTION_OBSERVED\n");
        std::fflush(stderr);
        if (!waitUntil(sessionRecovered, 30000)) {
            const auto failed = sessions.sessionById(sessionId);
            std::fprintf(
                stderr,
                "Service recovery did not restore both routes (sessionState=%d):\n%s\n",
                failed ? static_cast<int>(failed->state) : -1,
                qPrintable(pipeWire.diagnosticsText()));
            stopPlayer(player);
            sessions.deactivateSession(sessionId);
            return 1;
        }
        QElapsedTimer stableTimer;
        stableTimer.start();
        while (stableTimer.elapsed() < 1000) {
            QCoreApplication::processEvents();
            if (!sessionRecovered()) {
                std::fprintf(stderr, "Recovered session did not remain stable for 1s\n");
                stopPlayer(player);
                sessions.deactivateSession(sessionId);
                return 1;
            }
            QThread::msleep(40);
        }
        probeArmed = false;
        const auto recovered = sessions.sessionById(sessionId);
        const int recoveredMembers = static_cast<int>(std::count_if(
            recovered->devices.cbegin(), recovered->devices.cend(), [](const auto& device) {
                return device.runtime.connected && device.runtime.endpointAvailable && device.runtime.routeActive;
            }));
        std::fprintf(
            stderr,
            "SERVICE_RECOVERY_PASS session=Active members=%d ownedLinks=%d source=%s\n",
            recoveredMembers,
            router->ownedLinkCount(),
            qPrintable(sourceId));
        std::fflush(stderr);
        QObject::disconnect(connectionStateConnection);
        QObject::disconnect(graphRevisionConnection);
        QObject::disconnect(sessionStateConnection);
    }

    QElapsedTimer playTimer;
    playTimer.start();
    while (playTimer.elapsed() < 1500) {
        QCoreApplication::processEvents();
        QThread::msleep(40);
    }
    printLatencySnapshot();

    player.waitForFinished(17000);
    sessions.deactivateSession(sessionId);
    stopPlayer(player);
    sessions.shutdown();
    pipeWire.shutdown();
    bluetooth.shutdown();
    std::printf("Done.\n");
    return 0;
}
