#include <auralis/core/ConfigurationManager.h>

#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QVariant>

#include <optional>

namespace auralis::core {
namespace {

constexpr auto kApplicationNameKey = "application/name";
constexpr auto kFileLoggingEnabledKey = "logging/fileEnabled";
constexpr auto kFileLoggingPathKey = "logging/filePath";
constexpr auto kShowDeveloperStatusKey = "ui/showDeveloperStatus";
constexpr auto kRestoreLastSessionKey = "ui/restoreLastSession";
constexpr auto kAutoRecoverServicesKey = "recovery/autoRecoverServices";
constexpr auto kRestoreOnResumeKey = "recovery/restoreOnResume";
constexpr auto kLastNavPageKey = "ui/lastNavPage";
constexpr auto kWindowWidthKey = "ui/windowWidth";
constexpr auto kWindowHeightKey = "ui/windowHeight";
constexpr int kMinimumWindowWidth = 880;
constexpr int kMinimumWindowHeight = 600;

constexpr auto kEnvFileLoggingEnabled = "AURALIS_LOG_FILE_ENABLED";
constexpr auto kEnvFileLoggingPath = "AURALIS_LOG_FILE_PATH";
constexpr auto kEnvShowDeveloperStatus = "AURALIS_UI_SHOW_DEVELOPER_STATUS";

std::optional<bool> parseBool(const QByteArray& raw)
{
    const QByteArray value = raw.trimmed().toLower();
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return std::nullopt;
}

QString defaultExecutionLogPath()
{
    QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataRoot.trimmed().isEmpty()) {
        dataRoot = QDir::currentPath();
    }
    QDir logDirectory(QDir(dataRoot).filePath(QStringLiteral("logs")));
    if (!logDirectory.exists()) {
        QDir().mkpath(logDirectory.absolutePath());
    }
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return logDirectory.filePath(
        QStringLiteral("auralis-%1-p%2.log").arg(timestamp).arg(QCoreApplication::applicationPid()));
}

} // namespace

ConfigurationManager::ConfigurationManager(QObject* parent)
    : ConfigurationManager(
          std::make_unique<QSettings>(QStringLiteral("Auralis"), QStringLiteral("Auralis")),
          parent)
{
}

ConfigurationManager::ConfigurationManager(std::unique_ptr<QSettings> settings, QObject* parent)
    : QObject(parent)
    , settings_(std::move(settings))
{
}

bool ConfigurationManager::initialize()
{
    if (initialized_) {
        return true;
    }

    if (!settings_) {
        qCCritical(auralisConfig) << "Configuration manager has no settings backend";
        return false;
    }

    applicationName_ = settings_->value(kApplicationNameKey, QStringLiteral("Auralis")).toString();
    const bool hasExplicitFileLoggingPreference = settings_->contains(kFileLoggingEnabledKey);
    fileLoggingEnabled_ = settings_->value(kFileLoggingEnabledKey, true).toBool();
    logFilePath_ = settings_->value(kFileLoggingPathKey, QString()).toString();
    // Before execution-wise logs existed, a stale path could remain even when
    // no logging preference had ever been persisted. Migrate that legacy state
    // to the timestamped per-run default; explicit user preferences are kept.
    if (!hasExplicitFileLoggingPreference || logFilePath_.trimmed().isEmpty()) {
        logFilePath_ = defaultExecutionLogPath();
    }
    showDeveloperStatus_ = settings_->value(kShowDeveloperStatusKey, true).toBool();
    restoreLastSession_ = settings_->value(kRestoreLastSessionKey, false).toBool();
    autoRecoverServices_ = settings_->value(kAutoRecoverServicesKey, true).toBool();
    restoreOnResume_ = settings_->value(kRestoreOnResumeKey, true).toBool();
    lastNavPage_ = qBound(0, settings_->value(kLastNavPageKey, 0).toInt(), 5);
    windowWidth_ = qMax(kMinimumWindowWidth, settings_->value(kWindowWidthKey, 1280).toInt());
    windowHeight_ = qMax(kMinimumWindowHeight, settings_->value(kWindowHeightKey, 800).toInt());

    applyEnvironmentOverrides();

    initialized_ = true;
    qCInfo(auralisConfig) << "Configuration initialized";
    return true;
}

