set(WINDOWS_INSTALL_FILES "veyon-${VEYON_WINDOWS_ARCH}-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_BUILD}")

set(NEXTSPACE_INTERCEPTION_PAYLOAD_DIR "${CMAKE_SOURCE_DIR}/3rdparty/interception")
set(NEXTSPACE_INTERCEPTION_HELPER_SHA256 "98B4B384BC2C84A9E9F9275352B265DD1B351C4780598D7D1AA7D32B06F5BA86")
set(NEXTSPACE_INTERCEPTION_INSTALL_BAT_SHA256 "77048165377633E8CB5FDBD62C313493BD19E4CDCB0F9CF5BDAEDB88D7BCD105")
set(NEXTSPACE_INTERCEPTION_UNINSTALL_BAT_SHA256 "3CA834971CE2364F53FEF24582C368BD14ECC5F4D8B620FFB04332C2BFC11FA8")

file(GLOB NEXTSPACE_INTERCEPTION_PAYLOAD_MEMBERS
	LIST_DIRECTORIES false
	RELATIVE "${NEXTSPACE_INTERCEPTION_PAYLOAD_DIR}"
	"${NEXTSPACE_INTERCEPTION_PAYLOAD_DIR}/*"
)
list(SORT NEXTSPACE_INTERCEPTION_PAYLOAD_MEMBERS)
set(NEXTSPACE_INTERCEPTION_EXPECTED_MEMBERS
	"install-interception.exe;install.bat;uninstall.bat"
)
if(NOT NEXTSPACE_INTERCEPTION_PAYLOAD_MEMBERS STREQUAL NEXTSPACE_INTERCEPTION_EXPECTED_MEMBERS)
	message(FATAL_ERROR
		"GC3-S3: canonical Interception directory must contain exactly "
		"${NEXTSPACE_INTERCEPTION_EXPECTED_MEMBERS}; found ${NEXTSPACE_INTERCEPTION_PAYLOAD_MEMBERS}"
	)
endif()

file(REAL_PATH "${NEXTSPACE_INTERCEPTION_PAYLOAD_DIR}" NEXTSPACE_INTERCEPTION_PAYLOAD_REAL_DIR)
function(nextspace_verify_interception_payload filename expected_size expected_hash)
	set(payload "${NEXTSPACE_INTERCEPTION_PAYLOAD_DIR}/${filename}")
	if(NOT EXISTS "${payload}" OR IS_DIRECTORY "${payload}")
		message(FATAL_ERROR "GC3-S3: missing canonical Interception payload ${payload}")
	endif()
	if(IS_SYMLINK "${payload}")
		message(FATAL_ERROR "GC3-S3: canonical Interception payload may not be a symlink: ${payload}")
	endif()
	file(REAL_PATH "${payload}" payload_real)
	string(FIND "${payload_real}" "${NEXTSPACE_INTERCEPTION_PAYLOAD_REAL_DIR}/" payload_prefix)
	if(NOT payload_prefix EQUAL 0)
		message(FATAL_ERROR "GC3-S3: canonical Interception payload escaped its source directory: ${payload_real}")
	endif()
	file(SIZE "${payload}" payload_size)
	if(NOT payload_size EQUAL expected_size)
		message(FATAL_ERROR
			"GC3-S3: ${filename} size mismatch: expected ${expected_size}, got ${payload_size}"
		)
	endif()
	file(SHA256 "${payload}" payload_hash)
	string(TOUPPER "${payload_hash}" payload_hash)
	if(NOT payload_hash STREQUAL expected_hash)
		message(FATAL_ERROR
			"GC3-S3: ${filename} SHA-256 mismatch: expected ${expected_hash}, got ${payload_hash}"
		)
	endif()
endfunction()

nextspace_verify_interception_payload(
	"install-interception.exe" 470528 "${NEXTSPACE_INTERCEPTION_HELPER_SHA256}"
)
nextspace_verify_interception_payload(
	"install.bat" 150 "${NEXTSPACE_INTERCEPTION_INSTALL_BAT_SHA256}"
)
nextspace_verify_interception_payload(
	"uninstall.bat" 53 "${NEXTSPACE_INTERCEPTION_UNINSTALL_BAT_SHA256}"
)

