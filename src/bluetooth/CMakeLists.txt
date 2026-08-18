add_library(auralis-bluetooth
    AdapterManager.cpp
    BluetoothDeviceListModel.cpp
    BluetoothError.cpp
    BluetoothManager.cpp
    BlueZDbusClient.cpp
    BlueZPropertyParser.cpp
    BlueZTypes.cpp
    DeviceRegistry.cpp
    DiscoveryManager.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/AdapterManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothDeviceListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BluetoothManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/BlueZDbusClient.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DeviceRegistry.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/DiscoveryManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/bluetooth/IBlueZClient.h"
)

add_library(Auralis::Bluetooth ALIAS auralis-bluetooth)

target_link_libraries(auralis-bluetooth
    PUBLIC
        auralis-core
        Qt6::DBus
)

auralis_apply_warnings(auralis-bluetooth)