bool ConfigurationManager::isInitialized() const noexcept
{
    return initialized_;
}

QString ConfigurationManager::applicationName() const
{
    return applicationName_;
}

bool ConfigurationManager::fileLoggingEnabled() const
{
    return fileLoggingEnabled_;
}

QString ConfigurationManager::logFilePath() const
{
    return logFilePath_;
}

bool ConfigurationManager::showDeveloperStatus() const
{
    return showDeveloperStatus_;
}

bool ConfigurationManager::restoreLastSession() const
{
    return restoreLastSession_;
}

bool ConfigurationManager::autoRecoverServices() const
{
    return autoRecoverServices_;
}

bool ConfigurationManager::restoreOnResume() const
{
    return restoreOnResume_;
}

int ConfigurationManager::lastNavPage() const
{
    return lastNavPage_;
}

int ConfigurationManager::windowWidth() const
{
    return windowWidth_;
}

int ConfigurationManager::windowHeight() const
{
    return windowHeight_;
}

QString ConfigurationManager::lastErrorText() const
{
    return lastErrorText_;
}

bool ConfigurationManager::fileLoggingEnvLocked() const noexcept
{
    return fileLoggingEnvLocked_ || logPathEnvLocked_;
}

bool ConfigurationManager::developerStatusEnvLocked() const noexcept
{
    return developerStatusEnvLocked_;
}

bool ConfigurationManager::setFileLoggingEnabled(bool enabled)
{
    if (fileLoggingEnvLocked_) {
        setLastError(QStringLiteral("File logging is locked by AURALIS_LOG_FILE_ENABLED"));
        return false;
    }
    if (fileLoggingEnabled_ == enabled) {
        // Configuration is loaded before ApplicationCore activates the file
        // sink. Keep an idempotent "enable" capable of repairing that runtime
        // state (also useful after a recoverable sink failure).
        if (enabled && Logger::isInitialized() && !Logger::isFileLoggingActive()) {
            if (logFilePath_.trimmed().isEmpty()) {
                setLastError(QStringLiteral("Choose a log file path before enabling file logging."));
                return false;
            }
#ifdef AURALIS_ENABLE_FILE_LOGGING
            if (!Logger::enableFileLogging(logFilePath_)) {
                setLastError(QStringLiteral("Unable to enable file logging at the selected path."));
                return false;
            }
#else
            setLastError(QStringLiteral("File logging support is not compiled into this build."));
            return false;
#endif
        }
        setLastError({});
        return true;
    }

    if (enabled) {
        if (logFilePath_.trimmed().isEmpty()) {
            setLastError(QStringLiteral("Choose a log file path before enabling file logging."));
            return false;
        }

#ifdef AURALIS_ENABLE_FILE_LOGGING
        if (Logger::isInitialized()) {
            if (!Logger::enableFileLogging(logFilePath_)) {
                setLastError(QStringLiteral("Unable to enable file logging at the selected path."));
                return false;
            }
        }
#else
        setLastError(QStringLiteral("File logging support is not compiled into this build."));
        return false;
#endif

        if (!writeValue(kFileLoggingEnabledKey, true)) {
#ifdef AURALIS_ENABLE_FILE_LOGGING
            if (Logger::isInitialized()) {
                Logger::disableFileLogging();
            }
#endif
            setLastError(QStringLiteral("Unable to save the file logging setting."));
            return false;
        }

        fileLoggingEnabled_ = true;
        emit fileLoggingEnabledChanged();
        setLastError({});
        return true;
    }

    const bool wasActive = Logger::isInitialized() && Logger::isFileLoggingActive();
#ifdef AURALIS_ENABLE_FILE_LOGGING
    if (Logger::isInitialized()) {
        Logger::disableFileLogging();
    }
#endif

    if (!writeValue(kFileLoggingEnabledKey, false)) {
#ifdef AURALIS_ENABLE_FILE_LOGGING
        if (wasActive && Logger::isInitialized() && !logFilePath_.trimmed().isEmpty()) {
            Logger::enableFileLogging(logFilePath_);
        }
#else
        Q_UNUSED(wasActive)
#endif
        setLastError(QStringLiteral("Unable to save the file logging setting."));
        return false;
    }

    fileLoggingEnabled_ = false;
    emit fileLoggingEnabledChanged();
    setLastError({});
    return true;
}