set(NEXTSPACE_INTERCEPTION_PAYLOAD_MANIFEST
	"${CMAKE_BINARY_DIR}/nextspace-interception-payload.tsv"
)
file(WRITE "${NEXTSPACE_INTERCEPTION_PAYLOAD_MANIFEST}"
	"# NextSpace Interception canonical payload v1.0.0\n"
	"install-interception.exe\t470528\t${NEXTSPACE_INTERCEPTION_HELPER_SHA256}\n"
	"install.bat\t150\t${NEXTSPACE_INTERCEPTION_INSTALL_BAT_SHA256}\n"
	"uninstall.bat\t53\t${NEXTSPACE_INTERCEPTION_UNINSTALL_BAT_SHA256}\n"
)

if(CMAKE_HOST_WIN32 AND NOT CMAKE_CROSSCOMPILING)
	# NextSpace native Windows (MSYS2 UCRT64) build: stage through an auditable script-mode helper using the
	# CMake-resolved toolchain, Qt and SDK paths; the Linux cross-prefix recipe below stays the cross branch
	if(VEYON_BUILD_WIN64)
		set(DLL_DDENGINE "ddengine64.dll")
		set(NEXTSPACE_INTERCEPTION_ARCH "x64")
	else()
		set(DLL_DDENGINE "ddengine.dll")
		set(NEXTSPACE_INTERCEPTION_ARCH "x86")
	endif()
	get_filename_component(NEXTSPACE_TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
	add_custom_target(windows-binaries
		COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
		COMMAND ${CMAKE_COMMAND}
			-DSOURCE_DIR=${CMAKE_SOURCE_DIR}
			-DBINARY_DIR=${CMAKE_BINARY_DIR}
			-DSTAGE_NAME=${WINDOWS_INSTALL_FILES}
			-DWINDOWS_ARCH=${VEYON_WINDOWS_ARCH}
			-DDDENGINE_DLL=${CMAKE_SOURCE_DIR}/3rdparty/ddengine/${DLL_DDENGINE}
			-DINTERCEPTION_DIR=${CMAKE_SOURCE_DIR}/../dependencies/Interception/Interception/library/${NEXTSPACE_INTERCEPTION_ARCH}
			-DTOOLCHAIN_BIN_DIR=${NEXTSPACE_TOOLCHAIN_BIN_DIR}
			-DQT_PLUGIN_DIR=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}
			-DOBJDUMP=${CMAKE_OBJDUMP}
			-DSTRIP=${CMAKE_STRIP}
			-DWITH_TRANSLATIONS=${WITH_TRANSLATIONS}
			-DWITH_WEBAPI=${WITH_WEBAPI}
			-P ${CMAKE_CURRENT_LIST_DIR}/WindowsNativeStaging.cmake
		VERBATIM
	)
else()
set(DLLDIR "${MINGW_PREFIX}/bin")
set(DLLDIR_LIB "${MINGW_PREFIX}/lib")
string(REGEX MATCH "^[^.]+" GCC_VERSION_MAJOR ${CMAKE_CXX_COMPILER_VERSION})
set(DLLDIR_GCC "/usr/lib/gcc/${MINGW_TARGET}/${GCC_VERSION_MAJOR}-posix")
if(VEYON_BUILD_WIN64)
	set(DLL_GCC "libgcc_s_seh-1.dll")
	set(DLL_DDENGINE "ddengine64.dll")
else()
	set(DLL_GCC "libgcc_s_dw2-1.dll")
	set(DLL_DDENGINE "ddengine.dll")
endif()

