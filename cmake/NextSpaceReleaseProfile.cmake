# NextSpaceReleaseProfile.cmake - NextSpace 1.0.0 release cache-init profile
#
# description: pins the approved English-only, WebAPI-off native Windows (MSYS2 UCRT64) release options.
#              FORCE keeps the pinned values even in a reused cache. VEYON_DEBUG is not defined here, and the
#              existing stack-protection and _FORTIFY_SOURCE defaults in CMakeLists.txt are left unchanged.
# usage: cmake -C <source>/cmake/NextSpaceReleaseProfile.cmake -S <source> -B <fresh build directory>

set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "NextSpace release build type" FORCE)
set(WITH_TRANSLATIONS OFF CACHE BOOL "NextSpace 1.0.0 is English-only; translation sources stay in the tree" FORCE)
set(WITH_WEBAPI OFF CACHE BOOL "The optional WebAPI plugin is not part of the NextSpace 1.0.0 payload" FORCE)
set(WITH_TESTS OFF CACHE BOOL "No test binaries or runtime test extensions in the release" FORCE)
set(WITH_LTO OFF CACHE BOOL "Link-time optimization stays off on Windows" FORCE)
set(WITH_UNITY_BUILD OFF CACHE BOOL "No unity build for the release" FORCE)
set(WITH_PCH OFF CACHE BOOL "No precompiled headers for the release" FORCE)
