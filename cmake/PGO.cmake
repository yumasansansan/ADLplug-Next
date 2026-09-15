# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# Profile-guided optimisation (PGO) of Release builds, with ADLplug_PGO (on by
# default). Such a build trains before it compiles:
# - it configures and builds, as an external project in pgo/instrumented, the
#   same sources with the same settings, instrumented (-fprofile-generate):
#   the VST3 plugin, and the render tool of tools/render, not instrumented;
# - it renders the fixed sequence of the render tool with every core of the
#   build (cmake/PGOTrain.cmake, the cores of cmake/Cores.cmake) and merges the
#   profiles with llvm-profdata into pgo/profile.profdata;
# - it compiles every target with the profile (-fprofile-use), and ThinLTO
#   carries it into the link.
# Every object depends on the profile, which is written again only when the
# instrumented plugin or the render tool changes. The training runs the plugin,
# so the machine that builds has to run it: an AVX2 build needs a processor
# with AVX2. Where it cannot, and to build faster, turn ADLplug_PGO off.
#
# Included after the other flags of the build and before any target. The
# instrumented build is told so with the internal ADLplug_PGO_STAGE.

option(ADLplug_PGO "Optimise Release builds with profiles of their own renders (profile-guided optimisation)" ON)
set(ADLplug_PGO_STAGE "" CACHE INTERNAL "The stage of profile-guided optimisation that this build is")

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  return()
endif()

if(ADLplug_PGO_STAGE STREQUAL "instrument")
  add_compile_options(-fprofile-generate)
  add_link_options(-fprofile-generate)
  # The render tool that trains is not instrumented itself; it links none of
  # the instrumented libraries.
  function(adlplug_pgo_uninstrumented_render_tool)
    target_compile_options(ADLplug_render PRIVATE -fno-profile-generate)
    target_link_options(ADLplug_render PRIVATE -fno-profile-generate)
  endfunction()
  cmake_language(DEFER CALL adlplug_pgo_uninstrumented_render_tool)
  return()
endif()

if(NOT ADLplug_PGO)
  return()
endif()

string(REGEX MATCH "^[0-9]+" ADLplug_PGO_LLVM_MAJOR "${ADLplug_LLVM_VERSION}")
find_program(ADLplug_LLVM_PROFDATA NAMES llvm-profdata "llvm-profdata-${ADLplug_PGO_LLVM_MAJOR}"
  HINTS "${ADLplug_LLVM_BIN}" NO_CACHE)
if(NOT ADLplug_LLVM_PROFDATA)
  message(FATAL_ERROR "ADLplug_PGO needs llvm-profdata of LLVM ${ADLplug_LLVM_VERSION}; "
    "install it, or turn ADLplug_PGO off.")
endif()
adlplug_check_llvm_tool("profile merger" "${ADLplug_LLVM_PROFDATA}" "^llvm-profdata(-[0-9]+)?(\\.exe)?$")

