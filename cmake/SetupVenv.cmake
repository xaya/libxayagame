# Copyright (C) 2025 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Sets up a Python virtual environment for running tests as a build target.
#
# setup_venv(VENV_DIR REQUIREMENTS_FILE)
#
#   VENV_DIR          - path where the venv will be created
#   REQUIREMENTS_FILE - path to a pip requirements.txt file
#
# Adds a custom target "venv" (built by default) that creates a Python venv
# at VENV_DIR and installs the packages from REQUIREMENTS_FILE into it.
# The target is automatically rebuilt when REQUIREMENTS_FILE changes.
#
# Sets VENV_PYTHON_EXECUTABLE in the parent scope to the venv's Python
# binary, which can then be used to run Python tests.

function(setup_venv VENV_DIR REQUIREMENTS_FILE)
  set(VENV_PYTHON "${VENV_DIR}/bin/python")
  set(VENV_STAMP "${VENV_DIR}/.requirements-installed")

  add_custom_command(
    OUTPUT "${VENV_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${VENV_DIR}"
    COMMAND ${Python3_EXECUTABLE} -m venv "${VENV_DIR}"
    COMMAND "${VENV_PYTHON}" -m pip install -r "${REQUIREMENTS_FILE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${VENV_STAMP}"
    DEPENDS "${REQUIREMENTS_FILE}"
    COMMENT "Setting up Python virtual environment for tests"
  )

  add_custom_target(venv ALL DEPENDS "${VENV_STAMP}")

  set(VENV_PYTHON_EXECUTABLE "${VENV_PYTHON}" PARENT_SCOPE)
endfunction()