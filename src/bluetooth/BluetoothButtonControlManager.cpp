#include <auralis/bluetooth/BluetoothButtonControlManager.h>

#include <auralis/core/LoggingCategories.h>

#include <QFile>
#include <QRegularExpression>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/input.h>
#include <sys/ioctl.h>
#endif

namespace auralis::bluetooth {
namespace {

constexpr auto kSettingsGroup = "deviceButtonPolicy";

QString settingsKeyForAddress(const QString& normalized)
{
    return QString::fromLatin1(kSettingsGroup) + QLatin1Char('/') + normalized;
}

} // namespace

BluetoothButtonControlManager::BluetoothButtonControlManager(QObject* parent)
    : BluetoothButtonControlManager(
          std::make_unique<QSettings>(QStringLiteral("Auralis"), QStringLiteral("Auralis")),
          parent)
{
}

BluetoothButtonControlManager::BluetoothButtonControlManager(
    std::unique_ptr<QSettings> settings,
    QObject* parent)
    : QObject(parent)
    , settings_(std::move(settings))
{
}

BluetoothButtonControlManager::~BluetoothButtonControlManager()
{
    shutdown();
}

void BluetoothButtonControlManager::setInputProbeForTesting(InputProbe probe)
{
    probeOverride_ = std::move(probe);
}

void BluetoothButtonControlManager::setGrabHooksForTesting(GrabFn grab, ReleaseFn release)
{
    grabOverride_ = std::move(grab);
    releaseOverride_ = std::move(release);
}

bool BluetoothButtonControlManager::initialize()
{
    if (initialized_) {
        return true;
    }
    loadPolicies();
    initialized_ = true;
    return true;
}

void BluetoothButtonControlManager::shutdown()
{
    if (!initialized_) {
        releaseAll();
        return;
    }
    releaseAll();
    initialized_ = false;
}

QString BluetoothButtonControlManager::normalizeAddress(const QString& address)
{
    QString out = address.trimmed().toUpper();
    out.replace(QLatin1Char('-'), QLatin1Char(':'));
    return out;
}

bool BluetoothButtonControlManager::addressesMatch(const QString& left, const QString& right)
{
    return normalizeAddress(left) == normalizeAddress(right);
}

bool BluetoothButtonControlManager::isMediaKeyCode(int code)
{
#if defined(__linux__)
    switch (code) {
    case KEY_PLAYPAUSE:
    case KEY_PLAYCD:
    case KEY_PAUSECD:
    case KEY_PLAY:
    case KEY_PAUSE:
    case KEY_STOPCD:
    case KEY_STOP:
    case KEY_NEXTSONG:
    case KEY_PREVIOUSSONG:
    case KEY_REWIND:
    case KEY_FASTFORWARD:
    case KEY_FORWARD:
    case KEY_MEDIA:
        return true;
    default:
        return false;
    }
#else
    Q_UNUSED(code);
    return false;
#endif
}

DeviceButtonPolicy BluetoothButtonControlManager::policyForAddress(const QString& address) const
{
    const QString key = normalizeAddress(address);
    return policies_.value(key, DeviceButtonPolicy::Allow);
}

void BluetoothButtonControlManager::setPolicyForAddress(const QString& address, DeviceButtonPolicy policy)
{
    const QString key = normalizeAddress(address);
    if (key.isEmpty()) {
        return;
    }
    const DeviceButtonPolicy previous = policies_.value(key, DeviceButtonPolicy::Allow);
    if (policy == DeviceButtonPolicy::Allow) {
        policies_.remove(key);
        removePersistedPolicy(key);
    } else {
        policies_.insert(key, policy);
        persistPolicy(key, policy);
    }
    if (previous != policy) {
        emit policyChanged(key);
    }
    if (connected_.value(key, false)) {
        syncDevice(key, true);
    } else if (policy == DeviceButtonPolicy::Allow) {
        applyAllow(key);
    } else {
        updateEffective(key, DeviceButtonEffectiveState::Unsupported);
    }
}

void BluetoothButtonControlManager::clearPolicyForAddress(const QString& address)
{
    setPolicyForAddress(address, DeviceButtonPolicy::Allow);
}

DeviceButtonEffectiveState BluetoothButtonControlManager::effectiveStateForAddress(const QString& address) const
{
    const QString key = normalizeAddress(address);
    if (policyForAddress(key) == DeviceButtonPolicy::Allow) {
        return DeviceButtonEffectiveState::Allowed;
    }
    return effective_.value(key, DeviceButtonEffectiveState::Unsupported);
}

QString BluetoothButtonControlManager::effectiveStatusTextForAddress(const QString& address) const
{
    return deviceButtonEffectiveStateText(effectiveStateForAddress(address));
}

bool BluetoothButtonControlManager::canControlButtonsForAddress(const QString& address) const
{
    return findEndpoint(address).has_value();
}

QString BluetoothButtonControlManager::matchedEventNodeForAddress(const QString& address) const
{
    if (const auto endpoint = findEndpoint(address)) {
        return endpoint->eventNode;
    }
    return {};
}

void BluetoothButtonControlManager::syncDevice(const QString& address, bool connected)
{
    const QString key = normalizeAddress(address);
    if (key.isEmpty()) {
        return;
    }
    connected_.insert(key, connected);
    if (!connected) {
        applyAllow(key);
        if (policyForAddress(key) == DeviceButtonPolicy::Disallow) {
            updateEffective(key, DeviceButtonEffectiveState::Unsupported);
        }
        return;
    }
    if (policyForAddress(key) == DeviceButtonPolicy::Disallow) {
        applyDisallow(key);
    } else {
        applyAllow(key);
    }
}

void BluetoothButtonControlManager::reapplyAll()
{
    const QList<QString> keys = connected_.keys();
    for (const QString& key : keys) {
        if (connected_.value(key)) {
            syncDevice(key, true);
        }
    }
}

void BluetoothButtonControlManager::releaseAll()
{
    const QList<QString> keys = grabs_.keys();
    for (const QString& key : keys) {
        applyAllow(key);
    }
}

void BluetoothButtonControlManager::loadPolicies()
{
    policies_.clear();
    if (!settings_) {
        return;
    }
    settings_->beginGroup(QString::fromLatin1(kSettingsGroup));
    const QStringList keys = settings_->childKeys();
    for (const QString& raw : keys) {
        const QString address = normalizeAddress(raw);
        if (address.isEmpty()) {
            continue;
        }
        if (settings_->value(raw).toString().compare(QStringLiteral("disallow"), Qt::CaseInsensitive) == 0
            || settings_->value(raw).toInt() == static_cast<int>(DeviceButtonPolicy::Disallow)) {
            policies_.insert(address, DeviceButtonPolicy::Disallow);
        }
    }
    settings_->endGroup();
}

void BluetoothButtonControlManager::persistPolicy(const QString& key, DeviceButtonPolicy policy)
{
    if (!settings_) {
        return;
    }
    settings_->setValue(
        settingsKeyForAddress(key),
        policy == DeviceButtonPolicy::Disallow ? QStringLiteral("disallow") : QStringLiteral("allow"));
    settings_->sync();
}

void BluetoothButtonControlManager::removePersistedPolicy(const QString& key)
{
    if (!settings_) {
        return;
    }
    settings_->remove(settingsKeyForAddress(key));
    settings_->sync();
}

std::optional<BluetoothInputEndpoint> BluetoothButtonControlManager::findEndpoint(const QString& address) const
{
    const QString key = normalizeAddress(address);
    for (const BluetoothInputEndpoint& endpoint : probeInputs()) {
        if (addressesMatch(endpoint.address, key)) {
            return endpoint;
        }
    }
    return std::nullopt;
}

QVector<BluetoothInputEndpoint> BluetoothButtonControlManager::probeInputs() const
{
    if (probeOverride_) {
        return probeOverride_();
    }

    QVector<BluetoothInputEndpoint> endpoints;
#if defined(__linux__)
    QFile devices(QStringLiteral("/proc/bus/input/devices"));
    if (!devices.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return endpoints;
    }
    const QString content = QString::fromUtf8(devices.readAll());
    const QStringList blocks = content.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
    static const QRegularExpression uniqRe(QStringLiteral(R"(^U:\s*Uniq=(.*)$)"), QRegularExpression::MultilineOption);
    static const QRegularExpression nameRe(QStringLiteral(R"(^N:\s*Name=\"(.*)\"$)"), QRegularExpression::MultilineOption);
    static const QRegularExpression handlersRe(QStringLiteral(R"(^H:\s*Handlers=(.*)$)"), QRegularExpression::MultilineOption);
    static const QRegularExpression physRe(QStringLiteral(R"(^P:\s*Phys=(.*)$)"), QRegularExpression::MultilineOption);
    static const QRegularExpression addressRe(
        QStringLiteral(R"((?:^|[^0-9A-F])([0-9A-F]{2}(?::[0-9A-F]{2}){5})(?:[^0-9A-F]|$))"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QString& block : blocks) {
        const QRegularExpressionMatch uniqMatch = uniqRe.match(block);
        const QRegularExpressionMatch nameMatch = nameRe.match(block);
        const QRegularExpressionMatch handlersMatch = handlersRe.match(block);
        const QRegularExpressionMatch physMatch = physRe.match(block);
        QString address;
        if (uniqMatch.hasMatch()) {
            address = normalizeAddress(uniqMatch.captured(1));
        }
        if (address.isEmpty() && physMatch.hasMatch()) {
            const QRegularExpressionMatch addrMatch = addressRe.match(physMatch.captured(1));
            if (addrMatch.hasMatch()) {
                address = normalizeAddress(addrMatch.captured(1));
            }
        }
        if (address.isEmpty() || !handlersMatch.hasMatch()) {
            continue;
        }
        QString eventNode;
        for (const QString& token : handlersMatch.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (token.startsWith(QStringLiteral("event"))) {
                eventNode = QStringLiteral("/dev/input/") + token;
                break;
            }
        }
        if (eventNode.isEmpty()) {
            continue;
        }
        BluetoothInputEndpoint endpoint;
        endpoint.address = address;
        endpoint.eventNode = eventNode;
        endpoint.name = nameMatch.hasMatch() ? nameMatch.captured(1) : eventNode;
        endpoints.push_back(endpoint);
    }
#endif
    return endpoints;
}

void BluetoothButtonControlManager::updateEffective(const QString& address, DeviceButtonEffectiveState state)
{
    const QString key = normalizeAddress(address);
    if (effective_.value(key, DeviceButtonEffectiveState::Allowed) == state) {
        return;
    }
    effective_.insert(key, state);
    emit effectiveStateChanged(key);
}

void BluetoothButtonControlManager::applyDisallow(const QString& address)
{
    const QString key = normalizeAddress(address);
    applyAllow(key);

    const auto endpoint = findEndpoint(key);
    if (!endpoint.has_value()) {
        updateEffective(key, DeviceButtonEffectiveState::Unsupported);
        return;
    }

    if (grabOverride_) {
        if (!grabOverride_(endpoint->eventNode)) {
            updateEffective(key, DeviceButtonEffectiveState::PermissionDenied);
            return;
        }
        grabs_.insert(key, ActiveGrab{endpoint->eventNode, -1});
        updateEffective(key, DeviceButtonEffectiveState::Suppressed);
        return;
    }

#if defined(__linux__)
    const int fd = ::open(endpoint->eventNode.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        qCWarning(auralisBluetooth) << "ButtonGrabOpenFailed" << endpoint->eventNode << std::strerror(errno);
        updateEffective(key, DeviceButtonEffectiveState::PermissionDenied);
        return;
    }
    if (::ioctl(fd, EVIOCGRAB, 1) != 0) {
        qCWarning(auralisBluetooth) << "ButtonGrabIoctlFailed" << endpoint->eventNode << std::strerror(errno);
        ::close(fd);
        updateEffective(key, DeviceButtonEffectiveState::PermissionDenied);
        return;
    }
    grabs_.insert(key, ActiveGrab{endpoint->eventNode, fd});
    updateEffective(key, DeviceButtonEffectiveState::Suppressed);
    qCInfo(auralisBluetooth) << "ButtonPolicySuppressed" << key << endpoint->eventNode;
#else
    updateEffective(key, DeviceButtonEffectiveState::Unsupported);
#endif
}

void BluetoothButtonControlManager::applyAllow(const QString& address)
{
    const QString key = normalizeAddress(address);
    if (grabs_.contains(key)) {
        ActiveGrab grab = grabs_.take(key);
        if (releaseOverride_) {
            releaseOverride_(grab.eventNode);
        } else if (grab.fd >= 0) {
#if defined(__linux__)
            ::ioctl(grab.fd, EVIOCGRAB, 0);
#endif
            ::close(grab.fd);
        }
    }
    if (policyForAddress(key) == DeviceButtonPolicy::Allow) {
        updateEffective(key, DeviceButtonEffectiveState::Allowed);
    }
}

} // namespace auralis::bluetooth