void ConfigurationManager::reconcileFileLoggingActivationFailure(const QString& reason)
{
    const QString message = reason.trimmed().isEmpty()
        ? QStringLiteral("Unable to enable file logging at the selected path.")
        : reason;

    if (fileLoggingEnvLocked_) {
        setLastError(message);
        return;
    }

    if (fileLoggingEnabled_) {
        writeValue(kFileLoggingEnabledKey, false);
        fileLoggingEnabled_ = false;
        emit fileLoggingEnabledChanged();
    }

#ifdef AURALIS_ENABLE_FILE_LOGGING
    if (Logger::isInitialized() && Logger::isFileLoggingActive()) {
        Logger::disableFileLogging();
    }
#endif

    setLastError(message);
}

bool ConfigurationManager::setLogFilePath(const QString& path)
{
    if (logPathEnvLocked_) {
        setLastError(QStringLiteral("Log file path is locked by AURALIS_LOG_FILE_PATH"));
        return false;
    }
    if (logFilePath_ == path) {
        return true;
    }
    if (!writeValue(kFileLoggingPathKey, path)) {
        return false;
    }
    // Persisting a custom path also makes the logging choice explicit, so the
    // legacy-state migration does not replace it with a generated path on the
    // next execution.
    if (!settings_->contains(kFileLoggingEnabledKey)
        && !writeValue(kFileLoggingEnabledKey, fileLoggingEnabled_)) {
        return false;
    }
    logFilePath_ = path;
    emit logFilePathChanged();
    return true;
}

bool ConfigurationManager::setShowDeveloperStatus(bool enabled)
{
    if (developerStatusEnvLocked_) {
        setLastError(QStringLiteral("Developer status is locked by AURALIS_UI_SHOW_DEVELOPER_STATUS"));
        return false;
    }
    if (showDeveloperStatus_ == enabled) {
        return true;
    }
    if (!writeValue(kShowDeveloperStatusKey, enabled)) {
        return false;
    }
    showDeveloperStatus_ = enabled;
    emit showDeveloperStatusChanged();
    return true;
}

bool ConfigurationManager::setRestoreLastSession(bool enabled)
{
    if (restoreLastSession_ == enabled) {
        return true;
    }
    if (!writeValue(kRestoreLastSessionKey, enabled)) {
        return false;
    }
    restoreLastSession_ = enabled;
    emit restoreLastSessionChanged();
    return true;
}

bool ConfigurationManager::setAutoRecoverServices(bool enabled)
{
    if (autoRecoverServices_ == enabled) {
        return true;
    }
    if (!writeValue(kAutoRecoverServicesKey, enabled)) {
        return false;
    }
    autoRecoverServices_ = enabled;
    emit autoRecoverServicesChanged();
    return true;
}

bool ConfigurationManager::setRestoreOnResume(bool enabled)
{
    if (restoreOnResume_ == enabled) {
        return true;
    }
    if (!writeValue(kRestoreOnResumeKey, enabled)) {
        return false;
    }
    restoreOnResume_ = enabled;
    emit restoreOnResumeChanged();
    return true;
}

bool ConfigurationManager::setLastNavPage(int page)
{
    if (page < 0) {
        page = 0;
    }
    if (page > 5) {
        page = 5;
    }
    if (lastNavPage_ == page) {
        return true;
    }
    if (!writeValue(kLastNavPageKey, page)) {
        return false;
    }
    lastNavPage_ = page;
    emit lastNavPageChanged();
    return true;
}

