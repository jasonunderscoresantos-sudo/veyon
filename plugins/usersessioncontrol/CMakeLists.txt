include(BuildVeyonPlugin)

build_veyon_plugin(usersessioncontrol
	NAME UserSessionControl
	DESCRIPTION "NextSpace Classroom Management - User Session Control Plugin"
	SOURCES
	UserSessionControlPlugin.cpp
	UserSessionControlPlugin.h
	UserLoginDialog.cpp
	UserLoginDialog.h
	UserLoginDialog.ui
	usersessioncontrol.qrc
	)
