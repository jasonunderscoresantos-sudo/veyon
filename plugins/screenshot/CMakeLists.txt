include(BuildVeyonPlugin)

build_veyon_plugin(screenshot
	NAME Screenshot
	DESCRIPTION "NextSpace Classroom Management - Screenshot Plugin"
	SOURCES
	ScreenshotFeaturePlugin.cpp
	ScreenshotFeaturePlugin.h
	screenshot.qrc
	)
