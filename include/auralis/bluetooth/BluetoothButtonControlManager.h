#pragma once

#include <auralis/bluetooth/DeviceButtonPolicy.h>

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace auralis::bluetooth {

/// Per-device media-button ALLOW/DISALLOW with Backend B (evdev) suppression when available.
class BluetoothButtonControlManager final : public QObject {
    Q_OBJECT

public:
    using InputProbe = std::function<QVector<BluetoothInputEndpoint>()>;
    using GrabFn = std::function<bool(const QString& eventNode)>;
    using ReleaseFn = std::function<void(const QString& eventNode)>;

    explicit BluetoothButtonControlManager(QObject* parent = nullptr);
    explicit BluetoothButtonControlManager(std::unique_ptr<QSettings> settings, QObject* parent = nullptr);
    ~BluetoothButtonControlManager() override;

    void setInputProbeForTesting(InputProbe probe);
    void setGrabHooksForTesting(GrabFn grab, ReleaseFn release);

    bool initialize();
    void shutdown();

    DeviceButtonPolicy policyForAddress(const QString& address) const;
    void setPolicyForAddress(const QString& address, DeviceButtonPolicy policy);
    void clearPolicyForAddress(const QString& address);

    DeviceButtonEffectiveState effectiveStateForAddress(const QString& address) const;
    QString effectiveStatusTextForAddress(const QString& address) const;
    bool canControlButtonsForAddress(const QString& address) const;
    QString matchedEventNodeForAddress(const QString& address) const;

    /// Apply or release grabs for devices that should be actively controlled.
    void syncDevice(const QString& address, bool connected);
    void reapplyAll();
    void releaseAll();

    static QString normalizeAddress(const QString& address);
    static bool addressesMatch(const QString& left, const QString& right);
    static bool isMediaKeyCode(int code);

signals:
    void policyChanged(const QString& address);
    void effectiveStateChanged(const QString& address);

private:
    struct ActiveGrab {
        QString eventNode;
        int fd = -1;
    };

    void loadPolicies();
    void persistPolicy(const QString& key, DeviceButtonPolicy policy);
    void removePersistedPolicy(const QString& key);
    std::optional<BluetoothInputEndpoint> findEndpoint(const QString& address) const;
    QVector<BluetoothInputEndpoint> probeInputs() const;
    void updateEffective(const QString& address, DeviceButtonEffectiveState state);
    void applyDisallow(const QString& address);
    void applyAllow(const QString& address);

    std::unique_ptr<QSettings> settings_;
    bool initialized_ = false;
    QHash<QString, DeviceButtonPolicy> policies_;
    QHash<QString, DeviceButtonEffectiveState> effective_;
    QHash<QString, ActiveGrab> grabs_;
    QHash<QString, bool> connected_;
    mutable QVector<BluetoothInputEndpoint> inputCache_;
    mutable bool inputCacheValid_ = false;
    mutable QElapsedTimer inputCacheAge_;
    InputProbe probeOverride_;
    GrabFn grabOverride_;
    ReleaseFn releaseOverride_;
};

} // namespace auralis::bluetooth
