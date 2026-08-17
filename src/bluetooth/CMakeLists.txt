add_library(auralis-bluetooth
    BluetoothManager.cpp
)

add_library(Auralis::Bluetooth ALIAS auralis-bluetooth)

target_link_libraries(auralis-bluetooth
    PUBLIC
        auralis-core
)

auralis_apply_warnings(auralis-bluetooth)
