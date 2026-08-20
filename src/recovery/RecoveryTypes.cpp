#include <auralis/recovery/RecoveryTypes.h>

namespace auralis::recovery {

QString toString(RecoveryDomain domain)
{
    switch (domain) {
    case RecoveryDomain::SystemBus:
        return QStringLiteral("SystemBus");
    case RecoveryDomain::BluetoothService:
        return QStringLiteral("BluetoothService");
    case RecoveryDomain::BluetoothAdapter:
        return QStringLiteral("BluetoothAdapter");
    case RecoveryDomain::BluetoothDevice:
        return QStringLiteral("BluetoothDevice");
    case RecoveryDomain::PipeWire:
        return QStringLiteral("PipeWire");
    case RecoveryDomain::AudioGraph:
        return QStringLiteral("AudioGraph");
    case RecoveryDomain::Session:
        return QStringLiteral("Session");
    case RecoveryDomain::SuspendResume:
        return QStringLiteral("SuspendResume");
    }
    return QStringLiteral("Unknown");
}

QString toString(RecoveryState state)
{
    switch (state) {
    case RecoveryState::Healthy:
        return QStringLiteral("Healthy");
    case RecoveryState::Waiting:
        return QStringLiteral("Waiting");
    case RecoveryState::Recovering:
        return QStringLiteral("Recovering");
    case RecoveryState::Reconciling:
        return QStringLiteral("Reconciling");
    case RecoveryState::Degraded:
        return QStringLiteral("Degraded");
    case RecoveryState::Exhausted:
        return QStringLiteral("Exhausted");
    case RecoveryState::Suspended:
        return QStringLiteral("Suspended");
    }
    return QStringLiteral("Unknown");
}

QString toString(RecoveryCause cause)
{
    switch (cause) {
    case RecoveryCause::None:
        return QStringLiteral("None");
    case RecoveryCause::ServiceVanished:
        return QStringLiteral("ServiceVanished");
    case RecoveryCause::ServiceRestarted:
        return QStringLiteral("ServiceRestarted");
    case RecoveryCause::AdapterRemoved:
        return QStringLiteral("AdapterRemoved");
    case RecoveryCause::DeviceDisconnected:
        return QStringLiteral("DeviceDisconnected");
    case RecoveryCause::EndpointRemoved:
        return QStringLiteral("EndpointRemoved");
    case RecoveryCause::GraphReset:
        return QStringLiteral("GraphReset");
    case RecoveryCause::ProfileChanged:
        return QStringLiteral("ProfileChanged");
    case RecoveryCause::Timeout:
        return QStringLiteral("Timeout");
    case RecoveryCause::Resume:
        return QStringLiteral("Resume");
    case RecoveryCause::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString userFacingStatus(const RecoveryStatus& status)
{
    if (status.suspended) {
        return QStringLiteral("Paused for sleep");
    }
    if (status.overall == RecoveryState::Exhausted) {
        if (!status.lastError.isEmpty()) {
            return QStringLiteral("Recovery exhausted: %1").arg(status.lastError);
        }
        return QStringLiteral("Recovery exhausted — check Bluetooth and audio services");
    }
    if (status.overall == RecoveryState::Recovering || status.overall == RecoveryState::Waiting) {
        if (status.pipeWire == RecoveryState::Recovering) {
            if (status.pipeWireAttempts > 0) {
                return QStringLiteral("Reconnecting audio service (attempt %1)")
                    .arg(status.pipeWireAttempts);
            }
            return QStringLiteral("Preparing to reconnect audio service");
        }
        if (status.bluetooth == RecoveryState::Recovering || status.bluetooth == RecoveryState::Waiting) {
            return QStringLiteral("Waiting for Bluetooth service");
        }
        return QStringLiteral("Recovering services");
    }
    if (status.overall == RecoveryState::Reconciling) {
        return QStringLiteral("Restoring session after recovery");
    }
    if (status.overall == RecoveryState::Degraded) {
        return QStringLiteral("Running degraded — some services unavailable");
    }
    return QStringLiteral("All services healthy");
}

} // namespace auralis::recovery
