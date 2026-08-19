#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

#include <memory>

namespace auralis::core {

class ConfigurationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
    Q_PROPERTY(bool fileLoggingEnabled READ fileLoggingEnabled WRITE setFileLoggingEnabled NOTIFY fileLoggingEnabledChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath WRITE setLogFilePath NOTIFY logFilePathChanged)
    Q_PROPERTY(bool showDeveloperStatus READ showDeveloperStatus WRITE setShowDeveloperStatus NOTIFY showDeveloperStatusChanged)
    Q_PROPERTY(bool restoreLastSession READ restoreLastSession WRITE setRestoreLastSession NOTIFY restoreLastSessionChanged)
    Q_PROPERTY(int lastNavPage READ lastNavPage WRITE setLastNavPage NOTIFY lastNavPageChanged)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowGeometryChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowGeometryChanged)
    Q_PROPERTY(QString lastErrorText READ lastErrorText NOTIFY lastErrorTextChanged)
    Q_PROPERTY(bool fileLoggingEnvLocked READ fileLoggingEnvLocked CONSTANT)
    Q_PROPERTY(bool developerStatusEnvLocked READ developerStatusEnvLocked CONSTANT)

public:
    explicit ConfigurationManager(QObject* parent = nullptr);
    explicit ConfigurationManager(std::unique_ptr<QSettings> settings, QObject* parent = nullptr);
    ~ConfigurationManager() override = default;

    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;

    virtual bool initialize();
    bool isInitialized() const noexcept;

    QString applicationName() const;
    bool fileLoggingEnabled() const;
    QString logFilePath() const;
    bool showDeveloperStatus() const;
    bool restoreLastSession() const;
    int lastNavPage() const;
    int windowWidth() const;
    int windowHeight() const;
    QString lastErrorText() const;
    bool fileLoggingEnvLocked() const noexcept;
    bool developerStatusEnvLocked() const noexcept;

    Q_INVOKABLE bool setFileLoggingEnabled(bool enabled);
    Q_INVOKABLE bool setLogFilePath(const QString& path);
    Q_INVOKABLE bool setShowDeveloperStatus(bool enabled);
    Q_INVOKABLE bool setRestoreLastSession(bool enabled);
    Q_INVOKABLE bool setLastNavPage(int page);
    Q_INVOKABLE bool setWindowWidth(int width);
    Q_INVOKABLE bool setWindowHeight(int height);
    Q_INVOKABLE bool resetToDefaults();

signals:
    void fileLoggingEnabledChanged();
    void logFilePathChanged();
    void showDeveloperStatusChanged();
    void restoreLastSessionChanged();
    void lastNavPageChanged();
    void windowGeometryChanged();
    void lastErrorTextChanged();

private:
    void applyEnvironmentOverrides();
    bool writeValue(const char* key, const QVariant& value);
    void setLastError(const QString& text);
    void applyRuntimeFileLogging();

    std::unique_ptr<QSettings> settings_;
    bool initialized_ = false;
    QString applicationName_{QStringLiteral("Auralis")};
    bool fileLoggingEnabled_ = false;
    QString logFilePath_;
    bool showDeveloperStatus_ = true;
    bool restoreLastSession_ = false;
    int lastNavPage_ = 0;
    int windowWidth_ = 1280;
    int windowHeight_ = 800;
    QString lastErrorText_;
    bool fileLoggingEnvLocked_ = false;
    bool logPathEnvLocked_ = false;
    bool developerStatusEnvLocked_ = false;
};

} // namespace auralis::core
