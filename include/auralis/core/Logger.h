#pragma once

#include <QString>
#include <QtGlobal>

#include <functional>

namespace auralis::core {

class Logger final {
public:
    struct Options {
        bool enableConsole = true;
        bool enableFile = false;
        QString filePath;
        qint64 maxFileBytes = 5 * 1024 * 1024;
        int retainRotatedFiles = 3;
    };

    static bool initialize();
    static bool initialize(const Options& options);
    static void shutdown();
    static bool isInitialized();
    static bool isFileLoggingActive();
    static QString activeFilePath();

    // Optional file sink. Failure is non-fatal: console logging continues.
    static bool enableFileLogging(const QString& filePath);
    static void disableFileLogging();

    /// Configure rotation for the active/future file sink (bytes, retain count).
    static void setRotationPolicy(qint64 maxFileBytes, int retainRotatedFiles);

    using Observer = std::function<void(QtMsgType type, const QString& category, const QString& message)>;
    static void setObserver(Observer observer);

private:
    Logger() = delete;
};

} // namespace auralis::core
