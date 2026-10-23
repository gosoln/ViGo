cmake_minimum_required(VERSION 3.25)

project(ViGoLinux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VIGO_BUILD_IBUS "Build the IBus engine" OFF)
option(VIGO_BUILD_FCITX5 "Build the Fcitx5 addon" OFF)

get_filename_component(VIGO_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(NOT VIGO_CORE_LIBRARY)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
       CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(VIGO_CORE_PROFILE "release")
    else()
        set(VIGO_CORE_PROFILE "debug")
    endif()
    set(VIGO_CORE_LIBRARY "${VIGO_REPOSITORY_DIR}/target/${VIGO_CORE_PROFILE}/libvigo_core.a")
endif()
if(NOT EXISTS "${VIGO_CORE_LIBRARY}")
    message(FATAL_ERROR
        "Rust core not found at ${VIGO_CORE_LIBRARY}. Build vigo-core for the matching profile first."
    )
endif()

add_library(vigo_linux_composition STATIC
    common/committed_edit.cpp
    common/composition_session.cpp
    common/settings.cpp
    common/surrounding_tracker.cpp
)
set_target_properties(vigo_linux_composition PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(vigo_linux_composition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/common"
    PRIVATE "${VIGO_REPOSITORY_DIR}/core/include"
)
target_link_libraries(vigo_linux_composition PUBLIC "${VIGO_CORE_LIBRARY}")
target_compile_options(vigo_linux_composition PRIVATE -Wall -Wextra -Werror)

if(VIGO_BUILD_IBUS)
    add_subdirectory(ibus)
endif()

if(VIGO_BUILD_FCITX5)
    add_subdirectory(fcitx5)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(vigo_linux_contract_tests tests/composition_session_tests.cpp)
    target_link_libraries(vigo_linux_contract_tests PRIVATE vigo_linux_composition)
    target_compile_options(vigo_linux_contract_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME linux-composition-contract COMMAND vigo_linux_contract_tests)
endif()

if(VIGO_BUILD_IBUS OR VIGO_BUILD_FCITX5)
    include(GNUInstallDirs)
    if(VIGO_BUILD_IBUS)
        install(FILES "${VIGO_REPOSITORY_DIR}/LICENSE" "${VIGO_REPOSITORY_DIR}/NOTICE"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/vigo-ibus" COMPONENT ibus)
    endif()
    if(VIGO_BUILD_FCITX5)
        install(FILES "${VIGO_REPOSITORY_DIR}/LICENSE" "${VIGO_REPOSITORY_DIR}/NOTICE"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/vigo-fcitx5" COMPONENT fcitx5)
    endif()

    set(CPACK_PACKAGE_NAME "vigo")
    set(CPACK_PACKAGE_VENDOR "ViGo contributors")
    set(CPACK_PACKAGE_CONTACT "ViGo contributors")
    set(CPACK_PACKAGE_VERSION "2.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vietnamese Telex and VNI system input method")
    set(CPACK_RESOURCE_FILE_LICENSE "${VIGO_REPOSITORY_DIR}/LICENSE")
    set(CPACK_STRIP_FILES TRUE)
    set(VIGO_PACKAGE_COMPONENTS)
    if(VIGO_BUILD_IBUS)
        list(APPEND VIGO_PACKAGE_COMPONENTS ibus)
    endif()
    if(VIGO_BUILD_FCITX5)
        list(APPEND VIGO_PACKAGE_COMPONENTS fcitx5)
    endif()
    set(CPACK_COMPONENTS_ALL ${VIGO_PACKAGE_COMPONENTS})
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_IBUS_PACKAGE_NAME "vigo-ibus")
    set(CPACK_DEBIAN_IBUS_PACKAGE_DEPENDS "ibus (>= 1.5.29)")
    set(CPACK_DEBIAN_IBUS_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_NAME "vigo-fcitx5")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_DEPENDS "fcitx5 (>= 5.1.7)")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_SECTION "utils")
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_IBUS_PACKAGE_NAME "vigo-ibus")
    set(CPACK_RPM_IBUS_PACKAGE_REQUIRES "ibus >= 1.5.29")
    set(CPACK_RPM_IBUS_PACKAGE_GROUP "Applications/System")
    set(CPACK_RPM_FCITX5_PACKAGE_NAME "vigo-fcitx5")
    set(CPACK_RPM_FCITX5_PACKAGE_REQUIRES "fcitx5 >= 5.1.7")
    set(CPACK_RPM_FCITX5_PACKAGE_GROUP "Applications/System")
    include(CPack)
endif()
