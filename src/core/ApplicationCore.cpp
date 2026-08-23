#include <auralis/core/ApplicationCore.h>

#include <auralis/core/Logger.h>
#include <auralis/core/LoggingCategories.h>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QSysInfo>

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
    , notifications_(this)
    , diagnostics_(this)
    , platform_(this)
{
    platform_.configure(services_.bluetooth.get(), services_.pipeWire.get());
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
    currentPage_ = services_.configuration->lastNavPage();
    connect(
        services_.configuration.get(),
        &ConfigurationManager::showDeveloperStatusChanged,
        this,
        [this]() {
            showDeveloperStatus_ = services_.configuration->showDeveloperStatus();
            emit showDeveloperStatusChanged();
        });

    if (services_.configuration->fileLoggingEnabled()) {
        if (!Logger::enableFileLogging(services_.configuration->logFilePath())) {
            qCWarning(auralisCore)
                << "File logging requested but unavailable; continuing with console logging";
            services_.configuration->reconcileFileLoggingActivationFailure(
                QStringLiteral("Unable to enable file logging at the selected path."));
        } else {
            qCInfo(auralisCore) << "ExecutionLogStarted path=" << Logger::activeFilePath()
                                << "pid=" << QCoreApplication::applicationPid()
                                << "os=" << QSysInfo::prettyProductName()
                                << "arch=" << QSysInfo::currentCpuArchitecture()
                                << "qt=" << qVersion();
        }
    }

    if (!services_.bluetooth || !services_.pipeWire || !services_.devices || !services_.sessions) {
        qCCritical(auralisCore) << "ApplicationCore is missing one or more required service dependencies";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }

    if (!services_.bluetooth->initialize()) {
        qCCritical(auralisCore) << "Bluetooth service initialization failed";
        rollbackInitializedServices();
        setStatus(ServiceStatus::Error);
        return false;
    }
    qCInfo(auralisCore) << "Bluetooth service initialized backend=" << services_.bluetooth->backendName();

    if (!services_.pipeWire->initialize()) {
        qCWarning(auralisCore) << "Audio service failed to start; continuing without a live audio graph";
    } else {
        qCInfo(auralisCore) << "Audio service initialized backend=" << services_.pipeWire->backendName();
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

    if (QObject* sessionUi = services_.sessions->uiObject()) {
        QObject::connect(
            sessionUi,
            SIGNAL(currentSessionIdChanged()),
            this,
            SLOT(refreshActiveSessionCache()));
        QObject::connect(
            sessionUi,
            SIGNAL(sessionUpdated(QString)),
            this,
            SLOT(refreshActiveSessionCache()));
        QObject::connect(sessionUi, SIGNAL(sessionsChanged()), this, SLOT(refreshActiveSessionCache()));
    }

    refreshActiveSessionCache();
    connect(&notifications_, &ui::NotificationController::countChanged, this, [this]() {
        emit warningCountChanged();
        emit lastErrorTextChanged();
    });

    if (!services_.recovery) {
        services_.recovery = std::make_unique<recovery::RecoveryManager>(this);
    }
    if (!services_.powerMonitor) {
        services_.powerMonitor = std::make_unique<recovery::SystemPowerMonitor>(this);
    }
    services_.powerMonitor->initialize();
    services_.recovery->setAutoRecoverEnabled(services_.configuration->autoRecoverServices());
    services_.recovery->setRestoreOnResume(services_.configuration->restoreOnResume());
    connect(
        services_.configuration.get(),
        &ConfigurationManager::autoRecoverServicesChanged,
        services_.recovery.get(),
        [this]() {
            if (services_.recovery) {
                services_.recovery->setAutoRecoverEnabled(services_.configuration->autoRecoverServices());
            }
        });
    connect(
        services_.configuration.get(),
        &ConfigurationManager::restoreOnResumeChanged,
        services_.recovery.get(),
        [this]() {
            if (services_.recovery) {
                services_.recovery->setRestoreOnResume(services_.configuration->restoreOnResume());
            }
        });
    services_.recovery->initialize();
    connect(
        services_.recovery.get(),
        &recovery::RecoveryManager::statusChanged,
        this,
        &ApplicationCore::recoveryStatusChanged);
    connect(
        services_.recovery.get(),
        &recovery::RecoveryManager::recoveryExhausted,
        this,
        [this](const QString& domain, const QString& reason) {
            notifications_.postError(
                QStringLiteral("Recovery"),
                QStringLiteral("%1 recovery exhausted%2")
                    .arg(domain, reason.isEmpty() ? QString() : QStringLiteral(": %1").arg(reason)));
        });
    connect(
        services_.powerMonitor.get(),
        &recovery::SystemPowerMonitor::preparingForSleep,
        services_.recovery.get(),
        &recovery::RecoveryManager::onPreparingForSleep);

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

QObject* ApplicationCore::sessions() const
{
    return services_.sessions ? services_.sessions->uiObject() : nullptr;
}

QObject* ApplicationCore::configuration() const
{
    return services_.configuration.get();
}

QObject* ApplicationCore::notifications() const
{
    return const_cast<ui::NotificationController*>(&notifications_);
}

QObject* ApplicationCore::diagnostics() const
{
    return const_cast<ui::DiagnosticsLogModel*>(&diagnostics_);
}

QObject* ApplicationCore::recovery() const
{
    return services_.recovery.get();
}

recovery::RecoveryManager* ApplicationCore::recoveryManager() const noexcept
{
    return services_.recovery.get();
}

recovery::SystemPowerMonitor* ApplicationCore::powerMonitor() const noexcept
{
    return services_.powerMonitor.get();
}

QObject* ApplicationCore::platform() const
{
    return const_cast<PlatformCapabilities*>(&platform_);
}

bluetooth::IBluetoothManager* ApplicationCore::bluetoothService() const noexcept
{
    return services_.bluetooth.get();
}

audio::IAudioManager* ApplicationCore::audioService() const noexcept
{
    return services_.pipeWire.get();
}

QString ApplicationCore::recoveryStatus() const
{
    if (!services_.recovery) {
        return QStringLiteral("Recovery not initialized");
    }
    return services_.recovery->statusText();
}

int ApplicationCore::currentPage() const
{
    return currentPage_;
}

void ApplicationCore::setCurrentPage(int page)
{
    navigateTo(page);
}

void ApplicationCore::navigateTo(int page)
{
    if (page < 0) {
        page = 0;
    }
    if (page > kMaxNavPageIndex) {
        page = kMaxNavPageIndex;
    }
    if (currentPage_ == page) {
        return;
    }
    currentPage_ = page;
    qCInfo(auralisUi) << "Gui.NavigationChanged page=" << page;
    if (services_.configuration) {
        services_.configuration->setLastNavPage(page);
    }
    emit currentPageChanged();
}

int ApplicationCore::warningCount() const
{
    return notifications_.warningCount();
}

QString ApplicationCore::lastErrorText() const
{
    return notifications_.latestErrorText();
}

QString ApplicationCore::activeSessionName() const
{
    return activeSessionName_;
}

QString ApplicationCore::activeSessionId() const
{
    return activeSessionId_;
}

void ApplicationCore::refreshActiveSessionCache()
{
    QString id;
    QString name;
    if (QObject* sessionUi = sessions()) {
        id = sessionUi->property("currentSessionId").toString();
        const int count = sessionUi->property("sessionCount").toInt();
        if (!id.isEmpty()) {
            name = id;
        }
        Q_UNUSED(count)
        if (QAbstractItemModel* list = qvariant_cast<QAbstractItemModel*>(sessionUi->property("sessionList"))) {
            for (int row = 0; row < list->rowCount(); ++row) {
                const QModelIndex idx = list->index(row, 0);
                if (list->data(idx, Qt::UserRole + 1).toString() == id) {
                    name = list->data(idx, Qt::DisplayRole).toString();
                    break;
                }
            }
        }
    }
    if (activeSessionId_ == id && activeSessionName_ == name) {
        return;
    }
    activeSessionId_ = id;
    activeSessionName_ = name;
    emit activeSessionChanged();
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
    if (services_.recovery) {
        services_.recovery->shutdown();
    }
    if (services_.powerMonitor) {
        services_.powerMonitor->shutdown();
    }
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
