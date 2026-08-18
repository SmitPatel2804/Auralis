#include <auralis/core/ApplicationCore.h>

#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>

namespace auralis::core {
namespace {

QString serviceStatusString(const auto* service)
{
    if (service == nullptr) {
        return toString(ServiceStatus::Uninitialized);
    }
    return toString(service->status());
}

} // namespace

ApplicationCore::ApplicationCore(ApplicationServices services, QObject* parent)
    : QObject(parent)
    , services_(std::move(services))
{
}

ApplicationCore::~ApplicationCore()
{
    shutdown();
}

bool ApplicationCore::initialize()
{
    if (status_ == ServiceStatus::Ready) {
        return true;
    }

    setStatus(ServiceStatus::Initializing);

    if (!Logger::isInitialized()) {
        Logger::initialize();
    }

    if (!services_.configuration) {
        services_.configuration = std::make_unique<ConfigurationManager>();
    }

    if (!services_.configuration->initialize()) {
        qCCritical(auralisCore) << "Configuration initialization failed";
        setStatus(ServiceStatus::Error);
        return false;
    }

    showDeveloperStatus_ = services_.configuration->showDeveloperStatus();

    if (services_.configuration->fileLoggingEnabled()) {
        if (!Logger::enableFileLogging(services_.configuration->logFilePath())) {
            qCWarning(auralisCore)
                << "File logging requested but unavailable; continuing with console logging";
        }
    }

    if (!services_.bluetooth || !services_.pipeWire || !services_.devices || !services_.sessions) {
        qCCritical(auralisCore) << "ApplicationCore is missing one or more required service dependencies";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }

    if (!services_.bluetooth->initialize()) {
        qCCritical(auralisCore) << "Bluetooth service skeleton initialization failed";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }
    qCInfo(auralisCore) << "Bluetooth service skeleton initialized";

    if (!services_.pipeWire->initialize()) {
        qCWarning(auralisCore) << "PipeWire service failed to start; continuing without a live audio graph";
    } else {
        qCInfo(auralisCore) << "PipeWire service initialized";
    }
    if (QObject* audioUi = services_.pipeWire->uiObject()) {
        QObject::connect(audioUi, SIGNAL(statusChanged()), this, SIGNAL(statusChanged()));
        QObject::connect(audioUi, SIGNAL(connectionStateChanged()), this, SIGNAL(statusChanged()));
    }

    if (!services_.devices->initialize()) {
        qCCritical(auralisCore) << "Device service skeleton initialization failed";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }
    qCInfo(auralisCore) << "Device service skeleton initialized";

    if (!services_.sessions->initialize()) {
        qCCritical(auralisCore) << "Session service skeleton initialization failed";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }
    qCInfo(auralisCore) << "Session service skeleton initialized";

    setStatus(ServiceStatus::Ready);
    qCInfo(auralisCore) << "Application core ready";
    return true;
}

void ApplicationCore::shutdown()
{
    if (status_ == ServiceStatus::Uninitialized) {
        return;
    }

    qCInfo(auralisCore) << "Application core shutting down";
    rollbackInitializedServices();
    setStatus(ServiceStatus::Uninitialized);
}

bool ApplicationCore::isReady() const noexcept
{
    return status_ == ServiceStatus::Ready;
}

bool ApplicationCore::showDeveloperStatus() const
{
    return showDeveloperStatus_;
}

QString ApplicationCore::bluetoothStatus() const
{
    return serviceStatusString(services_.bluetooth.get());
}

QObject* ApplicationCore::bluetooth() const
{
    return services_.bluetooth ? services_.bluetooth->uiObject() : nullptr;
}

QString ApplicationCore::audioStatus() const
{
    return pipeWireStatus();
}

QObject* ApplicationCore::audio() const
{
    return services_.pipeWire ? services_.pipeWire->uiObject() : nullptr;
}

QString ApplicationCore::pipeWireStatus() const
{
    return serviceStatusString(services_.pipeWire.get());
}

QString ApplicationCore::coreStatus() const
{
    return toString(status_);
}

void ApplicationCore::setStatus(ServiceStatus status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;
    emit statusChanged();
}

void ApplicationCore::rollbackInitializedServices()
{
    if (services_.sessions) {
        services_.sessions->shutdown();
    }
    if (services_.devices) {
        services_.devices->shutdown();
    }
    if (services_.pipeWire) {
        services_.pipeWire->shutdown();
    }
    if (services_.bluetooth) {
        services_.bluetooth->shutdown();
    }
}

} // namespace auralis::core
