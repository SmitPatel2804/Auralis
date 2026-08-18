#include <auralis/bluetooth/ReconnectPolicy.h>

#include <QtMath>

namespace auralis::bluetooth {

ReconnectPolicy::ReconnectPolicy(QObject* parent)
    : QObject(parent)
{
}

void ReconnectPolicy::setConfig(const ReconnectPolicyConfig& config)
{
    config_ = config;
}

ReconnectPolicyConfig ReconnectPolicy::config() const
{
    return config_;
}

void ReconnectPolicy::scheduleReconnect(const QString& devicePath)
{
    if (!config_.enabled || paused_ || devicePath.isEmpty()) {
        return;
    }
    Entry& entry = entries_[devicePath];
    if (entry.userDisconnected) {
        return;
    }
    if (entry.attempt >= config_.maxAttempts) {
        return;
    }
    ++entry.attempt;
    if (entry.timer == nullptr) {
        entry.timer = new QTimer(this);
        entry.timer->setSingleShot(true);
        connect(entry.timer, &QTimer::timeout, this, [this, devicePath]() { fire(devicePath); });
    }
    const int delay = qMin(
        static_cast<int>(config_.initialDelayMs * qPow(config_.backoffMultiplier, entry.attempt - 1)),
        config_.maxDelayMs);
    entry.timer->start(delay);
}

void ReconnectPolicy::cancelReconnect(const QString& devicePath)
{
    auto it = entries_.find(devicePath);
    if (it == entries_.end()) {
        return;
    }
    if (it->timer != nullptr) {
        it->timer->stop();
    }
    entries_.erase(it);
}

void ReconnectPolicy::cancelAll()
{
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->timer != nullptr) {
            it->timer->stop();
        }
    }
    entries_.clear();
}

void ReconnectPolicy::pauseAll()
{
    paused_ = true;
    cancelAll();
}

void ReconnectPolicy::onConnected(const QString& devicePath)
{
    cancelReconnect(devicePath);
}

bool ReconnectPolicy::isScheduled(const QString& devicePath) const
{
    const auto it = entries_.constFind(devicePath);
    return it != entries_.cend() && it->timer != nullptr && it->timer->isActive();
}

int ReconnectPolicy::attempt(const QString& devicePath) const
{
    return entries_.value(devicePath).attempt;
}

void ReconnectPolicy::fire(const QString& devicePath)
{
    const Entry entry = entries_.value(devicePath);
    emit reconnectDue(devicePath, entry.attempt, config_.maxAttempts);
}

} // namespace auralis::bluetooth
