#include <auralis/recovery/SystemPowerMonitor.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(auralisPower, "auralis.recovery.power")

namespace auralis::recovery {

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
    subscribed_ = trySubscribeLogind();
    if (!subscribed_) {
        qCInfo(auralisPower) << "SystemPowerMonitor: logind PrepareForSleep unavailable; inject-only mode";
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

void SystemPowerMonitor::injectPrepareForSleep(bool sleeping)
{
    setSuspended(sleeping);
    emit preparingForSleep(sleeping);
}

bool SystemPowerMonitor::trySubscribeLogind()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return false;
    }

    // Ensure logind is reachable before connecting the signal.
    QDBusInterface iface(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        bus);
    if (!iface.isValid()) {
        return false;
    }

    const bool ok = bus.connect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(injectPrepareForSleep(bool)));
    return ok;
}

void SystemPowerMonitor::setSuspended(bool value)
{
    if (suspended_ == value) {
        return;
    }
    suspended_ = value;
    emit suspendedChanged();
}

} // namespace auralis::recovery
