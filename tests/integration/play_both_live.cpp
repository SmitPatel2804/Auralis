#include <auralis/audio/AudioEndpoint.h>
#include <auralis/audio/AudioEndpointRegistry.h>
#include <auralis/audio/AudioRouter.h>
#include <auralis/audio/PipeWireManager.h>
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

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

    QProcess player;
    player.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    player.start(
        QStringLiteral("bash"),
        {QStringLiteral("-lc"),
         QStringLiteral("for i in 1 2 3 4 5; do pw-play --target 0 --volume 1.0 '%1'; done").arg(kSound)});
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
        player.kill();
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
        player.kill();
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
        player.kill();
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
        player.kill();
        sessions.deactivateSession(sessionId);
        return 1;
    }

    std::fprintf(stderr, "Playing freedesktop alarm on BOTH headsets for ~10s. Listen now.\n");
    std::fflush(stderr);
    player.waitForFinished(12000);
    sessions.deactivateSession(sessionId);
    if (player.state() != QProcess::NotRunning) {
        player.terminate();
        player.waitForFinished(2000);
    }
    sessions.shutdown();
    pipeWire.shutdown();
    bluetooth.shutdown();
    std::printf("Done.\n");
    return 0;
}
