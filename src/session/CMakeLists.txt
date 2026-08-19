add_library(auralis-session
    AuralisSession.cpp
    RoutingCoordinator.cpp
    SessionManager.cpp
    SessionListModel.cpp
    SessionPersistence.cpp
    SessionStateMachine.cpp
    SessionTypes.cpp
    VolumeCoordinator.cpp
    "${CMAKE_SOURCE_DIR}/include/auralis/session/AuralisSession.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/ISessionManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/RoutingCoordinator.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/SessionManager.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/SessionListModel.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/SessionPersistence.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/SessionStateMachine.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/SessionTypes.h"
    "${CMAKE_SOURCE_DIR}/include/auralis/session/VolumeCoordinator.h"
)

add_library(Auralis::Session ALIAS auralis-session)

target_link_libraries(auralis-session
    PUBLIC
        auralis-core
        auralis-audio
        auralis-bluetooth
        Qt6::Core
)

auralis_apply_warnings(auralis-session)
