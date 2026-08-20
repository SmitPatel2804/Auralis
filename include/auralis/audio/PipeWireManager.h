#pragma once

#include <auralis/audio/IPipeWireManager.h>
#include <auralis/audio/PipeWireConnection.h>
#include <auralis/audio/PipeWireTypes.h>

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

    PipeWireConnectionState connectionState() const noexcept;
    QString connectionStateText() const;
    bool connected() const noexcept;
    QString lastError() const;
    int endpointCount() const;
    int deviceCount() const;
    int nodeCount() const;
    int mappedBluetoothCount() const;
    bool initialSyncComplete() const noexcept;
    int graphRevision() const noexcept;
    QString diagnosticsText() const;
    QAbstractItemModel* endpoints() const;
    QObject* router() const;
    AudioRouter* audioRouter() const noexcept;
    AudioEndpointRegistry* endpointRegistry() const noexcept;
    const PipeWireObjectStore* objectStore() const noexcept;

    Q_INVOKABLE QString audioStatusForDevice(const QString& bluetoothDeviceId) const;

signals:
    void statusChanged();
    void connectionStateChanged();
    void lastErrorChanged();
    void graphRevisionChanged();

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
    int graphRevision_ = 0;
    bool bluetoothWired_ = false;
    QTimer graphRefreshTimer_;
};

} // namespace auralis::audio
