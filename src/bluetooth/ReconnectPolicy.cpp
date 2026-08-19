#include <auralis/bluetooth/ReconnectPolicy.h>

#include <auralis/core/LoggingCategories.h>

#include <QString>
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
    if ((entry.timer != nullptr && entry.timer->isActive()) || entry.inFlight) {
        qCDebug(auralisBluetooth) << "ManagedReconnectAlreadyScheduled" << devicePath
                                  << "attempt=" << entry.attempt;
        return;
    }
    if (entry.attempt >= config_.maxAttempts) {
        const bool alreadyEmitted = entry.exhaustedEmitted;
        const int attempts = entry.attempt;
        destroyEntryTimer(entry);
        entries_.remove(devicePath);
        if (!alreadyEmitted) {
            qCInfo(auralisBluetooth) << "ManagedReconnectExhausted" << devicePath
                                     << "attempts=" << attempts;
            emit reconnectExhausted(
                devicePath,
                attempts,
                QStringLiteral("Reconnect attempt budget exhausted"));
        }
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
    qCInfo(auralisBluetooth) << "ManagedReconnectRequested" << devicePath << "attempt=" << entry.attempt;
}

void ReconnectPolicy::cancelReconnect(const QString& devicePath)
{
    auto it = entries_.find(devicePath);
    if (it == entries_.end()) {
        return;
    }
    destroyEntryTimer(*it);
    entries_.erase(it);
}

void ReconnectPolicy::cancelAll()
{
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        destroyEntryTimer(it.value());
    }
    entries_.clear();
}

void ReconnectPolicy::pauseAll()
{
    paused_ = true;
    cancelAll();
}

void ReconnectPolicy::resumeAll()
{
    paused_ = false;
}

void ReconnectPolicy::onConnected(const QString& devicePath)
{
    cancelReconnect(devicePath);
}

void ReconnectPolicy::completeReconnectAttempt(const QString& devicePath)
{
    auto it = entries_.find(devicePath);
    if (it == entries_.end()) {
        return;
    }
    it->inFlight = false;
}

bool ReconnectPolicy::isScheduled(const QString& devicePath) const
{
    const auto it = entries_.constFind(devicePath);
    return it != entries_.cend() && it->timer != nullptr && it->timer->isActive();
}

bool ReconnectPolicy::isReconnectInProgress(const QString& devicePath) const
{
    const auto it = entries_.constFind(devicePath);
    return it != entries_.cend() && it->inFlight;
}

int ReconnectPolicy::attempt(const QString& devicePath) const
{
    return entries_.value(devicePath).attempt;
}

void ReconnectPolicy::retryAfterContention(const QString& devicePath)
{
    auto it = entries_.find(devicePath);
    if (it == entries_.end()) {
        return;
    }
    it->inFlight = false;
    --it->attempt;
    if (it->attempt < 0) {
        it->attempt = 0;
    }
    qCInfo(auralisBluetooth) << "ReconnectRetryAfterContention" << devicePath << "attempt=" << it->attempt;
    scheduleReconnect(devicePath);
}

void ReconnectPolicy::reportTerminalFailure(const QString& devicePath, const QString& reason)
{
    auto it = entries_.find(devicePath);
    const int attempts = (it != entries_.end()) ? it->attempt : 0;
    if (it != entries_.end()) {
        destroyEntryTimer(*it);
        entries_.erase(it);
    }
    qCInfo(auralisBluetooth) << "ManagedReconnectTerminalFailure" << devicePath << "attempts=" << attempts << reason;
    emit reconnectTerminalFailure(devicePath, attempts, reason);
}

void ReconnectPolicy::fire(const QString& devicePath)
{
    auto it = entries_.find(devicePath);
    if (it == entries_.end()) {
        return;
    }
    it->inFlight = true;
    emit reconnectDue(devicePath, it->attempt, config_.maxAttempts);
}

void ReconnectPolicy::destroyEntryTimer(Entry& entry)
{
    if (entry.timer == nullptr) {
        return;
    }
    entry.timer->stop();
    entry.timer->deleteLater();
    entry.timer = nullptr;
}

int ReconnectPolicy::liveRetryTimerCountForTesting() const
{
    return static_cast<int>(findChildren<QTimer*>().size());
}

} // namespace auralis::bluetooth
