# Copyright (C) 2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# All xayautil files.  Shared between the native and WASM builds.

set (XAYAUTIL_SOURCES
  base64.cpp
  compression.cpp
  cryptorand.cpp
  hash.cpp
  jsonutils.cpp
  random.cpp
  uint256.cpp
)

set (XAYAUTIL_HEADERS
  base64.hpp
  compression.hpp
  cryptorand.hpp
  hash.hpp
  jsonutils.hpp
  random.hpp random.tpp
  uint256.hpp
)

set (XAYAUTIL_TEST_SOURCES
  base64_tests.cpp
  compression_tests.cpp
  cryptorand_tests.cpp
  hash_tests.cpp
  jsonutils_tests.cpp
  random_tests.cpp
  uint256_tests.cpp
)
