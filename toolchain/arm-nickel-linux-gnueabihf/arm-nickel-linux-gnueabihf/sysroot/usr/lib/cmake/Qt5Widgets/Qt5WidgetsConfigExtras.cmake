
if (NOT TARGET Qt5::uic)
    add_executable(Qt5::uic IMPORTED)

    set(imported_location "${_qt5Widgets_install_prefix}/bin/uic")
    _qt5_Widgets_check_file_exists(${imported_location})

    set_target_properties(Qt5::uic PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

set(Qt5Widgets_UIC_EXECUTABLE Qt5::uic)
