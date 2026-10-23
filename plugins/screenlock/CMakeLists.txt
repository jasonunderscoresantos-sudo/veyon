include(BuildVeyonPlugin)

build_veyon_plugin(screenlock
	NAME ScreenLock
	DESCRIPTION "NextSpace Classroom Management - Screen Lock Plugin"
	SOURCES
	ScreenLockFeaturePlugin.cpp ScreenLockFeaturePlugin.h screenlock.qrc)
