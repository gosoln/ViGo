find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED IMPORTED_TARGET ibus-1.0>=1.5.29)

add_executable(ibus-engine-vigo engine.cpp main.cpp)
target_link_libraries(ibus-engine-vigo PRIVATE vigo_linux_composition PkgConfig::IBUS)
target_compile_options(ibus-engine-vigo PRIVATE -Wall -Wextra -Werror)

include(GNUInstallDirs)
install(TARGETS ibus-engine-vigo RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
        COMPONENT ibus)
configure_file(vigo.xml.in vigo.xml @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vigo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ibus/component" COMPONENT ibus)
