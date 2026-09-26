# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The patches of patches/, applied to the submodules when CMake configures, so
# that every build has them. They are for the faults that ADLplug-Next finds in
# the libraries and the framework it ships, the sanitizers' among them: the
# fault is fixed rather than left alone, and the patch is offered upstream. A
# patch goes away once it is upstream and the submodule has moved on.
#
# Configuring twice is not an error: a patch that is already in place is left
# alone. Editing one makes CMake configure again.

if(NOT ADLplug_GIT)
  message(FATAL_ERROR
    "git is needed to patch the submodules, and was not found. "
    "It is needed for the submodules themselves as well.")
endif()

# Applies <patch> to the working tree of <directory>, unless it is already
# there.
function(adlplug_patch directory patch)
  set(tree "${PROJECT_SOURCE_DIR}/${directory}")
  set(file "${PROJECT_SOURCE_DIR}/${patch}")
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "The patch ${patch} is missing.")
  endif()
  if(NOT EXISTS "${tree}/.git")
    message(FATAL_ERROR
      "${directory} is not a checked-out submodule; the submodules are part of "
      "the sources: git submodule update --init --recursive")
  endif()
  set_property(DIRECTORY "${PROJECT_SOURCE_DIR}"
    APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")

  # A submodule of a repository that another user checked out is a working tree
  # that git refuses to touch unless it is told the ownership is expected; that
  # is what the CI's containers, which run as root, hand it. The setting lasts
  # for this one command.
  set(git "${ADLplug_GIT}" -c "safe.directory=*")

  # Applying it in reverse would succeed if it were already applied.
  execute_process(COMMAND ${git} apply --reverse --check "${file}"
    WORKING_DIRECTORY "${tree}" RESULT_VARIABLE in_place
    OUTPUT_QUIET ERROR_QUIET)
  if(in_place EQUAL 0)
    message(STATUS "Patch, already applied: ${patch}")
    return()
  endif()

  execute_process(COMMAND ${git} apply "${file}"
    WORKING_DIRECTORY "${tree}" RESULT_VARIABLE applied
    ERROR_VARIABLE complaint)
  if(NOT applied EQUAL 0)
    message(FATAL_ERROR
      "${patch} does not apply to ${directory}:\n${complaint}\n"
      "The submodule may have changes of its own, or may have moved on and "
      "carry the change already. git -C ${directory} status shows what is "
      "there; git -C ${directory} checkout -- . throws local changes away.")
  endif()
  message(STATUS "Patch: ${patch}")
endfunction()

adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0001-xml-the-one-line-form-has-no-line-endings-to-write.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0002-memory-block-two-blocks-of-no-bytes-are-equal.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0003-no-bytes-to-zero-is-no-call-to-memset.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0004-a-string-of-no-text-reads-none-of-it.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0005-a-parameter-takes-no-value-that-is-no-number.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0006-the-message-manager-lock-releases-what-it-took.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0007-the-time-of-a-file-is-read-where-it-lies.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0008-a-parameter-tells-its-listeners-without-waiting.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0009-asking-which-thread-this-is-takes-no-lock.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0010-a-hex-character-reference-the-text-ends-in-ends-there.patch")
adlplug_patch("thirdparty/JUCE"
  "patches/JUCE/0011-wasapi-asks-for-more-channels-in-one-sample-format.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0001-esfmu-the-rhythm-volume-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0002-mame-opl2-modulation-and-feedback-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0003-ymfm-the-round-trip-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0004-nuked-opl2-the-crushed-sample-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0005-nuked-cqm-the-modulation-and-the-output-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0006-the-api-looks-at-the-numbers-it-is-given.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0007-models-the-frequency-models-stay-inside-their-tables.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0008-nuked-opl3-the-fast-path-silences-a-slot-as-the-other-does.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0009-the-resampler-runs-the-chip-at-the-rate-asked-for.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0010-nuked-opl3-the-fast-path-shifts-by-less-than-its-width.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0011-a-panic-takes-every-note-a-channel-holds.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0012-a-number-of-chips-the-library-refuses-is-not-one-it-keeps.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0013-a-note-with-one-chip-channel-has-no-second-one-to-read.patch")
adlplug_patch("thirdparty/libADLMIDI"
  "patches/libADLMIDI/0014-dosbox-a-rhythm-mode-drum-is-panned-by-its-own-channel.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0001-psg-a-noise-table-that-does-not-overflow.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0002-ym2612-the-dac-value-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0003-mame-phase-modulation-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0004-mame-feedback-and-dac-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0005-nuked-opn2-the-envelope-and-the-output-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0006-gens-the-phase-counter-that-wraps-is-unsigned.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0007-fmgen-the-self-feedback-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0008-ymfm-the-round-trip-without-shifting-a-negative.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0009-the-api-looks-at-the-numbers-it-is-given.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0010-gens-the-phase-a-channel-update-works-with-wraps-as-well.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0011-the-chip-type-that-is-no-family.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0012-the-resampler-runs-the-chip-at-the-rate-asked-for.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0013-a-panic-takes-every-note-a-channel-holds.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0014-a-number-of-chips-the-library-refuses-is-not-one-it-keeps.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0015-the-counter-the-interpolation-starts-from-is-set-before-it-is-read.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0016-mame-the-tables-every-chip-shares-are-built-once.patch")
adlplug_patch("thirdparty/libOPNMIDI"
  "patches/libOPNMIDI/0017-nuked-opn2-the-chip-type-is-the-chips-own.patch")
