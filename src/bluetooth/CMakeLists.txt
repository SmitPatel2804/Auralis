set(AURALIS_BLUETOOTH_SOURCES
    BluetoothDeviceListModel.cpp
    BluetoothError.cpp
    BlueZPropertyParser.cpp
    BlueZTypes.cpp
    DeviceOperation.cpp
    DeviceRegistry.cpp
    PairingRequest.cpp
    ReconnectPolicy.cpp
    UuidCatalog.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/AgentCapability.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothDeviceListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothButtonControlManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceButtonPolicy.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZPropertyParser.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceReconnectMetadata.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceRegistry.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/IBluetoothManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/PairingRequest.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/ReconnectPolicy.h"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND AURALIS_BLUETOOTH_SOURCES
        AdapterManager.cpp
        BlueZAgent.cpp
        BlueZAgentAdaptor.cpp
        BluetoothButtonControlManager.cpp
        BluetoothDbusError.cpp
        BluetoothManager.cpp
        BlueZDbusClient.cpp
        DeviceLifecycleManager.cpp
        DiscoveryManager.cpp
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/AdapterManager.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZAgent.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZAgentAdaptor.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothManager.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZDbusClient.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceLifecycleManager.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DiscoveryManager.h"
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/IBlueZClient.h"
    )
else()
    list(APPEND AURALIS_BLUETOOTH_SOURCES
        NativeBluetoothManager.cpp
        "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/NativeBluetoothManager.h"
    )
endif()

add_library(auralis-bluetooth ${AURALIS_BLUETOOTH_SOURCES})

add_library(Auralis::Bluetooth ALIAS auralis-bluetooth)

target_include_directories(auralis-bluetooth
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(auralis-bluetooth
    PUBLIC
        auralis-core
        Qt6::Core
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(auralis-bluetooth PUBLIC Qt6::DBus)
else()
    target_link_libraries(auralis-bluetooth PUBLIC Qt6::Bluetooth)
endif()

auralis_apply_warnings(auralis-bluetooth)
