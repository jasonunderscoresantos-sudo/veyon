function(add_windows_resources TARGET)
	set(args_option REQUIRE_ADMINISTRATOR_PRIVILEGES CONSOLE NEXTSPACE_ROLE NEXTSPACE_COMPONENT)
	set(args_single NAME DESCRIPTION WINDOWS_ICON)
	set(args_multi)

	cmake_parse_arguments(PARSE_ARGV 1 arg
		"${args_option}"
		"${args_single}"
		"${args_multi}")

	if (NOT arg_NAME)
		message(FATAL_ERROR "No NAME given for target ${TARGET}")
	endif()

	if (NOT arg_DESCRIPTION)
		if (arg_NEXTSPACE_COMPONENT)
			message(FATAL_ERROR "No DESCRIPTION given for NextSpace component ${TARGET}")
		endif()
		set(arg_DESCRIPTION "Veyon ${arg_NAME}")
	endif()

	set(VERSIONINFO_COMMENTS "Virtual Eye On Networks (https://veyon.io)")
	set(VERSIONINFO_COMPANY "Veyon Solutions")
	set(VERSIONINFO_PRODUCT "Veyon")
	set(VERSIONINFO_PRODUCT_VERSION "${VERSION_STRING}")
	set(VERSIONINFO_PRODUCTVERSION_LINE "")
	if (arg_NEXTSPACE_ROLE OR arg_NEXTSPACE_COMPONENT)
		# NextSpace role executables and component DLLs: product/publisher identity and the public NextSpace
		# product version; FILEVERSION/FileVersion and the manifest stay on the upstream-compatible VERSION_* channel
		if (NOT NEXTSPACE_VERSION OR NOT NEXTSPACE_VERSION_NUMERIC)
			message(FATAL_ERROR "NextSpace public version is not defined for ${TARGET}")
		endif()
		set(VERSIONINFO_COMMENTS "NextSpace Classroom Management, based on Veyon (https://veyon.io)")
		set(VERSIONINFO_COMPANY "EduLearn Technologies Inc")
		set(VERSIONINFO_PRODUCT "NextSpace Classroom Management")
		set(VERSIONINFO_PRODUCT_VERSION "${NEXTSPACE_VERSION}")
		set(VERSIONINFO_PRODUCTVERSION_LINE "	PRODUCTVERSION ${NEXTSPACE_VERSION_MAJOR},${NEXTSPACE_VERSION_MINOR},${NEXTSPACE_VERSION_PATCH},${NEXTSPACE_VERSION_BUILD}\n")
	endif()

	if (arg_CONSOLE)
		set_target_properties(${TARGET} PROPERTIES LINK_FLAGS -mconsole)
	else()
		set_target_properties(${TARGET} PROPERTIES LINK_FLAGS -mwindows)
	endif()

	get_target_property(target_type ${TARGET} TYPE)
	if (target_type STREQUAL "EXECUTABLE")
		set(FILETYPE "VFT_APP")
		set(SUFFIX "exe")
	else()
		set(FILETYPE "VFT_DLL")
		set(SUFFIX "dll")
	endif()

	string(CONCAT RESOURCE_DATA
		"#include <windows.h>\n"
		"CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST ${TARGET}.${SUFFIX}.manifest\n"
		"VS_VERSION_INFO VERSIONINFO\n"
		"	FILEVERSION   ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},${VERSION_BUILD}\n"
		"${VERSIONINFO_PRODUCTVERSION_LINE}"
		"	FILEFLAGSMASK 0x0L\n"
		"	FILEOS        VOS_NT_WINDOWS32\n"
		"	FILETYPE      ${FILETYPE}\n"
		"	FILESUBTYPE   VFT2_UNKNOWN\n"
		"BEGIN\n"
		"	BLOCK \"StringFileInfo\"\n"
		"	BEGIN\n"
		"		BLOCK \"040904E4\"\n"
		"		BEGIN\n"
		"			VALUE \"Comments\",         \"${VERSIONINFO_COMMENTS}\\0\"\n"
		"			VALUE \"CompanyName\",      \"${VERSIONINFO_COMPANY}\\0\"\n"
		"			VALUE \"ProductName\",      \"${VERSIONINFO_PRODUCT}\\0\"\n"
		"			VALUE \"ProductVersion\",   \"${VERSIONINFO_PRODUCT_VERSION}\\0\"\n"
		"			VALUE \"FileDescription\",  \"${arg_DESCRIPTION}\\0\"\n"
		"			VALUE \"FileVersion\",      \"${VERSION_STRING}\\0\"\n"
		"			VALUE \"LegalCopyright\",   \"Copyright (c) 2017-2026 Veyon Solutions / Tobias Junghans\\0\"\n"
		"			VALUE \"OriginalFilename\", \"${TARGET}.${SUFFIX}\\0\"\n"
		"		END\n"
		"	END\n"
		"	BLOCK \"VarFileInfo\"\n"
		"	BEGIN\n"
		"		VALUE \"Translation\", 0x0409, 0x04E4\n"
		"	END\n"
		"END\n")
	if (arg_WINDOWS_ICON)
		string(APPEND RESOURCE_DATA "icon ICON ${arg_WINDOWS_ICON}\n")
	endif()

	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SUFFIX}.rc "${RESOURCE_DATA}")

	set(MANIFEST_TRUSTINFO "")
	if (arg_REQUIRE_ADMINISTRATOR_PRIVILEGES)
		string(CONCAT MANIFEST_TRUSTINFO "	<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">\n"
			"		<security>\n"
			"			<requestedPrivileges>\n"
			"				<requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"false\"/>\n"
			"			</requestedPrivileges>\n"
			"		</security>\n"
			"	</trustInfo>\n")
	endif()

	string(CONCAT MANIFEST_DATA "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
		"	<assemblyIdentity version=\"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_BUILD}\" name=\"Veyon.${arg_NAME}\" type=\"win32\"/>\n"
		"	<description>${arg_DESCRIPTION}</description>\n"
		"	<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">\n"
		"		<application>\n"
		"			<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>\n"
		"		</application>\n"
		"	</compatibility>\n"
		"${MANIFEST_TRUSTINFO}"
		"</assembly>")

	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SUFFIX}.manifest "${MANIFEST_DATA}")

	set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SUFFIX}.rc PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
	target_sources(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.${SUFFIX}.rc)
endfunction()
