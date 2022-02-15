cmake_minimum_required(VERSION 3.12..4.0)

project(source-switcher VERSION 0.4.0)
set(PROJECT_FULL_NAME "Source Switcher")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

set(source-switcher_HEADERS
	source-switcher.h)
set(source-switcher_SOURCES
	source-switcher.c)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in source-switcher.rc)
	list(APPEND source-switcher_SOURCES
		source-switcher.rc)
endif()

if(BUILD_OUT_OF_TREE)
    find_package(LibObs REQUIRED)
endif()

add_library(source-switcher MODULE
	${source-switcher_HEADERS}
	${source-switcher_SOURCES})
target_link_libraries(source-switcher
	libobs
	obs-frontend-api)


if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/source-switcher")
    endif()
    set_target_properties(source-switcher PROPERTIES PREFIX "")
    install(TARGETS source-switcher
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(source-switcher PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(source-switcher PROPERTIES FOLDER "plugins/exeldro")
    install_obs_plugin_with_data(source-switcher data)
endif()
