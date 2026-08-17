auralis_add_test(tst_ServiceStatus tst_ServiceStatus.cpp)
target_link_libraries(tst_ServiceStatus PRIVATE auralis-core)

auralis_add_test(tst_Logger tst_Logger.cpp)
target_link_libraries(tst_Logger PRIVATE auralis-core)

auralis_add_test(tst_ConfigurationManager tst_ConfigurationManager.cpp)
target_link_libraries(tst_ConfigurationManager PRIVATE auralis-core)

auralis_add_test(tst_ApplicationCore tst_ApplicationCore.cpp)
target_link_libraries(tst_ApplicationCore PRIVATE auralis-core)
target_include_directories(tst_ApplicationCore PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
