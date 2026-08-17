#pragma once

#include <auralis/audio/IPipeWireManager.h>
#include <auralis/bluetooth/IBluetoothManager.h>
#include <auralis/core/ConfigurationManager.h>
#include <auralis/core/ServiceStatus.h>
#include <auralis/devices/IDeviceManager.h>
#include <auralis/session/ISessionManager.h>

#include <QDir>
#include <QSettings>
#include <QUuid>

namespace auralis::test {

class FakeConfigurationManager final : public core::ConfigurationManager {
public:
    explicit FakeConfigurationManager(bool initializeResult)
        : core::ConfigurationManager(std::make_unique<QSettings>(
              QDir::temp().filePath(
                  QStringLiteral("auralis-fake-%1.ini")
                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))),
              QSettings::IniFormat))
        , initializeResult_(initializeResult)
    {
    }

    bool initialize() override
    {
        initializeCalled_ = true;
        if (!initializeResult_) {
            return false;
        }
        return core::ConfigurationManager::initialize();
    }

    bool initializeCalled() const noexcept
    {
        return initializeCalled_;
    }

private:
    bool initializeResult_ = true;
    bool initializeCalled_ = false;
};

template <typename Interface>
class FakeService final : public Interface {
public:
    explicit FakeService(bool initializeResult)
        : initializeResult_(initializeResult)
    {
    }

    bool initialize() override
    {
        initializeCalled_ = true;
        if (!initializeResult_) {
            status_ = core::ServiceStatus::Error;
            return false;
        }

        status_ = core::ServiceStatus::Ready;
        return true;
    }

    void shutdown() override
    {
        shutdownCalled_ = true;
        status_ = core::ServiceStatus::Uninitialized;
    }

    core::ServiceStatus status() const noexcept override
    {
        return status_;
    }

    bool initializeCalled() const noexcept
    {
        return initializeCalled_;
    }

    bool shutdownCalled() const noexcept
    {
        return shutdownCalled_;
    }

private:
    bool initializeResult_ = true;
    bool initializeCalled_ = false;
    bool shutdownCalled_ = false;
    core::ServiceStatus status_ = core::ServiceStatus::Uninitialized;
};

using FakeBluetoothManager = FakeService<bluetooth::IBluetoothManager>;
using FakePipeWireManager = FakeService<audio::IPipeWireManager>;
using FakeDeviceManager = FakeService<devices::IDeviceManager>;
using FakeSessionManager = FakeService<session::ISessionManager>;

} // namespace auralis::test
