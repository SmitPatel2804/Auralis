#pragma once

#include <auralis/audio/IPipeWireManager.h>
#include <auralis/audio/PipeWireConnection.h>
#include <auralis/audio/PipeWireTypes.h>
#include <auralis/audio/PipeWireVirtualOutput.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

namespace auralis::bluetooth {
class DeviceRegistry;
}

namespace auralis::audio {

class AudioEndpointListModel;
class AudioEndpointRegistry;
class AudioRouter;
class EndpointResolver;
class PipeWireConnection;
class PipeWireObjectStore;

class PipeWireManager final : public QObject, public IPipeWireManager {
    Q_OBJECT
    Q_PROPERTY(QString connectionStateText READ connectionStateText NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int endpointCount READ endpointCount NOTIFY graphRevisionChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY graphRevisionChanged)
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphRevisionChanged)
    Q_PROPERTY(int mappedBluetoothCount READ mappedBluetoothCount NOTIFY graphRevisionChanged)
    Q_PROPERTY(bool initialSyncComplete READ initialSyncComplete NOTIFY graphRevisionChanged)
    Q_PROPERTY(int graphRevision READ graphRevision NOTIFY graphRevisionChanged)
    Q_PROPERTY(QString diagnosticsText READ diagnosticsText NOTIFY graphRevisionChanged)
    Q_PROPERTY(bool virtualOutputAvailable READ virtualOutputAvailable NOTIFY graphRevisionChanged)
    Q_PROPERTY(bool virtualOutputSelected READ virtualOutputSelected NOTIFY graphRevisionChanged)
    Q_PROPERTY(QString virtualOutputStatus READ virtualOutputStatus NOTIFY graphRevisionChanged)
    Q_PROPERTY(QAbstractItemModel* endpoints READ endpoints CONSTANT)
    Q_PROPERTY(QObject* router READ router CONSTANT)

public:
    explicit PipeWireManager(QObject* parent = nullptr);
    explicit PipeWireManager(bluetooth::DeviceRegistry* bluetoothRegistry, QObject* parent = nullptr);
    ~PipeWireManager() override;

    bool initialize() override;
    void shutdown() override;
    auralis::core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;

    QString backendName() const override;

    PipeWireConnectionState connectionState() const noexcept;
    QString connectionStateText() const;
    bool connected() const noexcept override;
    bool graphReady() const noexcept override;
    QString lastError() const override;
    int endpointCount() const;
    int deviceCount() const;
    int nodeCount() const;
    int mappedBluetoothCount() const;
    bool initialSyncComplete() const noexcept;
    int graphRevision() const noexcept;
    QString diagnosticsText() const;
    bool virtualOutputAvailable() const noexcept;
    bool virtualOutputSelected() const noexcept;
    QString virtualOutputStatus() const;
    QAbstractItemModel* endpoints() const;
    QObject* router() const;
    AudioRouter* audioRouter() const noexcept override;
    AudioEndpointRegistry* endpointRegistry() const noexcept override;
    const PipeWireObjectStore* objectStore() const noexcept;

    Q_INVOKABLE QString audioStatusForDevice(const QString& bluetoothDeviceId) const;
    Q_INVOKABLE void refreshVirtualAudio();

    /// Bounded reconnect after daemon loss. RecoveryManager observes; does not schedule a second retry.
    void setAutoReconnectEnabled(bool enabled) override;
    bool autoReconnectEnabled() const noexcept;
    void requestReconnect() override;
    int reconnectAttempt() const noexcept;
    int maxReconnectAttempts() const noexcept;
    void setMaxReconnectAttemptsForTesting(int maxAttempts);
    void setReconnectInitialDelayMsForTesting(int delayMs);
    void setInitialSyncTimeoutMsForTesting(int timeoutMs);
    /// Inject PipeWire client events without a live daemon (unit tests).
    void injectClientEventForTesting(const PipeWireClientEvent& event);
    /// Count a failed reconnect attempt without waiting on the retry timer.
    void simulateReconnectFailureForTesting(const QString& reason = QStringLiteral("test"));
    void fireInitialSyncTimeoutForTesting();
    bool initialSyncTimeoutPendingForTesting() const noexcept;

signals:
    void statusChanged();
    void connectionStateChanged();
    void lastErrorChanged();
    void graphRevisionChanged();
    void reconnectAttemptStarted(int attempt);
    void reconnectExhausted(const QString& reason);

private:
    struct Guard {
        std::atomic<bool> alive{true};
        std::atomic<quint64> generation{0};
    };

    void handleClientEvent(const PipeWireClientEvent& event, quint64 generation);
    void setConnectionState(PipeWireConnectionState state, const QString& error = {});
    void refreshGraph();
    void scheduleGraphRefresh();
    void bumpGraph();
    void wireBluetoothRegistry();
    void scheduleAutoReconnect(const QString& reason);
    void performReconnect();
    bool startConnection(quint64 generation);
    void startInitialSyncTimeout();
    void stopInitialSyncTimeout();
    void onInitialSyncTimeout();
    void completeInitialSync();
    void updateVirtualOutputState();
    void ensureVirtualOutput();
    void resetVirtualOutputState();

    bluetooth::DeviceRegistry* bluetoothRegistry_ = nullptr;
    std::unique_ptr<PipeWireObjectStore> store_;
    std::unique_ptr<AudioEndpointRegistry> endpoints_;
    std::unique_ptr<EndpointResolver> resolver_;
    std::unique_ptr<PipeWireConnection> connection_;
    AudioEndpointListModel* model_ = nullptr;
    AudioRouter* router_ = nullptr;
    std::shared_ptr<Guard> guard_;
    PipeWireConnectionState connectionState_ = PipeWireConnectionState::Stopped;
    auralis::core::ServiceStatus status_ = auralis::core::ServiceStatus::Uninitialized;
    QString lastError_;
    bool initialSyncComplete_ = false;
    bool initialSyncEventReceived_ = false;
    int graphRevision_ = 0;
    bool bluetoothWired_ = false;
    QTimer graphRefreshTimer_;
    QTimer reconnectTimer_;
    QTimer initialSyncTimeoutTimer_;
    QTimer virtualOutputTimer_;
    bool autoReconnectEnabled_ = true;
    bool shuttingDown_ = false;
    bool reconnectInProgress_ = false;
    bool reconnectExhaustedEmitted_ = false;
    int reconnectAttempt_ = 0;
    int maxReconnectAttempts_ = 8;
    int reconnectInitialDelayMs_ = 500;
    int reconnectMaxDelayMs_ = 30000;
    int initialSyncTimeoutMs_ = 8000;
    PipeWireVirtualOutputState virtualOutput_;
    QString defaultAudioSinkName_;
    QString virtualOutputError_;
    int virtualOutputProvisionAttempt_ = 0;
};

} // namespace auralis::audio
