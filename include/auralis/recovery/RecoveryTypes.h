#pragma once

#include <QString>

#include <QtGlobal>

namespace auralis::recovery {

enum class RecoveryDomain {
    SystemBus,
    BluetoothService,
    BluetoothAdapter,
    BluetoothDevice,
    PipeWire,
    AudioGraph,
    Session,
    SuspendResume
};

enum class RecoveryState {
    Healthy,
    Waiting,
    Recovering,
    Reconciling,
    Degraded,
    Exhausted,
    Suspended
};

enum class RecoveryCause {
    None,
    ServiceVanished,
    ServiceRestarted,
    AdapterRemoved,
    DeviceDisconnected,
    EndpointRemoved,
    GraphReset,
    ProfileChanged,
    Timeout,
    Resume,
    Unknown
};

struct RecoveryStatus {
    RecoveryState overall = RecoveryState::Healthy;
    RecoveryState bluetooth = RecoveryState::Healthy;
    RecoveryState pipeWire = RecoveryState::Healthy;
    RecoveryCause lastCause = RecoveryCause::None;
    QString lastError;
    quint64 generation = 0;
    quint64 suspendEpoch = 0;
    int pipeWireAttempts = 0;
    bool suspended = false;
    bool autoRecoverEnabled = true;
};

QString toString(RecoveryDomain domain);
QString toString(RecoveryState state);
QString toString(RecoveryCause cause);
QString userFacingStatus(const RecoveryStatus& status);

} // namespace auralis::recovery
