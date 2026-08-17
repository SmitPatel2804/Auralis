add_library(auralis-devices
    DeviceManager.cpp
)

add_library(Auralis::Devices ALIAS auralis-devices)

target_link_libraries(auralis-devices
    PUBLIC
        auralis-core
)

auralis_apply_warnings(auralis-devices)
