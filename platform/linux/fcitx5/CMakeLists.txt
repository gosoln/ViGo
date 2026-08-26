find_package(Fcitx5Core 5.1.7 REQUIRED)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
include(GNUInstallDirs)

add_library(nakl_fcitx5 SHARED engine.cpp)
target_link_libraries(nakl_fcitx5 PRIVATE nakl_linux_composition Fcitx5::Core)
target_compile_options(nakl_fcitx5 PRIVATE -Wall -Wextra -Werror)
set_target_properties(nakl_fcitx5 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "nakl"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
install(TARGETS nakl_fcitx5 DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5" COMPONENT fcitx5)

configure_file(nakl-addon.conf.in nakl-addon.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nakl-addon.conf"
        RENAME nakl.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" COMPONENT fcitx5)
install(FILES nakl-telex.conf nakl-vni.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod" COMPONENT fcitx5)
install(FILES org.nakl.NAKL.Fcitx5.metainfo.xml
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo" COMPONENT fcitx5)
