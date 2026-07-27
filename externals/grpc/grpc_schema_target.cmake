# The MIT License
#
# Copyright (c) 2022 wstux
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

function(_grpc_generate TARGET_NAME SCHEMA)
    get_filename_component(_abs_schema_path ${SCHEMA} ABSOLUTE)
    get_filename_component(_schema_name ${SCHEMA} NAME_WE)

    set(_protoc_exe $<TARGET_FILE:protoc>)
    set(_grpc_cpp_pligin $<TARGET_FILE:grpc_cpp_plugin>)

    set(_schema_out "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.pb.cc"
                    "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.pb.h"
                    "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.grpc.pb.cc"
                    "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.grpc.pb.h"
    )

    add_custom_command(
        OUTPUT  ${_schema_out}
        COMMAND ${_protoc_exe}
        ARGS    --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                --plugin=protoc-gen-grpc=${_grpc_cpp_pligin}
                ${SCHEMA}
        DEPENDS ${_abs_schema_path}
        COMMENT "Running C++ grpc and protocol buffer compiler on ${SCHEMA}"
        VERBATIM
    )
endfunction()

function(GrpcSchemaTarget TARGET_NAME)
    set(_flags_kw   )
    set(_values_kw  INCLUDE_DIR LANGUAGE SCHEMA)
    set(_lists_kw   )
    _parse_target_args(${TARGET_NAME}
        _flags_kw _values_kw _lists_kw ${ARGN}
    )

    set(_target     ${TARGET_NAME})
    set(_lang       ${${TARGET_NAME}_LANGUAGE})
    set(_schema     ${${TARGET_NAME}_SCHEMA})

    get_filename_component(_schema_name ${_schema} NAME_WE)
    set(_proto_schema_header    "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.pb.h")
    set(_proto_schema_source    "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.pb.cc")
    set(_grpc_schema_header     "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.grpc.pb.h")
    set(_grpc_schema_source     "${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}.grpc.pb.cc")

    _grpc_generate(${_target} ${_schema})

    add_library(${_target} STATIC
        ${_proto_schema_header} ${_proto_schema_source}
        ${_grpc_schema_header} ${_grpc_schema_source}
    )

    target_compile_options(${_target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-pedantic>)
    target_compile_options(${_target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-parameter>)

    target_include_directories(${_target} PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${protobuf_SOURCE_DIR}/src
        ${absl_SOURCE_DIR}
        ${grpc_SOURCE_DIR}/include
    )

    get_target_property(_grpc_libraries grpc LIBRARIES)
    target_link_libraries(${_target} PUBLIC "${_grpc_libraries}")
endfunction()
