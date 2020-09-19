project(source-switcher VERSION 0.2.6)
set(PROJECT_FULL_NAME "Source Switcher")

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

add_library(source-switcher MODULE
	${source-switcher_HEADERS}
	${source-switcher_SOURCES})
target_link_libraries(source-switcher
	libobs)

set_target_properties(source-switcher PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(source-switcher data)
