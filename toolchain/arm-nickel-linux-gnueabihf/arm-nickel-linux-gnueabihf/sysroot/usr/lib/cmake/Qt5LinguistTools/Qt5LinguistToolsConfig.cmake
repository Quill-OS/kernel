
if (CMAKE_VERSION VERSION_LESS 2.8.3)
    message(FATAL_ERROR "Qt 5 requires at least CMake version 2.8.3")
endif()

get_filename_component(_qt5_linguisttools_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

if (NOT TARGET Qt5::lrelease)
    add_executable(Qt5::lrelease IMPORTED)

    set_target_properties(Qt5::lrelease PROPERTIES
        IMPORTED_LOCATION "${_qt5_linguisttools_install_prefix}/bin/lrelease"
    )
endif()

if (NOT TARGET Qt5::lupdate)
    add_executable(Qt5::lupdate IMPORTED)

    set_target_properties(Qt5::lupdate PROPERTIES
        IMPORTED_LOCATION "${_qt5_linguisttools_install_prefix}/bin/lupdate"
    )
endif()

set(Qt5_LRELEASE_EXECUTABLE Qt5::lrelease)
set(Qt5_LUPDATE_EXECUTABLE Qt5::lupdate)

include("${CMAKE_CURRENT_LIST_DIR}/Qt5LinguistToolsMacros.cmake")
