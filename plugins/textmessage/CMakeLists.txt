include(BuildVeyonPlugin)

build_veyon_plugin(textmessage
	NAME TextMessage
	DESCRIPTION "NextSpace Classroom Management - Text Message Plugin"
	SOURCES
	TextMessageFeaturePlugin.cpp
	TextMessageDialog.cpp
	TextMessageDialog.ui
	TextMessageFeaturePlugin.h
	TextMessageDialog.h
	textmessage.qrc
	)
