include(BuildVeyonPlugin)

build_veyon_plugin(powercontrol
	NAME PowerControl
	DESCRIPTION "NextSpace Classroom Management - Power Control Plugin"
	SOURCES
	PowerControlFeaturePlugin.cpp
	PowerControlFeaturePlugin.h
	PowerDownTimeInputDialog.cpp
	PowerDownTimeInputDialog.h
	PowerDownTimeInputDialog.ui
	powercontrol.qrc
	)
