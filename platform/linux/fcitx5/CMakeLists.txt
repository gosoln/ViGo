find_package(Fcitx5Core 5.1.7 REQUIRED)
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
include(GNUInstallDirs)

add_library(vigo_fcitx5 SHARED engine.cpp)
target_link_libraries(vigo_fcitx5 PRIVATE vigo_linux_composition Fcitx5::Core)
target_compile_options(vigo_fcitx5 PRIVATE -Wall -Wextra -Werror)
set_target_properties(vigo_fcitx5 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "vigo"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
install(TARGETS vigo_fcitx5 DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5" COMPONENT fcitx5)

configure_file(vigo-addon.conf.in vigo-addon.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vigo-addon.conf"
        RENAME vigo.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" COMPONENT fcitx5)
install(FILES vigo-telex.conf vigo-vni.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod" COMPONENT fcitx5)
install(FILES com.gosoln.ViGo.Fcitx5.metainfo.xml
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo" COMPONENT fcitx5)