bool ConfigurationManager::setWindowWidth(int width)
{
    if (width < kMinimumWindowWidth) {
        width = kMinimumWindowWidth;
    }
    if (windowWidth_ == width) {
        return true;
    }
    if (!writeValue(kWindowWidthKey, width)) {
        return false;
    }
    windowWidth_ = width;
    emit windowGeometryChanged();
    return true;
}

bool ConfigurationManager::setWindowHeight(int height)
{
    if (height < kMinimumWindowHeight) {
        height = kMinimumWindowHeight;
    }
    if (windowHeight_ == height) {
        return true;
    }
    if (!writeValue(kWindowHeightKey, height)) {
        return false;
    }
    windowHeight_ = height;
    emit windowGeometryChanged();
    return true;
}

bool ConfigurationManager::resetToDefaults()
{
    if (fileLoggingEnvLocked_ || logPathEnvLocked_ || developerStatusEnvLocked_) {
        setLastError(QStringLiteral("Some settings are locked by environment variables and were left unchanged"));
    } else {
        setLastError({});
    }
    bool ok = true;
    if (!fileLoggingEnvLocked_) {
        ok = setFileLoggingEnabled(false) && ok;
    }
    if (!logPathEnvLocked_) {
        ok = setLogFilePath(defaultExecutionLogPath()) && ok;
    }
    if (!fileLoggingEnvLocked_) {
        ok = setFileLoggingEnabled(true) && ok;
    }
    if (!developerStatusEnvLocked_) {
        ok = setShowDeveloperStatus(true) && ok;
    }
    ok = setRestoreLastSession(false) && ok;
    ok = setAutoRecoverServices(true) && ok;
    ok = setRestoreOnResume(true) && ok;
    ok = setLastNavPage(0) && ok;
    ok = setWindowWidth(1280) && ok;
    ok = setWindowHeight(800) && ok;
    return ok;
}

void ConfigurationManager::applyEnvironmentOverrides()
{
    if (qEnvironmentVariableIsSet(kEnvFileLoggingEnabled)) {
        const auto parsed = parseBool(qgetenv(kEnvFileLoggingEnabled));
        if (parsed.has_value()) {
            fileLoggingEnabled_ = *parsed;
            fileLoggingEnvLocked_ = true;
        } else {
            qCWarning(auralisConfig)
                << "Ignoring invalid boolean for" << kEnvFileLoggingEnabled
                << ":" << qgetenv(kEnvFileLoggingEnabled);
        }
    }

    if (qEnvironmentVariableIsSet(kEnvFileLoggingPath)) {
        logFilePath_ = QString::fromLocal8Bit(qgetenv(kEnvFileLoggingPath));
        logPathEnvLocked_ = true;
    }

    if (qEnvironmentVariableIsSet(kEnvShowDeveloperStatus)) {
        const auto parsed = parseBool(qgetenv(kEnvShowDeveloperStatus));
        if (parsed.has_value()) {
            showDeveloperStatus_ = *parsed;
            developerStatusEnvLocked_ = true;
        } else {
            qCWarning(auralisConfig)
                << "Ignoring invalid boolean for" << kEnvShowDeveloperStatus
                << ":" << qgetenv(kEnvShowDeveloperStatus);
        }
    }
}

bool ConfigurationManager::writeValue(const char* key, const QVariant& value)
{
    if (!settings_) {
        setLastError(QStringLiteral("Settings backend is unavailable"));
        return false;
    }
    settings_->setValue(QString::fromLatin1(key), value);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        setLastError(QStringLiteral("Failed to persist setting"));
        qCWarning(auralisConfig) << "Failed to persist" << key;
        return false;
    }
    setLastError({});
    return true;
}

void ConfigurationManager::setLastError(const QString& text)
{
    if (lastErrorText_ == text) {
        return;
    }
    lastErrorText_ = text;
    emit lastErrorTextChanged();
}

} // namespace auralis::core
