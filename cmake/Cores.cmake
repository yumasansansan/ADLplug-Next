# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The emulator cores of the chip that the build is for (ADLplug_CHIP). Each
# entry of ADLplug_CORES is the emulator number, its name, the build option
# that includes it, and the number of the core that plays in its place when a
# build leaves it out: an OPL2 core plays as DOSBox in OPL2 mode, an OPNA core
# as MAME YM2608, any other as the default core, ADLplug_DEFAULT_CORE
# (sources/*/adl/chip_settings.cc). The tests render with the cores
# (tests/CMakeLists.txt), and so does the training of profile-guided
# optimisation (cmake/PGO.cmake).

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_DEFAULT_CORE 2)
  set(ADLplug_CORES
    "0|Nuked OPL3 (v 1.8)|USE_NUKED_EMULATOR|2"
    "1|Nuked OPL3 Fast (by tgies)|USE_NUKED_EMULATOR|2"
    "2|DOSBox 0.74-r4111 OPL3|USE_DOSBOX_EMULATOR|2"
    "3|Opal OPL3|USE_OPAL_EMULATOR|2"
    "4|Java 1.0.6 OPL3|USE_JAVA_EMULATOR|2"
    "5|ESFMu|USE_ESFMU_EMULATOR|2"
    "6|MAME OPL2|USE_MAME_EMULATOR|13"
    "7|YMFM OPL2|USE_YMFM_EMULATOR|13"
    "8|YMFM OPL3|USE_YMFM_EMULATOR|2"
    "9|YM3812-LLE OPL2|USE_NUKED_OPL2_LLE_EMULATOR|13"
    "10|YMF262-LLE OPL3|USE_NUKED_OPL3_LLE_EMULATOR|2"
    "11|Nuked OPL2 Lite|USE_NUKED_EMULATOR|13"
    "12|Nuked CQM|USE_NUKED_EMULATOR|2"
    "13|DOSBox 0.74-r4111 OPL2|USE_DOSBOX_EMULATOR|13")
else()
  set(ADLplug_DEFAULT_CORE 0)
  set(ADLplug_CORES
    "0|MAME YM2612|USE_MAME_EMULATOR|0"
    "1|Nuked OPN2 (3438)|USE_NUKED_EMULATOR|0"
    "2|GENS/GS II OPN2|USE_GENS_EMULATOR|0"
    "3|YMFM OPN2|USE_YMFM_EMULATOR|0"
    "4|Neko Project II Kai OPNA|USE_NP2_EMULATOR|5"
    "5|MAME YM2608|USE_MAME_2608_EMULATOR|0"
    "6|YMFM OPNA|USE_YMFM_EMULATOR|5"
    "8|Nuked OPN2 (2612)|USE_NUKED_EMULATOR|0"
    "9|YM2612-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR|0"
    "10|YM2608-LLE OPNA|USE_NUKED_OPNA_LLE_EMULATOR|5"
    "11|YM3438-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR|0"
    "12|YMF276-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR|0")
endif()
