#pragma once

#include <QtGlobal>

namespace auralis::recovery {

struct ServiceRetryPolicyConfig {
    int maxAttempts = 8;
    int initialDelayMs = 500;
    int maxDelayMs = 30000;
    double backoffMultiplier = 2.0;
};

class ServiceRetryPolicy final {
public:
    explicit ServiceRetryPolicy(ServiceRetryPolicyConfig config = {});

    void setConfig(const ServiceRetryPolicyConfig& config);
    ServiceRetryPolicyConfig config() const noexcept;

    void reset();
    bool canAttempt() const noexcept;
    int attempt() const noexcept;
    int nextDelayMs() const noexcept;
    /// Advance attempt counter and return delay for this attempt (1-based after call).
    int consumeAttemptDelayMs();
    bool exhausted() const noexcept;

private:
    ServiceRetryPolicyConfig config_;
    int attempt_ = 0;
};

} // namespace auralis::recovery
