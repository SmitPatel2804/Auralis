if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_BluetoothManager tst_BluetoothManager.cpp)
    target_link_libraries(tst_BluetoothManager PRIVATE auralis-bluetooth)
    target_include_directories(tst_BluetoothManager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

endif()

auralis_add_test(tst_BlueZPropertyParser tst_BlueZPropertyParser.cpp)
target_link_libraries(tst_BlueZPropertyParser PRIVATE auralis-bluetooth)

auralis_add_test(tst_DeviceRegistry tst_DeviceRegistry.cpp)
target_link_libraries(tst_DeviceRegistry PRIVATE auralis-bluetooth)

auralis_add_test(tst_BluetoothDeviceListModel tst_BluetoothDeviceListModel.cpp)
target_link_libraries(tst_BluetoothDeviceListModel PRIVATE auralis-bluetooth)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_AdapterAndDiscovery tst_AdapterAndDiscovery.cpp)
    target_link_libraries(tst_AdapterAndDiscovery PRIVATE auralis-bluetooth)
    target_include_directories(tst_AdapterAndDiscovery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    auralis_add_test(tst_BluetoothDbusError tst_BluetoothDbusError.cpp)
    target_link_libraries(tst_BluetoothDbusError PRIVATE auralis-bluetooth)

    auralis_add_test(tst_BlueZAgent tst_BlueZAgent.cpp)
    target_link_libraries(tst_BlueZAgent PRIVATE auralis-bluetooth)
    target_include_directories(tst_BlueZAgent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    auralis_add_test(tst_DeviceLifecycle tst_DeviceLifecycle.cpp)
    target_link_libraries(tst_DeviceLifecycle PRIVATE auralis-bluetooth)
    target_include_directories(tst_DeviceLifecycle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

auralis_add_test(tst_ReconnectPolicy tst_ReconnectPolicy.cpp)
target_link_libraries(tst_ReconnectPolicy PRIVATE auralis-bluetooth)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    auralis_add_test(tst_BlueZDbusClientBusRecovery tst_BlueZDbusClientBusRecovery.cpp)
    target_link_libraries(tst_BlueZDbusClientBusRecovery PRIVATE auralis-bluetooth)
endif()
