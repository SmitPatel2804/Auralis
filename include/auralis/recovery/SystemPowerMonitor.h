#pragma once

#include <QObject>
#include <QTimer>

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
    bool isSubscribed() const noexcept;

    /// Retry logind subscription after system bus becomes available.
    void notifySystemBusAvailable();

    void setSubscribeRetryIntervalMsForTesting(int ms);
    void attemptSubscribeForTesting();

public slots:
    /// Test seam / logind callback: inject prepare-for-sleep without blocking.
    void injectPrepareForSleep(bool sleeping);

signals:
    void preparingForSleep(bool sleeping);
    void suspendedChanged();

private:
    bool trySubscribeLogind();
    void setSuspended(bool value);
    void startSubscribeRetryTimer();
    void stopSubscribeRetryTimer();

    QTimer subscribeRetryTimer_;
    bool initialized_ = false;
    bool suspended_ = false;
    bool subscribed_ = false;
    int subscribeRetryIntervalMs_ = 2000;
};

} // namespace auralis::recovery
