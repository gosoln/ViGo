cmake_minimum_required(VERSION 3.25)

project(ViGoWindows LANGUAGES CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # Rust's x86_64-pc-windows-msvc staticlib requests the dynamic release CRT
    # even for Cargo debug builds. Keep allocator/runtime ownership consistent.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

get_filename_component(VIGO_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT VIGO_CORE_LIBRARY)
    if(MSVC)
        set(VIGO_CORE_LIBRARY "${VIGO_REPOSITORY_DIR}/target/debug/vigo_core.lib")
    elseif(WIN32)
        set(VIGO_CORE_LIBRARY "${VIGO_REPOSITORY_DIR}/target/x86_64-pc-windows-gnu/debug/libvigo_core.a")
    else()
        set(VIGO_CORE_LIBRARY "${VIGO_REPOSITORY_DIR}/target/debug/libvigo_core.a")
    endif()
endif()

if(NOT EXISTS "${VIGO_CORE_LIBRARY}")
    message(FATAL_ERROR
        "Rust core not found at ${VIGO_CORE_LIBRARY}. Run cargo build -p vigo-core first."
    )
endif()

add_library(vigo_windows_composition STATIC
    src/composition_session.cpp
)
target_include_directories(vigo_windows_composition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${VIGO_REPOSITORY_DIR}/core/include"
)
target_link_libraries(vigo_windows_composition PUBLIC "${VIGO_CORE_LIBRARY}")
if(WIN32)
    # Native libraries reported by Rust's MSVC staticlib runtime.
    target_link_libraries(vigo_windows_composition PUBLIC
        dbghelp
        kernel32
        ntdll
        userenv
        ws2_32
    )
endif()

if(MSVC)
    target_compile_options(vigo_windows_composition PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
else()
    target_compile_options(vigo_windows_composition PRIVATE -Wall -Wextra -Werror)
endif()

if(WIN32)
    add_library(vigo_windows_settings STATIC src/settings.cpp)
    target_include_directories(vigo_windows_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(vigo_windows_settings PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(vigo_windows_settings PUBLIC advapi32 ktmw32)

    add_library(vigo_tsf SHARED
        src/class_factory.cpp
        src/dll_main.cpp
        src/edit_session.cpp
        src/module_state.cpp
        src/registration.cpp
        src/text_service.cpp
        resources/vigo_tsf.def
        resources/vigo.rc
    )
    target_include_directories(vigo_tsf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(vigo_tsf PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(vigo_tsf PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(vigo_tsf PRIVATE -Wall -Wextra -Werror)
    endif()
    target_link_libraries(vigo_tsf PRIVATE
        vigo_windows_composition
        vigo_windows_settings
        advapi32
        ole32
        oleaut32
        user32
        uuid
    )
    set_target_properties(vigo_tsf PROPERTIES PREFIX "" OUTPUT_NAME "ViGoTSF")
    install(TARGETS vigo_tsf RUNTIME DESTINATION bin LIBRARY DESTINATION bin)

    add_executable(vigo_settings WIN32 src/settings_app.cpp resources/vigo.rc)
    target_include_directories(vigo_settings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(vigo_settings PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(vigo_settings PRIVATE vigo_windows_settings comctl32 shell32 user32)
    set_target_properties(vigo_settings PROPERTIES OUTPUT_NAME "ViGoSettings")
    install(TARGETS vigo_settings RUNTIME DESTINATION bin)
endif()

if(MSVC)
    target_compile_options(vigo_windows_settings PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    target_compile_options(vigo_tsf PRIVATE /utf-8)
    target_compile_options(vigo_settings PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
elseif(WIN32)
    target_compile_options(vigo_windows_settings PRIVATE -Wall -Wextra -Werror)
    target_compile_options(vigo_settings PRIVATE -Wall -Wextra -Werror)
    if(MINGW)
        target_link_options(vigo_settings PRIVATE -municode)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(vigo_windows_contract_tests tests/composition_session_tests.cpp)
    target_link_libraries(vigo_windows_contract_tests PRIVATE vigo_windows_composition)
    if(MSVC)
        target_compile_options(vigo_windows_contract_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    else()
        target_compile_options(vigo_windows_contract_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME windows-composition-contract COMMAND vigo_windows_contract_tests)
endif()
