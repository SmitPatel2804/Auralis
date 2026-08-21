#include <auralis/core/ApplicationCore.h>
#include <auralis/core/Logger.h>
#include <auralis/core/QmlTypeRegistration.h>
#include <auralis/devices/DeviceManager.h>
#include <auralis/session/SessionManager.h>

#include <QtGlobal>

#if defined(Q_OS_LINUX)
#include <auralis/audio/PipeWireManager.h>
#include <auralis/bluetooth/BluetoothManager.h>
#else
#include <auralis/audio/NativeAudioManager.h>
#include <auralis/bluetooth/NativeBluetoothManager.h>
#endif

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtQml>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(Auralis_UiPlugin)

class TstQmlComponents : public QObject {
    Q_OBJECT

private slots:
    void statusBadgeLoads()
    {
        auralis::core::Logger::initialize();
        auralis::core::ApplicationServices services;
#if defined(Q_OS_LINUX)
        services.bluetooth = std::make_unique<auralis::bluetooth::BluetoothManager>();
        auto* bluetooth = static_cast<auralis::bluetooth::BluetoothManager*>(services.bluetooth.get());
        services.pipeWire = std::make_unique<auralis::audio::PipeWireManager>(bluetooth->deviceRegistry());
#else
        services.bluetooth = std::make_unique<auralis::bluetooth::NativeBluetoothManager>();
        auto* bluetooth = static_cast<auralis::bluetooth::NativeBluetoothManager*>(services.bluetooth.get());
        services.pipeWire = std::make_unique<auralis::audio::NativeAudioManager>(bluetooth->deviceRegistry());
#endif
        services.devices = std::make_unique<auralis::devices::DeviceManager>();
        services.sessions = std::make_unique<auralis::session::SessionManager>(
            services.bluetooth.get(),
            services.pipeWire.get(),
            QString());
        auralis::core::ApplicationCore core(std::move(services));
        QVERIFY(core.initialize());
        auralis::core::registerAuralisQmlTypes();
        qmlRegisterSingletonInstance("Auralis", 1, 0, "AppCore", &core);

        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/Auralis/Ui/StatusBadge.qml")));
        if (component.isError()) {
            // Fallback: module-relative type via loadFromModule is covered by desktop smoke.
            QSKIP(component.errorString().toUtf8().constData());
        }
        QObject* obj = component.create();
        QVERIFY2(obj != nullptr, component.errorString().toUtf8().constData());
        delete obj;
        core.shutdown();
        auralis::core::Logger::shutdown();
    }
};

QTEST_MAIN(TstQmlComponents)
#include "tst_QmlComponents.moc"
