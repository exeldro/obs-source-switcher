project(source-switcher)

set(source-switcher_HEADERS
	source-switcher.h)
set(source-switcher_SOURCES
	source-switcher.c)

if(WIN32)
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
