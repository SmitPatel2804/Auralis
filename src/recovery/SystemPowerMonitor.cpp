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
    connect(&subscribeRetryTimer_, &QTimer::timeout, this, [this]() {
        if (!initialized_ || subscribed_) {
            stopSubscribeRetryTimer();
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
    subscribed_ = trySubscribeLogind();
    if (!subscribed_) {
        qCInfo(auralisPower) << "SystemPowerMonitor: logind PrepareForSleep unavailable; will retry";
        startSubscribeRetryTimer();
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
    if (subscribed_) {
        QDBusConnection::systemBus().disconnect(
            QStringLiteral("org.freedesktop.login1"),
            QStringLiteral("/org/freedesktop/login1"),
            QStringLiteral("org.freedesktop.login1.Manager"),
            QStringLiteral("PrepareForSleep"),
            this,
            SLOT(injectPrepareForSleep(bool)));
        subscribed_ = false;
    }
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

void SystemPowerMonitor::notifySystemBusAvailable()
{
    if (!initialized_ || subscribed_) {
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

void SystemPowerMonitor::notifySystemBusUnavailable()
{
    if (!initialized_ || !subscribed_) {
        return;
    }
    stopSubscribeRetryTimer();
    QDBusConnection::systemBus().disconnect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(injectPrepareForSleep(bool)));
    subscribed_ = false;
    qCInfo(auralisPower) << "SystemPowerMonitor: system bus unavailable; logind subscription invalidated";
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
    if (!initialized_ || subscribed_) {
        return;
    }
    if (trySubscribeLogind()) {
        subscribed_ = true;
        stopSubscribeRetryTimer();
    }
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
    if (!initialized_ || subscribed_) {
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
