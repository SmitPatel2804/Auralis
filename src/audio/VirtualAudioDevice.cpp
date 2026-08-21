#include <auralis/audio/VirtualAudioDevice.h>

namespace auralis::audio {
namespace {

QString normalized(QStringView value)
{
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        if (character.isLetterOrNumber()) {
            result.append(character.toLower());
        }
    }
    return result;
}

} // namespace

bool isAuralisVirtualRender(QStringView description)
{
    const QString value = normalized(description);
    return value.contains(QStringLiteral("auralisvirtualoutput"))
        || value.contains(QStringLiteral("auralisvirtualspeaker"));
}

bool isAuralisVirtualMonitor(QStringView description)
{
    const QString value = normalized(description);
    return value.contains(QStringLiteral("auralisvirtualmonitor"))
        || value.contains(QStringLiteral("auralisvirtualcapture"));
}

VirtualAudioDeviceState inspectVirtualAudioDevices(
    const QStringList& renderDescriptions,
    const QStringList& captureDescriptions,
    QStringView defaultRenderDescription)
{
    VirtualAudioDeviceState state;
    for (const QString& description : renderDescriptions) {
        state.renderAvailable = state.renderAvailable || isAuralisVirtualRender(description);
    }
    for (const QString& description : captureDescriptions) {
        state.monitorAvailable = state.monitorAvailable || isAuralisVirtualMonitor(description);
    }
    state.selectedAsDefault = state.renderAvailable && isAuralisVirtualRender(defaultRenderDescription);
    return state;
}

} // namespace auralis::audio
