# Copyright (C) 2021-2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Python integration test.
add_test (
  NAME websocket_gametest_basic.py
  COMMAND ${VENV_PYTHON_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/gametest/basic.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gametest
)
set_tests_properties (websocket_gametest_basic.py PROPERTIES
  ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}:${PROJECT_SOURCE_DIR}/mover/gametest:$ENV{PYTHONPATH};BUILD_DIR=${PROJECT_BINARY_DIR}"
)

# Installation.
install (PROGRAMS gsp-websocket-server.py DESTINATION ${CMAKE_INSTALL_BINDIR})
