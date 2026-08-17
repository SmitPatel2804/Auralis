#pragma once

#include <QSettings>
#include <QString>

#include <memory>

namespace auralis::core {

class ConfigurationManager {
public:
    ConfigurationManager();
    explicit ConfigurationManager(std::unique_ptr<QSettings> settings);
    virtual ~ConfigurationManager() = default;

    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;

    virtual bool initialize();
    bool isInitialized() const noexcept;

    QString applicationName() const;
    bool fileLoggingEnabled() const;
    QString logFilePath() const;
    bool showDeveloperStatus() const;

private:
    void applyEnvironmentOverrides();

    std::unique_ptr<QSettings> settings_;
    bool initialized_ = false;
    QString applicationName_{QStringLiteral("Auralis")};
    bool fileLoggingEnabled_ = false;
    QString logFilePath_;
    bool showDeveloperStatus_ = true;
};

} // namespace auralis::core