add_custom_target(windows-binaries
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIGURATION>
	COMMAND rm -rf ${WINDOWS_INSTALL_FILES}*
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/interception
	COMMAND cp ${CMAKE_SOURCE_DIR}/3rdparty/interception/install-interception.exe ${CMAKE_SOURCE_DIR}/3rdparty/interception/install.bat ${CMAKE_SOURCE_DIR}/3rdparty/interception/uninstall.bat ${WINDOWS_INSTALL_FILES}/interception
	COMMAND cp ${NEXTSPACE_INTERCEPTION_PAYLOAD_MANIFEST} ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${CMAKE_SOURCE_DIR}/3rdparty/ddengine/${DLL_DDENGINE} ${WINDOWS_INSTALL_FILES}
	COMMAND cp core/veyon-core.dll ${WINDOWS_INSTALL_FILES}
	COMMAND find . -mindepth 2 -name 'veyon-*.exe' -exec cp '{}' ${WINDOWS_INSTALL_FILES}/ '\;'
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/plugins
	COMMAND find plugins/ -name '*.dll' -exec cp '{}' ${WINDOWS_INSTALL_FILES}/plugins/ '\;'
	COMMAND mv ${WINDOWS_INSTALL_FILES}/plugins/lib*.dll ${WINDOWS_INSTALL_FILES}
	COMMAND mv ${WINDOWS_INSTALL_FILES}/plugins/vnchooks.dll ${WINDOWS_INSTALL_FILES}
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/translations
	COMMAND cp translations/*qm ${WINDOWS_INSTALL_FILES}/translations/
	COMMAND cp ${DLLDIR}/libjpeg-62.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libpng16-16.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libcrypto-3*.dll ${DLLDIR}/libssl-3*.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libqca-qt6.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libsasl2-3.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libldap.dll ${DLLDIR}/liblber.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/interception.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/liblzo2-2.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libvncclient.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR}/libvncserver.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR_LIB}/zlib1.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR_LIB}/libwinpthread-1.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR_GCC}/libstdc++-6.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR_GCC}/libssp-0.dll ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${DLLDIR_GCC}/${DLL_GCC} ${WINDOWS_INSTALL_FILES}
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/crypto
	COMMAND cp ${DLLDIR_LIB}/qca-qt6/crypto/libqca-ossl.dll ${WINDOWS_INSTALL_FILES}/crypto
	COMMAND cp ${DLLDIR}/Qt6Core.dll
				${DLLDIR}/Qt6Core5Compat.dll
				${DLLDIR}/Qt6Gui.dll
				${DLLDIR}/Qt6Widgets.dll
				${DLLDIR}/Qt6Network.dll
				${DLLDIR}/Qt6Concurrent.dll
				${DLLDIR}/Qt6HttpServer.dll
				${DLLDIR}/Qt6WebSockets.dll
			${WINDOWS_INSTALL_FILES}
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/imageformats
	COMMAND cp ${MINGW_PREFIX}/plugins/imageformats/qjpeg.dll ${WINDOWS_INSTALL_FILES}/imageformats
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/platforms
	COMMAND cp ${MINGW_PREFIX}/plugins/platforms/qwindows.dll ${WINDOWS_INSTALL_FILES}/platforms
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/styles
	COMMAND cp ${MINGW_PREFIX}/plugins/styles/*.dll ${WINDOWS_INSTALL_FILES}/styles
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/tls
	COMMAND cp ${MINGW_PREFIX}/plugins/tls/qopensslbackend.dll ${WINDOWS_INSTALL_FILES}/tls
	COMMAND ${MINGW_TOOL_PREFIX}strip ${WINDOWS_INSTALL_FILES}/*.dll ${WINDOWS_INSTALL_FILES}/*.exe ${WINDOWS_INSTALL_FILES}/plugins/*.dll ${WINDOWS_INSTALL_FILES}/platforms/*.dll ${WINDOWS_INSTALL_FILES}/styles/*.dll ${WINDOWS_INSTALL_FILES}/crypto/*.dll
	COMMAND cp ${CMAKE_SOURCE_DIR}/COPYING ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${CMAKE_SOURCE_DIR}/COPYING ${WINDOWS_INSTALL_FILES}/LICENSE.TXT
	COMMAND cp ${CMAKE_SOURCE_DIR}/nsis/README-nextspace.txt ${WINDOWS_INSTALL_FILES}/README.TXT
	COMMAND cp ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.txt ${WINDOWS_INSTALL_FILES}
	COMMAND mkdir -p ${WINDOWS_INSTALL_FILES}/help
	COMMAND cp ${CMAKE_SOURCE_DIR}/doc/nextspace-help/index.html ${CMAKE_SOURCE_DIR}/doc/nextspace-help/quick-reference.txt ${WINDOWS_INSTALL_FILES}/help
	COMMAND todos ${WINDOWS_INSTALL_FILES}/*.TXT
	COMMAND cp -ra ${CMAKE_SOURCE_DIR}/nsis ${WINDOWS_INSTALL_FILES}
	COMMAND cp ${CMAKE_BINARY_DIR}/nsis/veyon.nsi ${WINDOWS_INSTALL_FILES}
	COMMAND find ${WINDOWS_INSTALL_FILES} -ls
)
endif()

add_custom_target(create-windows-installer
	COMMAND makensis ${WINDOWS_INSTALL_FILES}/veyon.nsi
	COMMAND mv ${WINDOWS_INSTALL_FILES}/${NEXTSPACE_INSTALLER_FILE} .
	COMMAND rm -rf ${WINDOWS_INSTALL_FILES}
	DEPENDS windows-binaries
)

add_custom_target(prepare-dev-nsi
	COMMAND sed -i ${WINDOWS_INSTALL_FILES}/veyon.nsi -e "s,/SOLID lzma,zlib,g"
	DEPENDS windows-binaries)

add_custom_target(dev-nsi
	DEPENDS prepare-dev-nsi create-windows-installer
)

