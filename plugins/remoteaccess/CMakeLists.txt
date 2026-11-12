include(BuildVeyonPlugin)

build_veyon_plugin(remoteaccess
	NAME RemoteAccess
	DESCRIPTION "NextSpace Classroom Management - Remote Access Plugin"
	SOURCES
	RemoteAccessFeaturePlugin.h
	RemoteAccessFeaturePlugin.cpp
	RemoteAccessWidget.h
	RemoteAccessWidget.cpp
	remoteaccess.qrc)

# Teacher action icons for the Remote Access toolbar controls (V3AM-064 to V3AM-069)
target_link_libraries(remoteaccess PRIVATE nextspace-action-icons)
