if(WITH_TESTS AND VEYON_BUILD_LINUX)

	find_package(X11 REQUIRED)

	#
	# LinuxKeyboardMappingTest: tests keysym → linux keycode mapping tables
	# and UTF-8 → linux keycode conversion.
	#
	add_executable(LinuxKeyboardMappingTest
		LinuxKeyboardMappingTest.cpp
		${CMAKE_SOURCE_DIR}/plugins/platform/linux/LinuxKeyboardInput.cpp
		${CMAKE_SOURCE_DIR}/3rdparty/libfakekey/src/libfakekey.c
	)

	target_include_directories(LinuxKeyboardMappingTest PRIVATE
		${CMAKE_SOURCE_DIR}/plugins/platform/linux
		${CMAKE_SOURCE_DIR}/3rdparty/libfakekey
		${CMAKE_SOURCE_DIR}/core/src
		${CMAKE_SOURCE_DIR}/core/src/Configuration
	)

	target_link_libraries(LinuxKeyboardMappingTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Core
		veyon-core
		${X11_LIBRARIES}
		${X11_XTest_LIB}
	)

	add_test(NAME LinuxKeyboardMappingTest COMMAND LinuxKeyboardMappingTest)

	#
	# PipeWireVncServerTest: tests BGRx→RGB conversion, damage rect clamping,
	# and portal session token/path generation.
	#
	add_executable(PipeWireVncServerTest
		PipeWireVncServerTest.cpp
	)

	target_link_libraries(PipeWireVncServerTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Core
	)

	add_test(NAME PipeWireVncServerTest COMMAND PipeWireVncServerTest)

	add_executable(VncProtocolRobustnessTest
		VncProtocolRobustnessTest.cpp
	)

	target_link_libraries(VncProtocolRobustnessTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Core
		veyon-core
	)

	add_test(NAME VncProtocolRobustnessTest COMMAND VncProtocolRobustnessTest)

endif()

if(WITH_TESTS AND VEYON_BUILD_WINDOWS)

	#
	# WindowsFirewallRuleManagerTest: pure fake-store tests for exact ownership,
	# pair transactions, rollback and durable topology snapshots. No COM or host
	# firewall access is linked into this target.
	#
	add_executable(WindowsFirewallRuleManagerTest
		WindowsFirewallRuleManagerTest.cpp
		${CMAKE_SOURCE_DIR}/plugins/platform/windows/WindowsFirewallRuleManager.cpp
	)

	target_include_directories(WindowsFirewallRuleManagerTest PRIVATE
		${CMAKE_SOURCE_DIR}/plugins/platform/windows
	)

	target_link_libraries(WindowsFirewallRuleManagerTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Core
	)

	add_test(NAME WindowsFirewallRuleManagerTest COMMAND WindowsFirewallRuleManagerTest)

	#
	# TeacherActionIconsTest: NextSpace Teacher action-icon mapping and offscreen rendering
	# (run with QT_QPA_PLATFORM=offscreen; V3AN_CONTACT_SHEET_DIR optionally receives the contact sheets)
	#
	add_executable(TeacherActionIconsTest
		TeacherActionIconsTest.cpp
	)

	set_default_target_properties(TeacherActionIconsTest)

	target_link_libraries(TeacherActionIconsTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Gui
		veyon-core
		nextspace-action-icons
	)

	add_test(NAME TeacherActionIconsTest COMMAND TeacherActionIconsTest)

	#
	# TeacherConsoleUiConvergenceTest: V3cc receiving-palette and truthful
	# computer-card copy checks against the production delegate implementation.
	#
	add_executable(TeacherConsoleUiConvergenceTest
		TeacherConsoleUiConvergenceTest.cpp
		${CMAKE_SOURCE_DIR}/master/src/ComputerItemDelegate.cpp
		${CMAKE_SOURCE_DIR}/master/src/ComputerItemDelegate.h
	)
	set_target_properties(TeacherConsoleUiConvergenceTest PROPERTIES UNITY_BUILD OFF)

	target_include_directories(TeacherConsoleUiConvergenceTest PRIVATE
		${CMAKE_SOURCE_DIR}/master/src
	)

	target_link_libraries(TeacherConsoleUiConvergenceTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Widgets
		veyon-core
	)
	if(WIN32)
		add_dependencies(TeacherConsoleUiConvergenceTest windows-platform)
		if(VEYON_BUILD_WIN64)
			set(_teacher_test_interception_arch x64)
		else()
			set(_teacher_test_interception_arch x86)
		endif()
		add_custom_command(TARGET TeacherConsoleUiConvergenceTest POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${CMAKE_SOURCE_DIR}/../dependencies/Interception/Interception/library/${_teacher_test_interception_arch}/interception.dll
				$<TARGET_FILE_DIR:TeacherConsoleUiConvergenceTest>
		)
	elseif(UNIX)
		add_dependencies(TeacherConsoleUiConvergenceTest linux-platform)
	endif()

	add_test(NAME TeacherConsoleUiConvergenceTest COMMAND TeacherConsoleUiConvergenceTest)

endif()
