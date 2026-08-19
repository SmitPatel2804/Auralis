#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>

namespace auralis::bluetooth {

struct ReconnectPolicyConfig {
    bool enabled = true;
    int maxAttempts = 5;
    int initialDelayMs = 1000;
    int maxDelayMs = 30000;
    double backoffMultiplier = 2.0;
};

class ReconnectPolicy final : public QObject {
    Q_OBJECT

public:
    explicit ReconnectPolicy(QObject* parent = nullptr);

    void setConfig(const ReconnectPolicyConfig& config);
    ReconnectPolicyConfig config() const;

    void scheduleReconnect(const QString& devicePath);
    void cancelReconnect(const QString& devicePath);
    void cancelAll();
    void pauseAll();
    void resumeAll();
    void onConnected(const QString& devicePath);
    /// Call after a due reconnect attempt finishes (success or failure) so the next
    /// scheduleReconnect can start a new attempt.
    void completeReconnectAttempt(const QString& devicePath);
    bool isScheduled(const QString& devicePath) const;
    bool isReconnectInProgress(const QString& devicePath) const;
    int attempt(const QString& devicePath) const;

    void reportTerminalFailure(const QString& devicePath, const QString& reason);
    void retryAfterContention(const QString& devicePath);

    /// Test-only: count live QTimer children (for leak detection).
    int liveRetryTimerCountForTesting() const;

signals:
    void reconnectDue(const QString& devicePath, int attempt, int maxAttempts);
    void reconnectExhausted(const QString& devicePath, int attempts, const QString& reason);
    void reconnectTerminalFailure(const QString& devicePath, int attempts, const QString& reason);

private:
    struct Entry {
        int attempt = 0;
        QTimer* timer = nullptr;
        bool userDisconnected = false;
        bool inFlight = false;
        bool exhaustedEmitted = false;
    };

    void fire(const QString& devicePath);
    void destroyEntryTimer(Entry& entry);

    ReconnectPolicyConfig config_;
    QHash<QString, Entry> entries_;
    bool paused_ = false;
};

} // namespace auralis::bluetooth
