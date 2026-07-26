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

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

################################################################################
# Public functions for setting global compilation options
################################################################################

macro(SetCxxStandard STANDARD)
    set(_std_version "${STANDARD}")

    string(TOLOWER ${STANDARD} STANDARD)
    if ("${STANDARD}" STREQUAL "default")
        set(_std_version "${CMAKE_CXX_STANDARD_COMPUTED_DEFAULT}")
    endif()

    set(_cxx_std "-std=gnu++${_std_version}")
    #set(_cxx_std "-std=c++${_std_version}")
    try_set_cxx_flag(CXX_STD "${_cxx_std}")
    if (FLAG_CXX_STD)
        log_info("Using C++${_std_version} standard")
    else ()
        log_fatal("Failed to set C++${_std_version} standard")
    endif()
endmacro()

macro(SetCStandard STANDARD)
    set(_std_version "${STANDARD}")

    string(TOLOWER ${STANDARD} STANDARD)
    if ("${STANDARD}" STREQUAL "default")
        set(_std_version "${CMAKE_C_STANDARD_COMPUTED_DEFAULT}")
    endif()

    set(_c_std "-std=gnu${_std_version}")
    #set(_cxx_std "-std=c${_std_version}")
    try_set_c_flag(C_STD "${_c_std}")
    if (FLAG_C_STD)
        log_info("Using C${_std_version} standard")
    else ()
        log_fatal("Failed to set C${_std_version} standard")
    endif()
endmacro()

################################################################################
# Utilities
################################################################################

function(_set_to_back LIST_OF_VALUES VALUE)
    if (${LIST_OF_VALUES})
        set(${LIST_OF_VALUES} "${${LIST_OF_VALUES}} ${VALUE}" PARENT_SCOPE)
    else()
        set(${LIST_OF_VALUES} "${VALUE}" PARENT_SCOPE)
    endif()
endfunction()

################################################################################
# Macro for set build flags
################################################################################

# Set C compiler flag.
macro(set_c_flag FLAG)
    if(CMAKE_C_COMPILER)
        if(${ARGC} GREATER 1)
            _set_to_back(CMAKE_C_FLAGS_${ARGV1} "${FLAG}")
            log_trace("Macro 'set_c_flag': ARGV1='${ARGV1}', FLAG='${FLAG}'")
        else()
            _set_to_back(CMAKE_C_FLAGS "${FLAG}")
            log_trace("Macro 'set_c_flag': FLAG='${FLAG}'")
        endif()
    endif()
endmacro()

# Set C++ compiler flag.
macro(set_cxx_flag FLAG)
    if(CMAKE_CXX_COMPILER)
        if(${ARGC} GREATER 1)
            _set_to_back(CMAKE_CXX_FLAGS_${ARGV1} "${FLAG}")
            log_trace("Macro 'set_cxx_flag': ARGV1='${ARGV1}', FLAG='${FLAG}'")
        else()
            _set_to_back(CMAKE_CXX_FLAGS "${FLAG}")
            log_trace("Macro 'set_cxx_flag': FLAG='${FLAG}'")
        endif()
    endif()
endmacro()

# Set C and C++ compiler flag.
macro(set_flag FLAG)
    set_c_flag(${FLAG} ${ARGN})
    set_cxx_flag(${FLAG} ${ARGN})
endmacro()

# Set C and C++ compiler flag if isset option.
macro(set_flag_by_opt OPT FLAG)
    log_trace("Macro 'set_flag_by_opt': OPT='${OPT}', OPT VALUE='${${OPT}}'")
    if("${${OPT}}" STREQUAL "ON")
        set_flag(${FLAG})
    endif()
endmacro()

# Try set C compiler flag if flag supported.
macro(try_set_c_flag PROP FLAG)
    if (CMAKE_C_COMPILER)
        log_trace("Macro 'try_set_c_flag': PROP='${PROP}', FLAG='${FLAG}', ARGV2='${ARGV2}'")

        set(CMAKE_REQUIRED_QUIET TRUE)
        check_c_compiler_flag(${FLAG} FLAG_${PROP})
        if (FLAG_${PROP})
            set_c_flag(${FLAG} ${ARGV2})
        endif()
    endif()
endmacro()

# Try set C++ compiler flag if flag supported.
macro(try_set_cxx_flag PROP FLAG)
    if (CMAKE_CXX_COMPILER)
        log_trace("Macro 'try_set_cxx_flag': PROP='${PROP}', FLAG='${FLAG}', ARGV2='${ARGV2}'")

        set(CMAKE_REQUIRED_QUIET TRUE)
        check_cxx_compiler_flag(${FLAG} FLAG_${PROP})
        if (FLAG_${PROP})
            set_cxx_flag(${FLAG} ${ARGV2})
        endif()
    endif()
endmacro()

# Try set C and C++ compiler flag if flag supported.
macro(try_set_flag PROP FLAG)
    log_trace("Macro 'try_set_flag': PROP='${PROP}', FLAG='${FLAG}', ARGV2='${ARGV2}'")
    set(CMAKE_REQUIRED_QUIET TRUE)
    if (CMAKE_C_COMPILER)
        check_C_compiler_flag(${FLAG} FLAG_${PROP})
    endif()
    if (CMAKE_CXX_COMPILER)
        check_cxx_compiler_flag(${FLAG} FLAG_${PROP})
    endif()
    if(FLAG_${PROP})
        set_c_flag(${FLAG} ${ARGV2})
        set_cxx_flag(${FLAG} ${ARGV2})
    endif()
