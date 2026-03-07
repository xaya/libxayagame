# Copyright (C) 2018-2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

include (sources.cmake)

add_library (xayautil STATIC ${XAYAUTIL_SOURCES})

set_target_properties (xayautil PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories (xayautil PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries (xayautil
  PUBLIC
    PkgConfig::JSONCPP
    PkgConfig::GLOG
  PRIVATE
    PkgConfig::OPENSSL
    PkgConfig::ZLIB
)

# Unit tests.
add_executable (xayautil_tests ${XAYAUTIL_TEST_SOURCES})
target_link_libraries (xayautil_tests PRIVATE
  xayautil
  PkgConfig::JSONCPP
  PkgConfig::ZLIB
  PkgConfig::GLOG
  GTest::gmock_main
)
add_test (NAME xayautil_tests COMMAND xayautil_tests)

# Installation.
install (TARGETS xayautil ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install (FILES ${XAYAUTIL_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayautil)

configure_file (libxayautil.pc.in libxayautil.pc @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/libxayautil.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
