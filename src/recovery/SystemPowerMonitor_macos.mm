#include <auralis/recovery/SystemPowerMonitor.h>

#include <QHash>
#include <QLoggingCategory>

#import <AppKit/AppKit.h>

Q_LOGGING_CATEGORY(auralisPower, "auralis.recovery.power")

@interface AuralisPowerObserver : NSObject
@property(nonatomic, assign) auralis::recovery::SystemPowerMonitor* owner;
- (void)willSleep:(NSNotification*)notification;
- (void)didWake:(NSNotification*)notification;
@end

@implementation AuralisPowerObserver
- (void)willSleep:(NSNotification*)notification
{
    Q_UNUSED(notification)
    if (self.owner != nullptr) {
        self.owner->injectPrepareForSleep(true);
    }
}
- (void)didWake:(NSNotification*)notification
{
    Q_UNUSED(notification)
    if (self.owner != nullptr) {
        self.owner->injectPrepareForSleep(false);
    }
}
@end

namespace auralis::recovery {
namespace {

QHash<SystemPowerMonitor*, AuralisPowerObserver*>& observers()
{
    static QHash<SystemPowerMonitor*, AuralisPowerObserver*> value;
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
    AuralisPowerObserver* observer = [[AuralisPowerObserver alloc] init];
    observer.owner = this;
    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center addObserver:observer selector:@selector(willSleep:) name:NSWorkspaceWillSleepNotification object:nil];
    [center addObserver:observer selector:@selector(didWake:) name:NSWorkspaceDidWakeNotification object:nil];
    observers().insert(this, observer);
    subscribed_ = true;
    qCInfo(auralisPower) << "SystemPowerMonitor: macOS sleep/wake notifications active";
    return true;
}

void SystemPowerMonitor::shutdown()
{
    if (!initialized_) {
        return;
    }
    if (AuralisPowerObserver* observer = observers().take(this)) {
        [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:observer];
        observer.owner = nullptr;
        [observer release];
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
void SystemPowerMonitor::injectTransportAvailability(bool) {}

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
