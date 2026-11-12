# NextSpace action icons: the existing palette-aware Tabler renderer (master/src), compiled once into a non-installed,
# position-independent static library behind the neutral public header NextSpaceActionIcons.h; linked by the Teacher
# Console, the file-transfer and Remote Access plugins and TeacherActionIconsTest. Nothing here is installed.
add_library(nextspace-action-icons STATIC
	${CMAKE_SOURCE_DIR}/master/src/TeacherActionIcons.cpp
	${CMAKE_SOURCE_DIR}/master/src/TeacherActionIcons.h
	${CMAKE_SOURCE_DIR}/master/src/TeacherActionIconData.h)
target_include_directories(nextspace-action-icons PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/master/src)
target_link_libraries(nextspace-action-icons PUBLIC Qt${QT_MAJOR_VERSION}::Gui PRIVATE veyon-core)
set_default_target_properties(nextspace-action-icons)
set_target_properties(nextspace-action-icons PROPERTIES POSITION_INDEPENDENT_CODE ON)
