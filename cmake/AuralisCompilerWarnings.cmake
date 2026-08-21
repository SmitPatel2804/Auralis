function(auralis_apply_warnings target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "auralis_apply_warnings: target '${target}' does not exist")
    endif()

    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4 /permissive-)

        if(AURALIS_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options("${target}" PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
        )

        if(AURALIS_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()
    endif()
endfunction()