set(ADLplug_PGO_DIR "${CMAKE_BINARY_DIR}/pgo")
set(ADLplug_PGO_PROFILE "${ADLplug_PGO_DIR}/profile.profdata")
set(ADLplug_PGO_TRAIN_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/PGOTrain.cmake")
add_compile_options("-fprofile-use=${ADLplug_PGO_PROFILE}")

# LLVM optimises for size the code that a profile shows little of (PGSO):
# code that the training does not reach, the editor's for one, and cores that
# the heavier ones outweigh in the profile. ESFMu rendered 9 % slower for it. It
# is turned off, in the compilers and in the LTO backend of the link.
add_compile_options("-mllvm=-pgso=false")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_link_options("LINKER:/mllvm:-pgso=false")
elseif(APPLE)
  add_link_options("LINKER:-mllvm,-pgso=false")
else()
  add_link_options("LINKER:--plugin-opt=-pgso=false")
endif()

# At the end of the top-level directory, when every target and setting exists:
# the instrumented build, the training, and the dependencies on the profile.
function(adlplug_pgo_train)
  include(ExternalProject)
  set(instrumented "${ADLplug_PGO_DIR}/instrumented")

  # The settings of this build that decide how the plugin is compiled, and
  # which cores and formats it has; the instrumented build also has the VST3
  # plugin and the render tool.
  set(settings "")
  get_cmake_property(cache_variables CACHE_VARIABLES)
  foreach(variable IN LISTS cache_variables)
    if(variable MATCHES "^(CMAKE_(C|CXX|RC)_COMPILER|CMAKE_MAKE_PROGRAM|CMAKE_LINKER_TYPE|CMAKE_(AR|RANLIB|NM|OBJCOPY|OBJDUMP|STRIP|READELF|ADDR2LINE|DLLTOOL)|CMAKE_(EXE|SHARED|MODULE)_LINKER_FLAGS|CMAKE_OSX_[A-Z_]+|ADLplug_(CHIP|ARCH|ASSERTIONS|WERROR|GREYZONE_BANKS|ASIO|LV2|AU|AAX|Standalone)|USE_[A-Z0-9_]+_EMULATOR)$")
      get_property(type CACHE "${variable}" PROPERTY TYPE)
      if(type STREQUAL "UNINITIALIZED")
        set(type STRING)
      endif()
      list(APPEND settings "-D${variable}:${type}=${${variable}}")
    endif()
  endforeach()
  # The environment that the presets give to the helpers JUCE builds.
  set(environment "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "LDFLAGS=$ENV{LDFLAGS}")
  if(CMAKE_RC_COMPILER)
    list(APPEND environment "RC=${CMAKE_RC_COMPILER}")
  endif()
  if(DEFINED ENV{ADLplug_LLVM_MAJOR})
    list(APPEND environment "ADLplug_LLVM_MAJOR=$ENV{ADLplug_LLVM_MAJOR}")
  endif()

  ExternalProject_Add(ADLplug_pgo_instrumented
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${instrumented}"
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env ${environment}
      "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" -S "${CMAKE_SOURCE_DIR}" -B "${instrumented}"
      -DCMAKE_BUILD_TYPE=Release ${settings}
      -DADLplug_VST3=ON -DADLplug_BUILD_TOOLS=ON -DADLplug_BUILD_TESTS=OFF
      -DADLplug_PGO=OFF -DADLplug_PGO_STAGE=instrument
    BUILD_COMMAND "${CMAKE_COMMAND}" --build "${instrumented}" --target ADLplug_VST3 ADLplug_render
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE)

  # Each core of the build renders for some seconds: the default core the
  # longest, and the low-level cores, far slower than the others, the least.
  set(cores "")
  foreach(core IN LISTS ADLplug_CORES)
    string(REPLACE "|" ";" fields "${core}")
    list(GET fields 0 number)
    list(GET fields 2 core_option)
    if(NOT ${core_option})
      continue()
    endif()
    if(number EQUAL ADLplug_DEFAULT_CORE)
      list(APPEND cores "${number}:60")
    elseif(core_option MATCHES "_LLE_")
      list(APPEND cores "${number}:2")
    else()
      list(APPEND cores "${number}:20")
    endif()
  endforeach()
  list(JOIN cores "," cores)

  add_custom_target(ADLplug_pgo_profile
    COMMAND "${CMAKE_COMMAND}"
      "-DRENDER=${instrumented}/ADLplug_render_artefacts/Release/ADLplug_render${CMAKE_EXECUTABLE_SUFFIX}"
      "-DPLUGIN=${instrumented}/ADLplug_artefacts/Release/VST3/${ADLplug_NAME}.vst3"
      "-DCORES=${cores}" "-DPROFDATA=${ADLplug_LLVM_PROFDATA}"
      "-DPROFILE=${ADLplug_PGO_PROFILE}" "-DWORK=${ADLplug_PGO_DIR}/training"
      -P "${ADLplug_PGO_TRAIN_SCRIPT}"
    BYPRODUCTS "${ADLplug_PGO_PROFILE}"
    USES_TERMINAL
    VERBATIM)
  add_dependencies(ADLplug_pgo_profile ADLplug_pgo_instrumented)

  # Nuked CQM rendered 3 to 4 % slower with the profile, and as fast as without
  # it when its sources were compiled without the profile: so they are.
  set(libadlmidi "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI")
  if(TARGET ADLMIDI_static)
    set_property(SOURCE "${libadlmidi}/src/chips/nuked_cqm.cpp" "${libadlmidi}/src/chips/nuked_cqm/cqm.c"
      DIRECTORY "${libadlmidi}" APPEND PROPERTY COMPILE_OPTIONS -fno-profile-instr-use)
  endif()

  # Every compiled target waits for the profile, and every source it compiles,
  # those of JUCE's modules included, is compiled again when the profile
  # changes.
  set(directories "${CMAKE_SOURCE_DIR}")
  set(compiled "")
  set(interface_sources "")
  while(directories)
    list(POP_FRONT directories directory)
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    list(APPEND directories ${subdirectories})
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
      get_target_property(type ${target} TYPE)
      if(type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(sources ${target} INTERFACE_SOURCES)
        if(sources)
          list(APPEND interface_sources ${sources})
        endif()
      elseif(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
        list(APPEND compiled ${target})
      endif()
    endforeach()
  endwhile()
  list(FILTER interface_sources EXCLUDE REGEX "\\$<")
  foreach(target IN LISTS compiled)
    get_target_property(sources ${target} SOURCES)
    get_target_property(source_dir ${target} SOURCE_DIR)
    set(files ${interface_sources})
    foreach(source IN LISTS sources)
      if(NOT source MATCHES "\\$<")
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}" OUTPUT_VARIABLE source)
        list(APPEND files "${source}")
      endif()
    endforeach()
    list(REMOVE_DUPLICATES files)
    set_property(SOURCE ${files} TARGET_DIRECTORY ${target} APPEND PROPERTY OBJECT_DEPENDS "${ADLplug_PGO_PROFILE}")
    add_dependencies(${target} ADLplug_pgo_profile)
  endforeach()
endfunction()
cmake_language(DEFER CALL adlplug_pgo_train)
