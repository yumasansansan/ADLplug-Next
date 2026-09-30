# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The version of ADLplug-Next, from the git history. Until the first release,
# 2.2.0, the builds are 2.1.N, where N counts the commits of main since bc9a041,
# the last of the builds numbered 1.99.N (whose N counted the commits since
# a488abe, the last commit of upstream ADLplug, up to 1.99.99). An odd minor
# number marks a development version. LV2 reads the numbers the same way: JUCE
# hands it the minor and the patch number as lv2:minorVersion and
# lv2:microVersion, an odd one of either marks a development version, and a
# minor version of 0 an experimental plugin that hosts need not show, which is
# why the release is 2.2.0 and not 2.0.0. LV2 has no major version, so a host
# takes a 1.99.N bundle for a newer one than any 2.x; the release notes of 2.2.0
# are to ask those who put a 1.99.N bundle in place by hand to remove it.
#
# JUCE packs the minor and the patch number into eight bits each of the version
# code (JucePlugin_VersionCode: the version of the Audio Units, and what
# sources/plugin_version.h reads the development bit from) without checking
# them, so a patch number of 256 would carry into the minor one: 2.1.256 would
# be 2.2.0. When N reaches 256 the builds go on as 2.3.(N - 256), then as
# 2.5.(N - 512), and so on, still odd and still rising; a release then takes
# the even minor number above the last of them.
#
# The version to show adds the time of the commit, in UTC, and its hash:
# 2.1.N+YYYYMMDD.HHMM.git<hash>. A commit gives the same version however often
# it is built. Without git or the history, as in an archive of the sources or a
# shallow clone, the version is 2.1.0+unknown.
#
# Sets ADLplug_VERSION, for project(), and ADLplug_VERSION_DISPLAY. Included
# before project().

set(ADLplug_VERSION "2.1.0")
set(ADLplug_VERSION_DISPLAY "2.1.0+unknown")

find_program(ADLplug_GIT git DOC "git, which gives the version of ADLplug-Next")

# Runs git in the source tree; the output is empty if git fails.
function(adlplug_git out)
  execute_process(COMMAND "${ADLplug_GIT}" -C "${CMAKE_CURRENT_SOURCE_DIR}" ${ARGN}
    OUTPUT_VARIABLE output RESULT_VARIABLE result
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT result EQUAL 0)
    set(output "")
  endif()
  set(${out} "${output}" PARENT_SCOPE)
endfunction()

if(ADLplug_GIT)
  adlplug_git(ADLplug_GIT_COUNT rev-list --first-parent --count bc9a041..HEAD)
  adlplug_git(ADLplug_GIT_TIME log -1 --format=%ct HEAD)
  adlplug_git(ADLplug_GIT_HASH rev-parse --short=7 HEAD)

  if(ADLplug_GIT_COUNT MATCHES "^[0-9]+$" AND ADLplug_GIT_TIME MATCHES "^[0-9]+$" AND
     ADLplug_GIT_HASH MATCHES "^[0-9a-f]+$")
    # string(TIMESTAMP) formats the time in SOURCE_DATE_EPOCH, when there is one,
    # instead of the current time.
    if(DEFINED ENV{SOURCE_DATE_EPOCH})
      set(ADLplug_SAVED_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
    endif()
    set(ENV{SOURCE_DATE_EPOCH} "${ADLplug_GIT_TIME}")
    string(TIMESTAMP ADLplug_GIT_WHEN "%Y%m%d.%H%M" UTC)
    if(DEFINED ADLplug_SAVED_EPOCH)
      set(ENV{SOURCE_DATE_EPOCH} "${ADLplug_SAVED_EPOCH}")
    else()
      unset(ENV{SOURCE_DATE_EPOCH})
    endif()

    # 256 commits to each odd minor number, as said above.
    math(EXPR ADLplug_VERSION_MINOR "1 + 2 * (${ADLplug_GIT_COUNT} / 256)")
    math(EXPR ADLplug_VERSION_PATCH "${ADLplug_GIT_COUNT} % 256")
    set(ADLplug_VERSION "2.${ADLplug_VERSION_MINOR}.${ADLplug_VERSION_PATCH}")
    set(ADLplug_VERSION_DISPLAY "${ADLplug_VERSION}+${ADLplug_GIT_WHEN}.git${ADLplug_GIT_HASH}")
  endif()

  # A commit, or checking out another one, configures the build again, so that
  # the version follows. The log of HEAD changes with both.
  foreach(ADLplug_GIT_FILE IN ITEMS HEAD logs/HEAD)
    adlplug_git(ADLplug_GIT_PATH rev-parse --git-path "${ADLplug_GIT_FILE}")
    if(ADLplug_GIT_PATH)
      get_filename_component(ADLplug_GIT_PATH "${ADLplug_GIT_PATH}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
      if(EXISTS "${ADLplug_GIT_PATH}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ADLplug_GIT_PATH}")
      endif()
    endif()
  endforeach()
endif()

message(STATUS "ADLplug-Next ${ADLplug_VERSION_DISPLAY}")
