option(AURALIS_BUILD_TESTS "Build Auralis tests" ON)
option(AURALIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AURALIS_ENABLE_FILE_LOGGING "Enable file logging support" ON)
option(AURALIS_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(AURALIS_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
