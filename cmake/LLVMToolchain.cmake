# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# Checks, when the build is configured and so before anything is built, that
# the toolchain that the build runs is LLVM's, and of one version: the C and
# C++ compilers, the LLD that Clang runs for -fuse-ld=lld, and the archivers,
# those that CMake uses for LTO included. A tool that reports its version has
# to report the version of the compilers exactly; llvm-rc, which reports none,
# has to lie beside the C++ compiler. When the environment variable
# ADLplug_LLVM_MAJOR is set, as ci/setup.sh sets it in CI, the compilers have to
# be of that major version. The presets name the tools without a version, and
# this check decides what the build accepts.
#
# Included after project(). Defines adlplug_check_llvm_tool(), with which
# CMakeLists.txt checks the resource compiler on Windows.

set(ADLplug_LLVM_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
get_filename_component(ADLplug_LLVM_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
file(REAL_PATH "${ADLplug_LLVM_BIN}" ADLplug_LLVM_BIN)

# Each tool that passes is also written to llvm-toolchain.txt in the build
# directory, a line of its part, its version and its program, separated by
# tabs, which ci/build.sh shows in the log and the summary of a CI run.
set(ADLplug_LLVM_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/llvm-toolchain.txt")
file(WRITE "${ADLplug_LLVM_TOOLCHAIN_FILE}" "")

if(DEFINED ENV{ADLplug_LLVM_MAJOR})
  string(REGEX MATCH "^[0-9]+" ADLplug_LLVM_MAJOR "${ADLplug_LLVM_VERSION}")
  if(NOT ADLplug_LLVM_MAJOR STREQUAL "$ENV{ADLplug_LLVM_MAJOR}")
    message(FATAL_ERROR
      "The compilers are of LLVM ${ADLplug_LLVM_VERSION}, but ADLplug_LLVM_MAJOR asks "
      "for LLVM $ENV{ADLplug_LLVM_MAJOR}.")
  endif()
  message(STATUS "LLVM ${ADLplug_LLVM_MAJOR}, the major version that ADLplug_LLVM_MAJOR asks for")
endif()

# Checks the program that plays a part of the toolchain: its file name has to
# match the pattern, and its version the compilers' version.
function(adlplug_check_llvm_tool part program pattern)
  if(NOT program)
    message(FATAL_ERROR "No program is set for the ${part}.")
  endif()
  if(NOT IS_ABSOLUTE "${program}")
    find_program(ADLplug_LLVM_TOOL_PATH NAMES "${program}" NO_CACHE)
    if(NOT ADLplug_LLVM_TOOL_PATH)
      message(FATAL_ERROR "The ${part}, ${program}, is not found.")
    endif()
    set(program "${ADLplug_LLVM_TOOL_PATH}")
  endif()

  get_filename_component(name "${program}" NAME)
  if(NOT name MATCHES "${pattern}")
    message(FATAL_ERROR "The ${part} is ${program}, which is not a tool of LLVM.")
  endif()

  execute_process(COMMAND "${program}" --version
    OUTPUT_VARIABLE output ERROR_VARIABLE output TIMEOUT 60)
  if(output MATCHES "(clang version|LLVM version|LLD) ([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(version "${CMAKE_MATCH_2}")
    set(checked "${version}")
    if(NOT version VERSION_EQUAL ADLplug_LLVM_VERSION)
      message(FATAL_ERROR
        "The ${part} is ${program}, of LLVM ${version}, but the compilers are of "
        "LLVM ${ADLplug_LLVM_VERSION}.")
    endif()
  else()
    set(version "none")
    set(checked "no version reported, beside the compiler")
    get_filename_component(directory "${program}" DIRECTORY)
    file(REAL_PATH "${directory}" directory)
    set(expected "${ADLplug_LLVM_BIN}")
    if(CMAKE_HOST_WIN32)
      string(TOLOWER "${directory}" directory)
      string(TOLOWER "${expected}" expected)
    endif()
    if(NOT directory STREQUAL expected)
      message(FATAL_ERROR
        "The ${part} is ${program}, which reports no version and does not lie "
        "beside the C++ compiler in ${ADLplug_LLVM_BIN}.")
    endif()
  endif()
  message(STATUS "  ${part}: ${program} (${checked})")
  file(APPEND "${ADLplug_LLVM_TOOLCHAIN_FILE}" "${part}\t${version}\t${program}\n")
endfunction()

message(STATUS "LLVM toolchain ${ADLplug_LLVM_VERSION}")
adlplug_check_llvm_tool("C compiler" "${CMAKE_C_COMPILER}" "^clang(-[0-9]+)?(\\.exe)?$")
adlplug_check_llvm_tool("C++ compiler" "${CMAKE_CXX_COMPILER}" "^clang(\\+\\+)?(-[0-9]+)?(\\.exe)?$")

# The LLD that Clang finds for -fuse-ld=lld: lld-link for Windows, ld64.lld for
# Apple systems, ld.lld elsewhere.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(ADLplug_LLD_NAME lld-link)
elseif(APPLE)
  set(ADLplug_LLD_NAME ld64.lld)
else()
  set(ADLplug_LLD_NAME ld.lld)
endif()
execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "--print-prog-name=${ADLplug_LLD_NAME}"
  OUTPUT_VARIABLE ADLplug_LLD OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT IS_ABSOLUTE "${ADLplug_LLD}")
  message(FATAL_ERROR "Clang does not find ${ADLplug_LLD_NAME}, the LLD it runs for -fuse-ld=lld.")
endif()
adlplug_check_llvm_tool("linker" "${ADLplug_LLD}" "^(lld-link|ld64\\.lld|ld\\.lld)(\\.exe)?$")

foreach(ADLplug_LLVM_ARCHIVER IN ITEMS
    CMAKE_AR CMAKE_RANLIB
    CMAKE_C_COMPILER_AR CMAKE_C_COMPILER_RANLIB
    CMAKE_CXX_COMPILER_AR CMAKE_CXX_COMPILER_RANLIB)
  adlplug_check_llvm_tool("${ADLplug_LLVM_ARCHIVER}" "${${ADLplug_LLVM_ARCHIVER}}"
    "^llvm-(ar|ranlib)(-[0-9]+)?(\\.exe)?$")
endforeach()
