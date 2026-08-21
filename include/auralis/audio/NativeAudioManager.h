#pragma once

#include <auralis/audio/IAudioManager.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>

#include <memory>

namespace auralis::bluetooth {
class DeviceRegistry;
}

namespace auralis::audio {

class AudioEndpointListModel;
class AudioEndpointRegistry;
class AudioRouter;

/// Windows/macOS audio service backed by Qt's native multimedia plugins.
/// It keeps the public PipeWireManager QML contract intact and implements
/// capture-source fan-out to multiple native output devices.
class NativeAudioManager final : public QObject, public IAudioManager {
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
    Q_PROPERTY(QAbstractItemModel* endpoints READ endpoints CONSTANT)
    Q_PROPERTY(QObject* router READ router CONSTANT)

public:
    explicit NativeAudioManager(
        bluetooth::DeviceRegistry* bluetoothRegistry = nullptr,
        QObject* parent = nullptr);
    ~NativeAudioManager() override;

    bool initialize() override;
    void shutdown() override;
    core::ServiceStatus status() const noexcept override;
    QObject* uiObject() override;
    QString backendName() const override;
    QString lastError() const override;
    bool connected() const noexcept override;
    bool graphReady() const noexcept override;
    void setAutoReconnectEnabled(bool enabled) override;
    void requestReconnect() override;
    AudioRouter* audioRouter() const noexcept override;
    AudioEndpointRegistry* endpointRegistry() const noexcept override;

    QString connectionStateText() const;
    int endpointCount() const;
    int deviceCount() const;
    int nodeCount() const;
    int mappedBluetoothCount() const;
    bool initialSyncComplete() const noexcept;
    int graphRevision() const noexcept;
    QString diagnosticsText() const;
    QAbstractItemModel* endpoints() const;
    QObject* router() const;
    Q_INVOKABLE QString audioStatusForDevice(const QString& bluetoothDeviceId) const;

signals:
    void statusChanged();
    void connectionStateChanged();
    void lastErrorChanged();
    void graphRevisionChanged();
    void reconnectAttemptStarted(int attempt);
    void reconnectExhausted(const QString& reason);

private:
    struct State;
    void scheduleGraphRefresh();
    void refreshGraph();
    void setLastError(const QString& error);

    std::unique_ptr<State> stateImpl_;
    bluetooth::DeviceRegistry* bluetoothRegistry_ = nullptr;
    AudioEndpointRegistry* endpoints_ = nullptr;
    AudioEndpointListModel* model_ = nullptr;
    AudioRouter* router_ = nullptr;
    core::ServiceStatus status_ = core::ServiceStatus::Uninitialized;
    QString lastError_;
    int graphRevision_ = 0;
    bool autoReconnectEnabled_ = true;
};

} // namespace auralis::audio
