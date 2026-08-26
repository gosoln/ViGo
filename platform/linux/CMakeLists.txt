cmake_minimum_required(VERSION 3.25)

project(NAKLLinux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NAKL_BUILD_IBUS "Build the IBus engine" OFF)
option(NAKL_BUILD_FCITX5 "Build the Fcitx5 addon" OFF)

get_filename_component(NAKL_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(NOT NAKL_CORE_LIBRARY)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
       CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(NAKL_CORE_PROFILE "release")
    else()
        set(NAKL_CORE_PROFILE "debug")
    endif()
    set(NAKL_CORE_LIBRARY "${NAKL_REPOSITORY_DIR}/target/${NAKL_CORE_PROFILE}/libnakl_core.a")
endif()
if(NOT EXISTS "${NAKL_CORE_LIBRARY}")
    message(FATAL_ERROR
        "Rust core not found at ${NAKL_CORE_LIBRARY}. Build nakl-core for the matching profile first."
    )
endif()

add_library(nakl_linux_composition STATIC
    common/committed_edit.cpp
    common/composition_session.cpp
    common/settings.cpp
    common/surrounding_tracker.cpp
)
set_target_properties(nakl_linux_composition PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nakl_linux_composition
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/common"
    PRIVATE "${NAKL_REPOSITORY_DIR}/core/include"
)
target_link_libraries(nakl_linux_composition PUBLIC "${NAKL_CORE_LIBRARY}")
target_compile_options(nakl_linux_composition PRIVATE -Wall -Wextra -Werror)

if(NAKL_BUILD_IBUS)
    add_subdirectory(ibus)
endif()

if(NAKL_BUILD_FCITX5)
    add_subdirectory(fcitx5)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(nakl_linux_contract_tests tests/composition_session_tests.cpp)
    target_link_libraries(nakl_linux_contract_tests PRIVATE nakl_linux_composition)
    target_compile_options(nakl_linux_contract_tests PRIVATE -Wall -Wextra -Werror)
    add_test(NAME linux-composition-contract COMMAND nakl_linux_contract_tests)
endif()

if(NAKL_BUILD_IBUS OR NAKL_BUILD_FCITX5)
    include(GNUInstallDirs)
    if(NAKL_BUILD_IBUS)
        install(FILES "${NAKL_REPOSITORY_DIR}/LICENSE" "${NAKL_REPOSITORY_DIR}/NOTICE"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/nakl-ibus" COMPONENT ibus)
    endif()
    if(NAKL_BUILD_FCITX5)
        install(FILES "${NAKL_REPOSITORY_DIR}/LICENSE" "${NAKL_REPOSITORY_DIR}/NOTICE"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/nakl-fcitx5" COMPONENT fcitx5)
    endif()

    set(CPACK_PACKAGE_NAME "nakl")
    set(CPACK_PACKAGE_VENDOR "NAKL contributors")
    set(CPACK_PACKAGE_CONTACT "NAKL contributors")
    set(CPACK_PACKAGE_VERSION "2.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vietnamese Telex and VNI system input method")
    set(CPACK_RESOURCE_FILE_LICENSE "${NAKL_REPOSITORY_DIR}/LICENSE")
    set(CPACK_STRIP_FILES TRUE)
    set(NAKL_PACKAGE_COMPONENTS)
    if(NAKL_BUILD_IBUS)
        list(APPEND NAKL_PACKAGE_COMPONENTS ibus)
    endif()
    if(NAKL_BUILD_FCITX5)
        list(APPEND NAKL_PACKAGE_COMPONENTS fcitx5)
    endif()
    set(CPACK_COMPONENTS_ALL ${NAKL_PACKAGE_COMPONENTS})
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_IBUS_PACKAGE_NAME "nakl-ibus")
    set(CPACK_DEBIAN_IBUS_PACKAGE_DEPENDS "ibus (>= 1.5.29)")
    set(CPACK_DEBIAN_IBUS_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_NAME "nakl-fcitx5")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_DEPENDS "fcitx5 (>= 5.1.7)")
    set(CPACK_DEBIAN_FCITX5_PACKAGE_SECTION "utils")
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_IBUS_PACKAGE_NAME "nakl-ibus")
    set(CPACK_RPM_IBUS_PACKAGE_REQUIRES "ibus >= 1.5.29")
    set(CPACK_RPM_IBUS_PACKAGE_GROUP "Applications/System")
    set(CPACK_RPM_FCITX5_PACKAGE_NAME "nakl-fcitx5")
    set(CPACK_RPM_FCITX5_PACKAGE_REQUIRES "fcitx5 >= 5.1.7")
    set(CPACK_RPM_FCITX5_PACKAGE_GROUP "Applications/System")
    include(CPack)
endif()
