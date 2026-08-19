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
    };

    static bool initialize();
    static bool initialize(const Options& options);
    static void shutdown();
    static bool isInitialized();
    static bool isFileLoggingActive();

    // Optional file sink. Failure is non-fatal: console logging continues.
    static bool enableFileLogging(const QString& filePath);
    static void disableFileLogging();

    using Observer = std::function<void(QtMsgType type, const QString& category, const QString& message)>;
    static void setObserver(Observer observer);

private:
    Logger() = delete;
};

} // namespace auralis::core
