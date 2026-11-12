include(BuildVeyonPlugin)

build_veyon_plugin(desktopservices
	NAME DesktopServices
	DESCRIPTION "NextSpace Classroom Management - Desktop Services Plugin"
	SOURCES
	DesktopServiceObject.cpp
	DesktopServicesConfigurationPage.cpp
	DesktopServicesConfigurationPage.ui
	DesktopServicesFeaturePlugin.cpp
	StartAppDialog.cpp
	StartAppDialog.h
	StartAppDialog.ui
	OpenWebsiteDialog.cpp
	OpenWebsiteDialog.h
	OpenWebsiteDialog.ui
	DesktopServiceObject.h
	DesktopServicesConfiguration.h
	DesktopServicesConfigurationPage.h
	DesktopServicesFeaturePlugin.h
	desktopservices.qrc
	)

# NextSpace action icons for the four Applications & websites add and remove buttons (V3AM-104, V3bl)
target_link_libraries(desktopservices PRIVATE nextspace-action-icons)
