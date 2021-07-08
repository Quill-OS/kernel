


macro(_qt5gui_find_extra_libs Name Libs LibDir IncDirs)
    set(Qt5Gui_${Name}_LIBRARIES)
    set(Qt5Gui_${Name}_INCLUDE_DIRS ${IncDirs})
    foreach(_lib ${Libs})
        string(REGEX REPLACE [^_A-Za-z0-9] _ _cmake_lib_name ${_lib})
        if (NOT TARGET Qt5::Gui_${_cmake_lib_name})
            find_library(Qt5Gui_${_cmake_lib_name}_LIBRARY ${_lib}
            )
            if (NOT Qt5Gui_${_cmake_lib_name}_LIBRARY)
                if ("${ARGN}" STREQUAL "OPTIONAL")
                    break()
                else()
                    message(FATAL_ERROR "Failed to find \"${_lib}\" in \"${LibDir}\" with CMAKE_CXX_LIBRARY_ARCHITECTURE \"${CMAKE_CXX_LIBRARY_ARCHITECTURE}\".")
                endif()
            endif()
            add_library(Qt5::Gui_${_cmake_lib_name} SHARED IMPORTED)
            set_property(TARGET Qt5::Gui_${_cmake_lib_name} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Qt5Gui_${Name}_INCLUDE_DIRS})

            set_property(TARGET Qt5::Gui_${_cmake_lib_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
            _qt5_Gui_check_file_exists("${Qt5Gui_${_cmake_lib_name}_LIBRARY}")
            set_property(TARGET Qt5::Gui_${_cmake_lib_name} PROPERTY IMPORTED_LOCATION_RELEASE "${Qt5Gui_${_cmake_lib_name}_LIBRARY}")

            unset(Qt5Gui_${_cmake_lib_name}_LIBRARY CACHE)

            find_library(Qt5Gui_${_cmake_lib_name}_LIBRARY_DEBUG ${_lib}d
                PATHS "${LibDir}"
                NO_DEFAULT_PATH
            )
            if (Qt5Gui_${_cmake_lib_name}_LIBRARY_DEBUG)
                set_property(TARGET Qt5::Gui_${_cmake_lib_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
                _qt5_Gui_check_file_exists("${Qt5Gui_${_cmake_lib_name}_LIBRARY_DEBUG}")
                set_property(TARGET Qt5::Gui_${_cmake_lib_name} PROPERTY IMPORTED_LOCATION_DEBUG "${Qt5Gui_${_cmake_lib_name}_LIBRARY_DEBUG}")
            endif()
            unset(Qt5Gui_${_cmake_lib_name}_LIBRARY_DEBUG CACHE)
        endif()
        list(APPEND Qt5Gui_${Name}_LIBRARIES Qt5::Gui_${_cmake_lib_name})
    endforeach()
    if (NOT CMAKE_CROSSCOMPILING)
        foreach(_dir ${Qt5Gui_${Name}_INCLUDE_DIRS})
            _qt5_Gui_check_file_exists(${_dir})
        endforeach()
    endif()
endmacro()



_qt5gui_find_extra_libs(OPENGL "GL" "" "")



set(Qt5Gui_OPENGL_IMPLEMENTATION GL)

get_target_property(_configs Qt5::Gui IMPORTED_CONFIGURATIONS)
foreach(_config ${_configs})
    set_property(TARGET Qt5::Gui APPEND PROPERTY
        IMPORTED_LINK_DEPENDENT_LIBRARIES_${_config}
        ${Qt5Gui_EGL_LIBRARIES} ${Qt5Gui_OPENGL_LIBRARIES}
    )
endforeach()
unset(_configs)
