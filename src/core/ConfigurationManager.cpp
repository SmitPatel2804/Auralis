#include <auralis/core/ConfigurationManager.h>

#include <auralis/core/LoggingCategories.h>

#include <QByteArray>
#include <optional>

namespace auralis::core {
namespace {

constexpr auto kApplicationNameKey = "application/name";
constexpr auto kFileLoggingEnabledKey = "logging/fileEnabled";
constexpr auto kFileLoggingPathKey = "logging/filePath";
constexpr auto kShowDeveloperStatusKey = "ui/showDeveloperStatus";

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

} // namespace

ConfigurationManager::ConfigurationManager()
    : ConfigurationManager(std::make_unique<QSettings>(
          QStringLiteral("Auralis"),
          QStringLiteral("Auralis")))
{
}

ConfigurationManager::ConfigurationManager(std::unique_ptr<QSettings> settings)
    : settings_(std::move(settings))
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
    fileLoggingEnabled_ = settings_->value(kFileLoggingEnabledKey, false).toBool();
    logFilePath_ = settings_->value(kFileLoggingPathKey, QString()).toString();
    showDeveloperStatus_ = settings_->value(kShowDeveloperStatusKey, true).toBool();

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

void ConfigurationManager::applyEnvironmentOverrides()
{
    if (qEnvironmentVariableIsSet(kEnvFileLoggingEnabled)) {
        const auto parsed = parseBool(qgetenv(kEnvFileLoggingEnabled));
        if (parsed.has_value()) {
            fileLoggingEnabled_ = *parsed;
        } else {
            qCWarning(auralisConfig)
                << "Ignoring invalid boolean for" << kEnvFileLoggingEnabled
                << ":" << qgetenv(kEnvFileLoggingEnabled);
        }
    }

    if (qEnvironmentVariableIsSet(kEnvFileLoggingPath)) {
        logFilePath_ = QString::fromLocal8Bit(qgetenv(kEnvFileLoggingPath));
    }

    if (qEnvironmentVariableIsSet(kEnvShowDeveloperStatus)) {
        const auto parsed = parseBool(qgetenv(kEnvShowDeveloperStatus));
        if (parsed.has_value()) {
            showDeveloperStatus_ = *parsed;
        } else {
            qCWarning(auralisConfig)
                << "Ignoring invalid boolean for" << kEnvShowDeveloperStatus
                << ":" << qgetenv(kEnvShowDeveloperStatus);
        }
    }
}

} // namespace auralis::core
