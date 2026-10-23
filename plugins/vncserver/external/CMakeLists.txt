include(BuildVeyonPlugin)

build_veyon_plugin(external-vnc-server
	NAME ExternalVncServer
	DESCRIPTION "NextSpace Classroom Management - External VNC Server Plugin"
	SOURCES
	ExternalVncServer.cpp
	ExternalVncServerConfigurationWidget.cpp
	ExternalVncServerConfigurationWidget.ui
	ExternalVncServer.h
	ExternalVncServerConfiguration.h
	ExternalVncServerConfigurationWidget.h
	)
