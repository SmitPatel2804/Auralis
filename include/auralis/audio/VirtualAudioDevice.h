#pragma once

#include <QString>
#include <QStringList>
#include <QStringView>

namespace auralis::audio {

struct VirtualAudioDeviceState {
    bool renderAvailable = false;
    bool monitorAvailable = false;
    bool selectedAsDefault = false;

    // A standard WaveRT render endpoint exposes its Windows mix through
    // WASAPI loopback, so a separate capture/monitor endpoint is unnecessary.
    bool ready() const noexcept { return renderAvailable; }
};

bool isAuralisVirtualRender(QStringView description);
bool isAuralisVirtualMonitor(QStringView description);
VirtualAudioDeviceState inspectVirtualAudioDevices(
    const QStringList& renderDescriptions,
    const QStringList& captureDescriptions,
    QStringView defaultRenderDescription);

} // namespace auralis::audio
