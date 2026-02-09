# Copyright (C) 2018-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

add_library (xayautil STATIC
  base64.cpp
  compression.cpp
  cryptorand.cpp
  hash.cpp
  jsonutils.cpp
  random.cpp
  uint256.cpp
)

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
add_executable (xayautil_tests
  base64_tests.cpp
  compression_tests.cpp
  cryptorand_tests.cpp
  hash_tests.cpp
  jsonutils_tests.cpp
  random_tests.cpp
  uint256_tests.cpp
)
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
install (FILES
    base64.hpp
    compression.hpp
    cryptorand.hpp
    hash.hpp
    jsonutils.hpp
    random.hpp random.tpp
    uint256.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xayautil
)

configure_file (libxayautil.pc.in libxayautil.pc @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/libxayautil.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
