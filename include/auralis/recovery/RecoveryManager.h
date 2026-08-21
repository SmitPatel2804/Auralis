#pragma once

#include <auralis/recovery/RecoveryTypes.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace auralis::recovery {

/// Coordinates service-level recovery without owning BlueZ Device1 or PipeWire links.
/// PipeWireManager owns bounded PipeWire reconnect timing; this class observes and reconciles.
class RecoveryManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool recovering READ recovering NOTIFY statusChanged)
    Q_PROPERTY(bool suspended READ suspended NOTIFY statusChanged)
    Q_PROPERTY(bool autoRecoverEnabled READ autoRecoverEnabled WRITE setAutoRecoverEnabled NOTIFY autoRecoverEnabledChanged)

public:
    struct HostHooks {
        std::function<void()> pauseBluetoothReconnect;
        std::function<void()> resumeBluetoothReconnect;
        std::function<void()> requestBlueZRefresh;
        std::function<void()> requestPipeWireReconnect;
        std::function<void()> refreshActiveSession;
        std::function<bool()> isBlueZAvailable;
        std::function<bool()> isPipeWireConnected;
        std::function<bool()> isPipeWireGraphReady;
        std::function<bool()> isSystemBusConnected;
        std::function<bool()> isAdapterPresent;
        std::function<QString()> audioLastError;
    };

    explicit RecoveryManager(QObject* parent = nullptr);
    ~RecoveryManager() override;

    void setHooks(HostHooks hooks);

    bool initialize();
    void shutdown();

    bool autoRecoverEnabled() const noexcept;
    void setAutoRecoverEnabled(bool enabled);
    bool restoreOnResume() const noexcept;
    void setRestoreOnResume(bool enabled);

    RecoveryStatus status() const;
    QString statusText() const;
    bool recovering() const;
    bool suspended() const noexcept;
    quint64 generation() const noexcept;
    quint64 suspendEpoch() const noexcept;

public slots:
    void notifyBlueZAvailable(bool available);
    void notifySystemBusConnected(bool connected);
    void notifyPipeWireConnected(bool connected, bool graphReady);
    void notifyPipeWireError(const QString& error);
    void notifyPipeWireReconnectAttempt(int attempt);
    void notifyPipeWireReconnectExhausted(const QString& reason);
    void notifyAdapterPresent(bool present);
    void onPreparingForSleep(bool sleeping);
    void refreshObservedHealth();
    void synchronizeBluetoothHealth();
    void synchronizeAudioHealth();

public:
    /// Test seam: advance coalesced reconcile without waiting for the timer.
    void flushPendingReconcileForTesting();

signals:
    void statusChanged();
    void autoRecoverEnabledChanged();
    void recoveryExhausted(const QString& domain, const QString& reason);

private:
    void bumpGeneration();
    void setOverall(RecoveryState state, RecoveryCause cause = RecoveryCause::None, const QString& error = {});
    void scheduleCoalescedReconcile();
    void runReconcile();
    void emitStatus();

    HostHooks hooks_;
    RecoveryStatus status_;
    QTimer coalesceTimer_;
    bool initialized_ = false;
    bool shuttingDown_ = false;
    bool pendingReconcile_ = false;
    bool blueZAvailable_ = true;
    bool systemBusConnected_ = true;
    bool pipeWireConnected_ = false;
    bool pipeWireGraphReady_ = false;
    bool adapterPresent_ = true;
    bool restoreOnResume_ = true;
};

} // namespace auralis::recovery
