cmake_minimum_required(VERSION 3.25)

project(NAKLWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(NAKL_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT NAKL_CORE_LIBRARY)
    if(MSVC)
        set(NAKL_CORE_LIBRARY "${NAKL_REPOSITORY_DIR}/target/debug/nakl_core.lib")
    elseif(WIN32)
        set(NAKL_CORE_LIBRARY "${NAKL_REPOSITORY_DIR}/target/x86_64-pc-windows-gnu/debug/libnakl_core.a")
    else()
        set(NAKL_CORE_LIBRARY "${NAKL_REPOSITORY_DIR}/target/debug/libnakl_core.a")
    endif()
endif()

if(NOT EXISTS "${NAKL_CORE_LIBRARY}")
    message(FATAL_ERROR
        "Rust core not found at ${NAKL_CORE_LIBRARY}. Run cargo build -p nakl-core first."
    )
endif()

add_library(nakl_windows_composition STATIC
    src/composition_session.cpp
)
target_include_directories(nakl_windows_composition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${NAKL_REPOSITORY_DIR}/core/include"
)
target_link_libraries(nakl_windows_composition PUBLIC "${NAKL_CORE_LIBRARY}")
if(WIN32)
    # Native libraries reported by Rust's MSVC staticlib runtime.
    target_link_libraries(nakl_windows_composition PUBLIC
        dbghelp
        kernel32
        ntdll
        userenv
        ws2_32
    )
endif()

if(MSVC)
    target_compile_options(nakl_windows_composition PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(nakl_windows_composition PRIVATE -Wall -Wextra -Werror)
endif()

if(WIN32)
    add_library(nakl_tsf SHARED
        src/class_factory.cpp
        src/dll_main.cpp
        src/edit_session.cpp
        src/module_state.cpp
        src/registration.cpp
        src/text_service.cpp
        resources/nakl_tsf.def
    )
    target_include_directories(nakl_tsf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(nakl_tsf PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(nakl_tsf PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(nakl_tsf PRIVATE -Wall -Wextra -Werror)
    endif()
    target_link_libraries(nakl_tsf PRIVATE
        nakl_windows_composition
        advapi32
        msctf
        ole32
        user32
        uuid
    )
    set_target_properties(nakl_tsf PROPERTIES PREFIX "" OUTPUT_NAME "NAKLTSF")
    install(TARGETS nakl_tsf RUNTIME DESTINATION bin LIBRARY DESTINATION bin)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(nakl_windows_contract_tests tests/composition_session_tests.cpp)
    target_link_libraries(nakl_windows_contract_tests PRIVATE nakl_windows_composition)
    if(MSVC)
        target_compile_options(nakl_windows_contract_tests PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(nakl_windows_contract_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME windows-composition-contract COMMAND nakl_windows_contract_tests)
endif()
