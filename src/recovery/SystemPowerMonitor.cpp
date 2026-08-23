#include <auralis/recovery/SystemPowerMonitor.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(auralisPower, "auralis.recovery.power")

namespace auralis::recovery {

SystemPowerMonitor::SystemPowerMonitor(QObject* parent)
    : QObject(parent)
{
    subscribeRetryTimer_.setSingleShot(false);
    subscribeRetryTimer_.setTimerType(Qt::VeryCoarseTimer);
    connect(&subscribeRetryTimer_, &QTimer::timeout, this, [this]() {
        if (!initialized_ || subscribed_ || !busAvailable_) {
            if (!busAvailable_) {
                stopSubscribeRetryTimer();
            }
            return;
        }
        if (trySubscribeLogind()) {
            subscribed_ = true;
            stopSubscribeRetryTimer();
            qCInfo(auralisPower) << "SystemPowerMonitor: subscribed to logind PrepareForSleep (retry)";
        }
    });
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
    busAvailable_ = QDBusConnection::systemBus().isConnected();
    subscribed_ = busAvailable_ && trySubscribeLogind();
    if (!subscribed_) {
        qCInfo(auralisPower) << "SystemPowerMonitor: logind PrepareForSleep unavailable; will retry";
        if (busAvailable_) {
            startSubscribeRetryTimer();
        }
    } else {
        qCInfo(auralisPower) << "SystemPowerMonitor: subscribed to logind PrepareForSleep";
    }
    return true;
}

void SystemPowerMonitor::shutdown()
{
    if (!initialized_) {
        return;
    }
    stopSubscribeRetryTimer();
    invalidateLogindSubscription();
    initialized_ = false;
}

bool SystemPowerMonitor::suspended() const noexcept
{
    return suspended_;
}

bool SystemPowerMonitor::isSubscribed() const noexcept
{
    return subscribed_;
}

void SystemPowerMonitor::notifySystemBusUnavailable()
{
    if (!initialized_) {
        return;
    }
    busAvailable_ = false;
    stopSubscribeRetryTimer();
    if (!subscribed_) {
        return;
    }
    invalidateLogindSubscription();
    qCInfo(auralisPower) << "SystemPowerMonitor: system bus lost — logind subscription invalidated";
}

void SystemPowerMonitor::notifySystemBusAvailable()
{
    if (!initialized_) {
        return;
    }
    busAvailable_ = true;
    if (subscribed_) {
        return;
    }
    if (trySubscribeLogind()) {
        subscribed_ = true;
        stopSubscribeRetryTimer();
        qCInfo(auralisPower) << "SystemPowerMonitor: subscribed to logind PrepareForSleep (bus return)";
    } else {
        startSubscribeRetryTimer();
    }
}

void SystemPowerMonitor::setSubscribeRetryIntervalMsForTesting(int ms)
{
    subscribeRetryIntervalMs_ = std::max(1, ms);
    if (subscribeRetryTimer_.isActive()) {
        subscribeRetryTimer_.setInterval(subscribeRetryIntervalMs_);
    }
}

void SystemPowerMonitor::attemptSubscribeForTesting()
{
    if (!initialized_ || subscribed_ || !busAvailable_) {
        return;
    }
    if (trySubscribeLogind()) {
        subscribed_ = true;
        stopSubscribeRetryTimer();
    }
}

void SystemPowerMonitor::injectSubscribedForTesting(bool subscribed)
{
    if (!initialized_) {
        return;
    }
    if (subscribed) {
        busAvailable_ = true;
        subscribed_ = true;
        stopSubscribeRetryTimer();
        return;
    }
    subscribed_ = false;
}

void SystemPowerMonitor::injectPrepareForSleep(bool sleeping)
{
    if (suspended_ == sleeping) {
        return;
    }
    setSuspended(sleeping);
    emit preparingForSleep(sleeping);
}

void SystemPowerMonitor::injectTransportAvailability(bool available)
{
    if (available) {
        notifySystemBusAvailable();
    } else {
        notifySystemBusUnavailable();
    }
}

bool SystemPowerMonitor::trySubscribeLogind()
{
    if (subscribed_) {
        return true;
    }
    if (!busAvailable_) {
        return false;
    }

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return false;
    }

    QDBusInterface iface(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        bus);
    if (!iface.isValid()) {
        return false;
    }

    return bus.connect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(injectPrepareForSleep(bool)));
}

void SystemPowerMonitor::invalidateLogindSubscription()
{
    if (!subscribed_) {
        return;
    }
    QDBusConnection::systemBus().disconnect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(injectPrepareForSleep(bool)));
    subscribed_ = false;
}

void SystemPowerMonitor::setSuspended(bool value)
{
    if (suspended_ == value) {
        return;
    }
    suspended_ = value;
    emit suspendedChanged();
}

void SystemPowerMonitor::startSubscribeRetryTimer()
{
    if (!initialized_ || subscribed_ || !busAvailable_) {
        return;
    }
    subscribeRetryTimer_.setInterval(subscribeRetryIntervalMs_);
    if (!subscribeRetryTimer_.isActive()) {
        subscribeRetryTimer_.start();
    }
}

void SystemPowerMonitor::stopSubscribeRetryTimer()
{
    subscribeRetryTimer_.stop();
}

} // namespace auralis::recovery
