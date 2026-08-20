#include <auralis/recovery/RecoveryManager.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(auralisRecovery, "auralis.recovery")

namespace auralis::recovery {

RecoveryManager::RecoveryManager(QObject* parent)
    : QObject(parent)
{
    coalesceTimer_.setSingleShot(true);
    coalesceTimer_.setInterval(150);
    connect(&coalesceTimer_, &QTimer::timeout, this, &RecoveryManager::runReconcile);
}

RecoveryManager::~RecoveryManager()
{
    shutdown();
}

void RecoveryManager::setHooks(HostHooks hooks)
{
    hooks_ = std::move(hooks);
}

bool RecoveryManager::initialize()
{
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    shuttingDown_ = false;
    bumpGeneration();
    refreshObservedHealth();

    status_.bluetooth = blueZAvailable_ ? RecoveryState::Healthy : RecoveryState::Waiting;
    status_.pipeWire = (pipeWireConnected_ && pipeWireGraphReady_) ? RecoveryState::Healthy : RecoveryState::Waiting;
    if (!blueZAvailable_ || !pipeWireConnected_ || !pipeWireGraphReady_) {
        status_.overall = RecoveryState::Degraded;
    } else {
        status_.overall = RecoveryState::Healthy;
    }
    emitStatus();
    qCInfo(auralisRecovery) << "RecoveryManager initialized";
    return true;
}

void RecoveryManager::shutdown()
{
    if (!initialized_ && !shuttingDown_) {
        return;
    }
    shuttingDown_ = true;
    coalesceTimer_.stop();
    pendingReconcile_ = false;
    bumpGeneration();
    initialized_ = false;
    qCInfo(auralisRecovery) << "RecoveryManager shut down";
}

bool RecoveryManager::autoRecoverEnabled() const noexcept
{
    return status_.autoRecoverEnabled;
}

void RecoveryManager::setAutoRecoverEnabled(bool enabled)
{
    if (status_.autoRecoverEnabled == enabled) {
        return;
    }
    status_.autoRecoverEnabled = enabled;
    emit autoRecoverEnabledChanged();
    emitStatus();
}

bool RecoveryManager::restoreOnResume() const noexcept
{
    return restoreOnResume_;
}

void RecoveryManager::setRestoreOnResume(bool enabled)
{
    restoreOnResume_ = enabled;
}

RecoveryStatus RecoveryManager::status() const
{
    return status_;
}

QString RecoveryManager::statusText() const
{
    return userFacingStatus(status_);
}

bool RecoveryManager::recovering() const
{
    return status_.overall == RecoveryState::Recovering || status_.overall == RecoveryState::Waiting
        || status_.overall == RecoveryState::Reconciling;
}

bool RecoveryManager::suspended() const noexcept
{
    return status_.suspended;
}

quint64 RecoveryManager::generation() const noexcept
{
    return status_.generation;
}

quint64 RecoveryManager::suspendEpoch() const noexcept
{
    return status_.suspendEpoch;
}

void RecoveryManager::refreshObservedHealth()
{
    if (hooks_.isBlueZAvailable) {
        blueZAvailable_ = hooks_.isBlueZAvailable();
    }
    if (hooks_.isSystemBusConnected) {
        systemBusConnected_ = hooks_.isSystemBusConnected();
    }
    if (hooks_.isPipeWireConnected) {
        pipeWireConnected_ = hooks_.isPipeWireConnected();
    }
    if (hooks_.isPipeWireGraphReady) {
        pipeWireGraphReady_ = hooks_.isPipeWireGraphReady();
    }
}

void RecoveryManager::notifyBlueZAvailable(bool available)
{
    if (shuttingDown_) {
        return;
    }
    if (blueZAvailable_ == available) {
        if (available && status_.bluetooth == RecoveryState::Healthy) {
            return;
        }
        if (!available && status_.bluetooth == RecoveryState::Waiting) {
            return;
        }
    }

    blueZAvailable_ = available;
    if (!available) {
        status_.bluetooth = RecoveryState::Waiting;
        setOverall(RecoveryState::Recovering, RecoveryCause::ServiceVanished, QStringLiteral("Bluetooth service unavailable"));
        if (hooks_.pauseBluetoothReconnect) {
            hooks_.pauseBluetoothReconnect();
        }
        return;
    }

    status_.bluetooth = RecoveryState::Recovering;
    setOverall(RecoveryState::Recovering, RecoveryCause::ServiceRestarted);
    if (hooks_.resumeBluetoothReconnect) {
        hooks_.resumeBluetoothReconnect();
    }
    // Snapshot ownership stays with BlueZDbusClient/BluetoothManager on availability return.
    // RecoveryManager only coalesces session reconcile when auto-recover is enabled.
    if (status_.autoRecoverEnabled) {
        scheduleCoalescedReconcile();
    } else {
        status_.bluetooth = RecoveryState::Healthy;
        status_.overall = RecoveryState::Degraded;
        emitStatus();
    }
}

void RecoveryManager::notifySystemBusConnected(bool connected)
{
    if (shuttingDown_) {
        return;
    }
    if (systemBusConnected_ == connected) {
        return;
    }
    systemBusConnected_ = connected;
    if (!connected) {
        status_.bluetooth = RecoveryState::Waiting;
        setOverall(RecoveryState::Recovering, RecoveryCause::ServiceVanished, QStringLiteral("System D-Bus unavailable"));
        if (hooks_.pauseBluetoothReconnect) {
            hooks_.pauseBluetoothReconnect();
        }
        return;
    }
    if (hooks_.resumeBluetoothReconnect) {
        hooks_.resumeBluetoothReconnect();
    }
    setOverall(RecoveryState::Recovering, RecoveryCause::ServiceRestarted);
    if (status_.autoRecoverEnabled) {
        scheduleCoalescedReconcile();
    } else {
        status_.overall = RecoveryState::Degraded;
        emitStatus();
    }
}

void RecoveryManager::notifyPipeWireConnected(bool connected, bool graphReady)
{
    if (shuttingDown_) {
        return;
    }
    pipeWireConnected_ = connected;
    pipeWireGraphReady_ = graphReady;
    if (connected && graphReady) {
        status_.pipeWire = RecoveryState::Healthy;
        status_.pipeWireAttempts = 0;
        status_.lastError.clear();
        if (status_.autoRecoverEnabled) {
            scheduleCoalescedReconcile();
        } else {
            status_.overall = (blueZAvailable_ && adapterPresent_) ? RecoveryState::Healthy : RecoveryState::Degraded;
            emitStatus();
        }
    } else if (connected) {
        status_.pipeWire = RecoveryState::Waiting;
        emitStatus();
    }
}

void RecoveryManager::notifyPipeWireError(const QString& error)
{
    if (shuttingDown_ || status_.suspended) {
        return;
    }
    pipeWireConnected_ = false;
    pipeWireGraphReady_ = false;
    status_.pipeWire = status_.autoRecoverEnabled ? RecoveryState::Recovering : RecoveryState::Degraded;
    setOverall(
        status_.autoRecoverEnabled ? RecoveryState::Recovering : RecoveryState::Degraded,
        RecoveryCause::GraphReset,
        error);
}

void RecoveryManager::notifyPipeWireReconnectAttempt(int attempt)
{
    if (shuttingDown_) {
        return;
    }
    status_.pipeWireAttempts = attempt;
    status_.pipeWire = RecoveryState::Recovering;
    if (status_.autoRecoverEnabled) {
        setOverall(RecoveryState::Recovering, RecoveryCause::GraphReset);
    } else {
        emitStatus();
    }
}

void RecoveryManager::notifyPipeWireReconnectExhausted(const QString& reason)
{
    if (shuttingDown_) {
        return;
    }
    status_.pipeWire = RecoveryState::Exhausted;
    setOverall(RecoveryState::Exhausted, RecoveryCause::Timeout, reason);
    emit recoveryExhausted(QStringLiteral("PipeWire"), reason);
}

void RecoveryManager::notifyAdapterPresent(bool present)
{
    if (shuttingDown_) {
        return;
    }
    if (adapterPresent_ == present) {
        return;
    }
    adapterPresent_ = present;
    if (!present) {
        setOverall(RecoveryState::Degraded, RecoveryCause::AdapterRemoved, QStringLiteral("Bluetooth adapter unavailable"));
        return;
    }
    if (status_.autoRecoverEnabled) {
        scheduleCoalescedReconcile();
    }
}

void RecoveryManager::onPreparingForSleep(bool sleeping)
{
    if (shuttingDown_) {
        return;
    }
    if (sleeping) {
        status_.suspended = true;
        status_.overall = RecoveryState::Suspended;
        coalesceTimer_.stop();
        pendingReconcile_ = false;
        bumpGeneration();
        if (hooks_.pauseBluetoothReconnect) {
            hooks_.pauseBluetoothReconnect();
        }
        emitStatus();
        qCInfo(auralisRecovery) << "RecoveryManager: prepare-for-sleep — timers paused generation=" << status_.generation;
        return;
    }

    // Always leave suspend and restore reconnect ability; gate only proactive restore.
    status_.suspended = false;
    ++status_.suspendEpoch;
    bumpGeneration();
    if (hooks_.resumeBluetoothReconnect) {
        hooks_.resumeBluetoothReconnect();
    }

    if (!restoreOnResume_) {
        refreshObservedHealth();
        if (blueZAvailable_ && pipeWireConnected_ && pipeWireGraphReady_ && adapterPresent_) {
            status_.bluetooth = RecoveryState::Healthy;
            status_.pipeWire = RecoveryState::Healthy;
            status_.overall = RecoveryState::Healthy;
        } else {
            status_.overall = RecoveryState::Degraded;
        }
        emitStatus();
        qCInfo(auralisRecovery) << "RecoveryManager: resume without auto-restore epoch=" << status_.suspendEpoch;
        return;
    }

    setOverall(RecoveryState::Recovering, RecoveryCause::Resume);
    if (hooks_.requestBlueZRefresh) {
        hooks_.requestBlueZRefresh();
    }
    if (!pipeWireConnected_ && status_.autoRecoverEnabled && hooks_.requestPipeWireReconnect) {
        hooks_.requestPipeWireReconnect();
        status_.pipeWire = RecoveryState::Recovering;
    }
    scheduleCoalescedReconcile();
    qCInfo(auralisRecovery) << "RecoveryManager: resume epoch=" << status_.suspendEpoch
                            << "generation=" << status_.generation;
}

void RecoveryManager::flushPendingReconcileForTesting()
{
    coalesceTimer_.stop();
    if (pendingReconcile_) {
        runReconcile();
    }
}

void RecoveryManager::bumpGeneration()
{
    ++status_.generation;
}

void RecoveryManager::setOverall(RecoveryState state, RecoveryCause cause, const QString& error)
{
    status_.overall = state;
    if (cause != RecoveryCause::None) {
        status_.lastCause = cause;
    }
    if (!error.isEmpty()) {
        status_.lastError = error;
    }
    emitStatus();
}

void RecoveryManager::scheduleCoalescedReconcile()
{
    if (shuttingDown_ || status_.suspended || !status_.autoRecoverEnabled) {
        return;
    }
    pendingReconcile_ = true;
    if (!coalesceTimer_.isActive()) {
        coalesceTimer_.start();
    }
}

void RecoveryManager::runReconcile()
{
    if (shuttingDown_ || status_.suspended || !pendingReconcile_) {
        return;
    }
    pendingReconcile_ = false;
    status_.overall = RecoveryState::Reconciling;
    emitStatus();

    if (hooks_.refreshActiveSession) {
        hooks_.refreshActiveSession();
    }

    if (blueZAvailable_ && pipeWireConnected_ && pipeWireGraphReady_) {
        status_.bluetooth = RecoveryState::Healthy;
        status_.pipeWire = RecoveryState::Healthy;
        status_.overall = adapterPresent_ ? RecoveryState::Healthy : RecoveryState::Degraded;
        status_.lastError.clear();
        status_.pipeWireAttempts = 0;
    } else if (!blueZAvailable_ || !systemBusConnected_) {
        status_.bluetooth = RecoveryState::Waiting;
        status_.overall = RecoveryState::Degraded;
    } else if (!pipeWireConnected_ || !pipeWireGraphReady_) {
        status_.pipeWire = RecoveryState::Recovering;
        status_.overall = RecoveryState::Degraded;
    } else {
        status_.overall = RecoveryState::Degraded;
    }
    emitStatus();
}

void RecoveryManager::emitStatus()
{
    emit statusChanged();
}

} // namespace auralis::recovery
