#pragma once

#include <QObject>

namespace auralis::recovery {

/// Observes systemd-logind PrepareForSleep. Does not block suspend.
class SystemPowerMonitor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool suspended READ suspended NOTIFY suspendedChanged)

public:
    explicit SystemPowerMonitor(QObject* parent = nullptr);
    ~SystemPowerMonitor() override;

    bool initialize();
    void shutdown();

    bool suspended() const noexcept;

public slots:
    /// Test seam / logind callback: inject prepare-for-sleep without blocking.
    void injectPrepareForSleep(bool sleeping);

signals:
    void preparingForSleep(bool sleeping);
    void suspendedChanged();

private:
    bool trySubscribeLogind();
    void setSuspended(bool value);

    bool initialized_ = false;
    bool suspended_ = false;
    bool subscribed_ = false;
};

} // namespace auralis::recovery
