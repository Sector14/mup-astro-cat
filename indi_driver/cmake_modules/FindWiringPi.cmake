# - Try to find LibWiringPi
# Once done, this will define
#
#  WiringPi_FOUND - system has WiringPi
#  WiringPi_INCLUDE_DIR - the WiringPi include directories
#  WiringPi_LIBRARY - link these to use WiringPi
#
# Based on https://github.com/Tronic/cmake-modules/blob/master/LibFindMacros.cmake

include(LibFindMacros)

# Dependencies
# libfind_package(FOOBAR libfoobar)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(WiringPi_PKGCONF WiringPi)

# Include dir
find_path(WiringPi_INCLUDE_DIR
  NAMES wiringPi.h
  PATHS ${WiringPi_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(WiringPi_LIBRARY
  NAMES wiringPi
  PATHS ${WiringPi_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
libfind_process(WiringPi)
