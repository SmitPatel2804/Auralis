add_library(auralis-bluetooth
    AdapterManager.cpp
    BlueZAgent.cpp
    BlueZAgentAdaptor.cpp
    BluetoothDeviceListModel.cpp
    BluetoothDbusError.cpp
    BluetoothError.cpp
    BluetoothManager.cpp
    BlueZDbusClient.cpp
    BlueZPropertyParser.cpp
    BlueZTypes.cpp
    DeviceLifecycleManager.cpp
    DeviceRegistry.cpp
    DiscoveryManager.cpp
    DeviceOperation.cpp
    PairingRequest.cpp
    ReconnectPolicy.cpp
    UuidCatalog.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/AgentCapability.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/AdapterManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZAgent.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZAgentAdaptor.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothDeviceListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZDbusClient.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceLifecycleManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceRegistry.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DiscoveryManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/IBlueZClient.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/PairingRequest.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/ReconnectPolicy.h"
)

add_library(Auralis::Bluetooth ALIAS auralis-bluetooth)

target_include_directories(auralis-bluetooth
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-bluetooth
    PUBLIC
        auralis-core
        Qt6::Core
        Qt6::DBus
)

auralis_apply_warnings(auralis-bluetooth)
