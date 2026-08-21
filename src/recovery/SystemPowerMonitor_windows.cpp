#include <auralis/recovery/SystemPowerMonitor.h>

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QHash>
#include <QLoggingCategory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

Q_LOGGING_CATEGORY(auralisPower, "auralis.recovery.power")

namespace auralis::recovery {
namespace {

class WindowsPowerEventFilter final : public QAbstractNativeEventFilter {
public:
    explicit WindowsPowerEventFilter(SystemPowerMonitor* owner)
        : owner_(owner)
    {
    }

    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
    {
        const auto* native = static_cast<MSG*>(message);
        if (native == nullptr || native->message != WM_POWERBROADCAST || owner_ == nullptr) {
            return false;
        }
        switch (native->wParam) {
        case PBT_APMSUSPEND:
            owner_->injectPrepareForSleep(true);
            break;
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
        case PBT_APMRESUMECRITICAL:
            owner_->injectPrepareForSleep(false);
            break;
        default:
            break;
        }
        return false;
    }

private:
    SystemPowerMonitor* owner_ = nullptr;
};

QHash<SystemPowerMonitor*, WindowsPowerEventFilter*>& filters()
{
    static QHash<SystemPowerMonitor*, WindowsPowerEventFilter*> value;
    return value;
}

} // namespace

SystemPowerMonitor::SystemPowerMonitor(QObject* parent)
    : QObject(parent)
{
}

SystemPowerMonitor::~SystemPowerMonitor()
{
    shutdown();
}

bool SystemPowerMonitor::initialize()
{
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    auto* filter = new WindowsPowerEventFilter(this);
    filters().insert(this, filter);
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->installNativeEventFilter(filter);
        subscribed_ = true;
    }
    qCInfo(auralisPower) << "SystemPowerMonitor: Windows power notifications active=" << subscribed_;
    return true;
}

void SystemPowerMonitor::shutdown()
{
    if (!initialized_) {
        return;
    }
    if (auto* filter = filters().take(this)) {
        if (QCoreApplication::instance() != nullptr) {
            QCoreApplication::instance()->removeNativeEventFilter(filter);
        }
        delete filter;
    }
    subscribed_ = false;
    initialized_ = false;
}

bool SystemPowerMonitor::suspended() const noexcept { return suspended_; }
bool SystemPowerMonitor::isSubscribed() const noexcept { return subscribed_; }
void SystemPowerMonitor::notifySystemBusUnavailable() {}
void SystemPowerMonitor::notifySystemBusAvailable() {}
void SystemPowerMonitor::setSubscribeRetryIntervalMsForTesting(int ms) { subscribeRetryIntervalMs_ = qMax(1, ms); }
void SystemPowerMonitor::attemptSubscribeForTesting() {}
void SystemPowerMonitor::injectSubscribedForTesting(bool subscribed)
{
    if (!initialized_) {
        return;
    }
    subscribed_ = subscribed;
    if (subscribed) {
        busAvailable_ = true;
    }
}
void SystemPowerMonitor::injectTransportAvailability(bool) {}
void SystemPowerMonitor::invalidateLogindSubscription() {}

void SystemPowerMonitor::injectPrepareForSleep(bool sleeping)
{
    if (suspended_ == sleeping) {
        return;
    }
    setSuspended(sleeping);
    emit preparingForSleep(sleeping);
}

bool SystemPowerMonitor::trySubscribeLogind() { return subscribed_; }
void SystemPowerMonitor::setSuspended(bool value)
{
    if (suspended_ == value) {
        return;
    }
    suspended_ = value;
    emit suspendedChanged();
}
void SystemPowerMonitor::startSubscribeRetryTimer() {}
void SystemPowerMonitor::stopSubscribeRetryTimer() {}

} // namespace auralis::recovery