endmacro()

# Try set C and C++ compiler flag if isset option.
macro(try_set_flag_by_opt OPT FLAG)
    log_trace("Macro 'try_set_flag_by_opt': OPT='${OPT}', OPT VALUE='${${OPT}}', FLAG='${FLAG}'")
    if("${${OPT}}" STREQUAL "ON")
        try_set_flag(${OPT} ${FLAG} ${ARGV2})
    endif()
endmacro()

################################################################################
# Macro for set linker flags
################################################################################

# Set linker flag.
macro(set_linker_flag FLAG)
    _set_to_back(CMAKE_EXE_LINKER_FLAGS    "${FLAG}")
    _set_to_back(CMAKE_SHARED_LINKER_FLAGS "${FLAG}")
    _set_to_back(CMAKE_MODULE_LINKER_FLAGS "${FLAG}")
    log_trace("Macro 'set_linker_flag': FLAG='${FLAG}'")
endmacro()

# Set linker flag if isset option.
macro(set_linker_flag_by_opt OPT FLAG)
    log_trace("Macro 'set_linker_flag_by_opt': OPT='${OPT}', OPT VALUE='${${OPT}}'")
    if ("${${OPT}}" STREQUAL "ON")
        set_linker_flag(${FLAG})
    endif()
endmacro()

# Try set linker flag.
macro(try_set_linker_flag PROP FLAG)
    # Check it with the C compiler
    set(CMAKE_REQUIRED_QUIET TRUE)
    set(CMAKE_REQUIRED_FLAGS ${FLAG})
    check_C_compiler_flag(${FLAG} FLAG_${PROP})
    set(CMAKE_REQUIRED_FLAGS "")
    if (FLAG_${PROP})
        set_linker_flag(${FLAG})
    endif()
endmacro()

# Try set linker flag if isset option.
macro(try_set_linker_flag_by_opt OPT FLAG)
    log_trace("Macro 'try_set_linker_flag_by_opt': OPT='${OPT}', OPT VALUE='${${OPT}}', FLAG='${FLAG}'")
    if ("${${OPT}}" STREQUAL "ON")
        try_set_linker_flag(${OPT} ${FLAG})
    endif()
endmacro()

################################################################################
# Set compiler flags
################################################################################

if (CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
    if (PROJECT_C_STANDARD)
        # Setting the C standard version from defined variable
        SetCStandard("${PROJECT_C_STANDARD}")
    elseif (USE_DEFAULT_STANDARD)
        # Setting the default C standard version
        SetCStandard("default")
    endif()
endif()

if (CMAKE_CXX_COMPILER)
    if (PROJECT_CXX_STANDARD)
        # Setting the C++ standard version from defined variable
        SetCxxStandard("${PROJECT_CXX_STANDARD}")
    elseif (USE_DEFAULT_STANDARD)
        # Setting the default C++ standard version
        SetCxxStandard("default")
    endif()
endif()

################################################################################
# Setting common compile flags
################################################################################

set_flag("-Os -DNDEBUG"     MINSIZEREL)
set_flag("-O3 -DNDEBUG"     RELEASE)
set_flag("-O2 -DNDEBUG -g3" RELWITHDEBINFO)
set_flag("-O0 -g3"          DEBUG)
set_flag("-Wall -Wextra")

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set_flag("-rdynamic")
    set_flag("-fPIC")
    set_flag("-ggdb3")
    set_flag("-ffunction-sections")
    set_flag("-fstrict-aliasing")
endif()

#try_set_flag(FPIE "-fPIE")
#try_set_linker_flag(LINKER_PIE "-pie")

# Use hidden symbol visibility if possible.
# void __attribute__((visibility("default"))) Exported() {...}
#try_set_flag(FVISIBILITY_HIDDEN "-fvisibility=hidden")

################################################################################
# Protecting stack
################################################################################

# try_set_flag(FSTACK_PROTECTOR "-fstack-protector-strong")
# if (NOT FLAG_FSTACK_PROTECTOR)
#     try_set_flag(FSTACK_PROTECTOR "-fstack-protector-all")
# endif()
# try_set_flag(WSTACK_PROTECTOR "-Wstack-protector")

# try_set_flag(FNO_STRICT_OVERFLOW "-fno-strict-overflow")

################################################################################
# Setting coverage compile flags
################################################################################

if (COVERAGE_BUILD)
    set_flag("-g -O0 --coverage -fprofile-arcs -ftest-coverage")
    set_linker_flag("-fprofile-arcs -ftest-coverage")
endif()

################################################################################
# Set flags if isset options
################################################################################

set_flag_by_opt(USE_FAST_MAT        "--ffast-math")

set_flag_by_opt(USE_LTO             "-flto=auto")
set_linker_flag_by_opt(USE_LTO      "-flto=auto")

set_flag_by_opt(USE_PEDANTIC        "-pedantic")
set_flag_by_opt(USE_PEDANTIC        "-pedantic-errors")
set_flag_by_opt(USE_WERROR          "-Werror")

################################################################################
# Setting linker options
################################################################################

set_linker_flag("-Wl,-rpath=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
