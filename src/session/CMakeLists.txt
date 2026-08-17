add_library(auralis-session
    SessionManager.cpp
)

add_library(Auralis::Session ALIAS auralis-session)

target_link_libraries(auralis-session
    PUBLIC
        auralis-core
)

auralis_apply_warnings(auralis-session)
