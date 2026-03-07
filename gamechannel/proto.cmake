# Copyright (C) 2026 The Xaya developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# All gamechannel proto names.  Shared between the native and WASM builds.
set (GAMECHANNEL_PROTO_NAMES
  broadcast
  metadata
  signatures
  stateproof
  testprotos
)

# gamechannel_generate_protos (SOURCES_VAR HEADERS_VAR PROTO_SRC_DIR INCLUDE_ROOT OUT_ROOT)
#
# Generates C++ protobuf stubs for all gamechannel protos and sets
# SOURCES_VAR / HEADERS_VAR to the resulting .pb.cc / .pb.h paths.
#
# Arguments:
#   PROTO_SRC_DIR  - directory containing the .proto source files
#   INCLUDE_ROOT   - root passed to protoc's -I flag (proto import paths
#                    are relative to this, so output mirrors the structure)
#   OUT_ROOT       - root passed to protoc's --cpp_out flag
function (gamechannel_generate_protos SOURCES_VAR HEADERS_VAR
                                      PROTO_SRC_DIR INCLUDE_ROOT OUT_ROOT)
  file (MAKE_DIRECTORY "${OUT_ROOT}/gamechannel/proto")

  if (TARGET protobuf::protoc)
    set (_protoc protobuf::protoc)
  else ()
    find_program (_protoc_exe protoc REQUIRED)
    set (_protoc "${_protoc_exe}")
  endif ()

  set (_sources)
  set (_headers)
  foreach (proto ${GAMECHANNEL_PROTO_NAMES})
    set (_src "${PROTO_SRC_DIR}/${proto}.proto")
    set (_cc  "${OUT_ROOT}/gamechannel/proto/${proto}.pb.cc")
    set (_h   "${OUT_ROOT}/gamechannel/proto/${proto}.pb.h")
    add_custom_command (
      OUTPUT ${_cc} ${_h}
      COMMAND ${_protoc}
        -I${INCLUDE_ROOT}
        --cpp_out=${OUT_ROOT}
        ${_src}
      DEPENDS ${_src}
    )
    list (APPEND _sources ${_cc})
    list (APPEND _headers ${_h})
  endforeach ()

  set (${SOURCES_VAR} ${_sources} PARENT_SCOPE)
  set (${HEADERS_VAR} ${_headers} PARENT_SCOPE)
endfunction ()

# gamechannel_generate_proto_py (PROTO_SRC_DIR INCLUDE_ROOT OUT_ROOT PY_SRC_DIR)
#
# Generates Python protobuf stubs for all gamechannel protos, defines the
# gamechannel_proto_py ALL target that depends on them, and copies the
# gamechannel Python package source files into the build tree.
#
# Arguments:
#   PROTO_SRC_DIR  - directory containing the .proto source files
#   INCLUDE_ROOT   - root passed to protoc's -I flag
#   OUT_ROOT       - root passed to protoc's --python_out flag
#   PY_SRC_DIR     - directory containing the gamechannel Python source files
function (gamechannel_generate_proto_py PROTO_SRC_DIR INCLUDE_ROOT OUT_ROOT PY_SRC_DIR)
  set (_py_outputs)
  foreach (proto ${GAMECHANNEL_PROTO_NAMES})
    set (_src "${PROTO_SRC_DIR}/${proto}.proto")
    set (_py  "${OUT_ROOT}/gamechannel/proto/${proto}_pb2.py")
    add_custom_command (
      OUTPUT ${_py}
      COMMAND protobuf::protoc
        -I${INCLUDE_ROOT}
        --python_out=${OUT_ROOT}
        ${_src}
      DEPENDS ${_src}
    )
    list (APPEND _py_outputs ${_py})
  endforeach ()

  add_custom_target (gamechannel_proto_py ALL DEPENDS ${_py_outputs})

  # Copy Python package source files into the build tree so that the
  # gamechannel Python package is complete (source .py + generated _pb2.py).
  set (_py_sources
    __init__.py
    channeltest.py
    rpcbroadcast.py
    signatures.py
  )
  foreach (pyfile ${_py_sources})
    configure_file ("${PY_SRC_DIR}/${pyfile}" "${OUT_ROOT}/gamechannel/${pyfile}" COPYONLY)
  endforeach ()
  configure_file ("${PY_SRC_DIR}/proto/__init__.py"
    "${OUT_ROOT}/gamechannel/proto/__init__.py" COPYONLY)
endfunction ()
