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
