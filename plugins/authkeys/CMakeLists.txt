include(BuildVeyonPlugin)

build_veyon_plugin(authkeys
	NAME AuthKeys
	DESCRIPTION "NextSpace Classroom Management - Authentication Keys Plugin"
	SOURCES
	AuthKeysPlugin.cpp
	AuthKeysConfigurationPage.cpp
	AuthKeysConfigurationPage.ui
	AuthKeysTableModel.cpp
	AuthKeysManager.cpp
	AuthKeysPlugin.h
	AuthKeysConfigurationPage.h
	AuthKeysTableModel.h
	AuthKeysManager.h
	)

# NextSpace action icons for the two Authentication Keys directory buttons (V3AM-099, V3bj)
target_link_libraries(authkeys PRIVATE nextspace-action-icons)
