include(BuildVeyonApplication)

set(cli_SOURCES
	src/main.cpp
	src/ConfigCommands.cpp
	src/ConfigCommands.h
	src/FeatureCommands.cpp
	src/FeatureCommands.h
	src/PluginCommands.cpp
	src/PluginCommands.h
	src/ServiceControlCommands.cpp
	src/ServiceControlCommands.h
	src/ShellCommands.cpp
	src/ShellCommands.h
	)

build_veyon_application(veyon-cli
	NAME CLI
	DESCRIPTION "NextSpace Command Line Interface (console version)"
	CONSOLE
	NEXTSPACE_ROLE
	WINDOWS_ICON "\"../configurator/data/nextspace-classroom-administration.ico\""
	SOURCES ${cli_SOURCES})

target_compile_options(veyon-cli PRIVATE -Wno-date-time)

if(VEYON_BUILD_WINDOWS)
	build_veyon_application(veyon-wcli
		NAME WCLI
		DESCRIPTION "NextSpace Command Line Interface (non-console version)"
		NEXTSPACE_ROLE
		WINDOWS_ICON "\"../configurator/data/nextspace-classroom-administration.ico\""
		SOURCES ${cli_SOURCES})
endif()
