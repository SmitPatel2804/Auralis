add_library(auralis-audio
    PipeWireManager.cpp
)

add_library(Auralis::Audio ALIAS auralis-audio)

target_link_libraries(auralis-audio
    PUBLIC
        auralis-core
)

auralis_apply_warnings(auralis-audio)
