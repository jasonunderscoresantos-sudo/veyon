include(BuildVeyonPlugin)

build_veyon_plugin(builtindirectory
	NAME BuiltinDirectory
	DESCRIPTION "NextSpace Classroom Management - Built-in Directory Plugin"
	SOURCES
	BuiltinDirectoryPlugin.cpp
	BuiltinDirectoryConfigurationPage.cpp
	BuiltinDirectoryConfigurationPage.ui
	BuiltinDirectory.cpp
	BuiltinDirectoryPlugin.h
	BuiltinDirectoryConfiguration.h
	BuiltinDirectoryConfigurationPage.h
	BuiltinDirectory.h
	builtindirectory.qrc
	)

# NextSpace action icons for the eight Locations & computers add, remove and move buttons (V3AM-101, V3bk)
target_link_libraries(builtindirectory PRIVATE nextspace-action-icons)
