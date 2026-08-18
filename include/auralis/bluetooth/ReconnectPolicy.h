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
    bool isScheduled(const QString& devicePath) const;
    int attempt(const QString& devicePath) const;

signals:
    void reconnectDue(const QString& devicePath, int attempt, int maxAttempts);

private:
    struct Entry {
        int attempt = 0;
        QTimer* timer = nullptr;
        bool userDisconnected = false;
    };

    void fire(const QString& devicePath);

    ReconnectPolicyConfig config_;
    QHash<QString, Entry> entries_;
    bool paused_ = false;
};

} // namespace auralis::bluetooth
