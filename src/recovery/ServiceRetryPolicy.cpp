#include <auralis/recovery/ServiceRetryPolicy.h>

#include <algorithm>
#include <cmath>

namespace auralis::recovery {

ServiceRetryPolicy::ServiceRetryPolicy(ServiceRetryPolicyConfig config)
    : config_(std::move(config))
{
}

void ServiceRetryPolicy::setConfig(const ServiceRetryPolicyConfig& config)
{
    config_ = config;
}

ServiceRetryPolicyConfig ServiceRetryPolicy::config() const noexcept
{
    return config_;
}

void ServiceRetryPolicy::reset()
{
    attempt_ = 0;
}

bool ServiceRetryPolicy::canAttempt() const noexcept
{
    return attempt_ < config_.maxAttempts;
}

int ServiceRetryPolicy::attempt() const noexcept
{
    return attempt_;
}

int ServiceRetryPolicy::nextDelayMs() const noexcept
{
    if (attempt_ <= 0) {
        return config_.initialDelayMs;
    }
    const double factor = std::pow(config_.backoffMultiplier, static_cast<double>(attempt_ - 1));
    const int delay = static_cast<int>(std::lround(config_.initialDelayMs * factor));
    return std::clamp(delay, config_.initialDelayMs, config_.maxDelayMs);
}

int ServiceRetryPolicy::consumeAttemptDelayMs()
{
    ++attempt_;
    const double factor = std::pow(config_.backoffMultiplier, static_cast<double>(std::max(0, attempt_ - 1)));
    const int delay = static_cast<int>(std::lround(config_.initialDelayMs * factor));
    return std::clamp(delay, config_.initialDelayMs, config_.maxDelayMs);
}

bool ServiceRetryPolicy::exhausted() const noexcept
{
    return attempt_ >= config_.maxAttempts;
}

} // namespace auralis::recovery
