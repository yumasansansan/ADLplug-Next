#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/prototypes.sh [<file>...]
#
# Every function of this project's C says what it takes. A declarator with an
# empty parameter list -- f() where f(void) was meant -- is an error here, and
# this is the only thing that can say so:
#
#   - The compiler cannot. In C before C23, f() declared a function that takes
#     anything, and -Wstrict-prototypes said so; C23 made f() mean f(void), so
#     there is nothing left for a compiler to warn about. Asked with -Weverything
#     under -std=c23, Clang 23 says nothing at all about f().
#   - The abstract syntax tree cannot. Clang gives both forms the same type,
#     int (void), so a check written as a matcher over the tree cannot tell them
#     apart. Only the text can, which is what this script reads.
#   - clang-tidy has the check the other way round (modernize-redundant-void-arg
#     takes the void out), which is why .clang-tidy leaves it off.
#
# The rule: in a C file, a parameter list of nothing is written (void). Without
# arguments the script reads every .c file of the project; with arguments it
# reads those files. It prints each place and fails if there is one.
#
# What a declaration looks like, as text: a type and then a name before the
# parentheses (int f(), static void g()), or a declarator in parentheses before
# them (void (*handler)()). A call is none of those -- nothing stands before its
# name but the beginning of a statement, an operator, or a keyword -- so the
# keywords that can stand there are left out by name.
set -euo pipefail

keywords='return|sizeof|case|goto|else|do|while|if|switch|for|typeof|typeof_unqual|alignof|_Generic|_Alignof|static_assert'

files=("$@")
if [ ${#files[@]} -eq 0 ]; then
  shopt -s nullglob globstar
  files=(sources/**/*.c tests/**/*.c fuzz/**/*.c tools/**/*.c)
fi
if [ ${#files[@]} -eq 0 ]; then
  echo "error: no C files to read" >&2
  exit 1
fi

echo "== the parameter lists of ${#files[@]} C files"

found=0
for file in "${files[@]}"; do
  # A type and a name, then nothing between the parentheses.
  if grep -nPH "(?<![\w)])(?!(?:$keywords)\b)[A-Za-z_]\w*[\s*]+\**[A-Za-z_]\w*\s*\(\s*\)" "$file"; then
    found=1
  fi
  # A declarator of its own in parentheses, then nothing between the next ones.
  if grep -nPH '\(\s*\*\s*[A-Za-z_]\w*\s*\)\s*\(\s*\)' "$file"; then
    found=1
  fi
done

if [ "$found" -ne 0 ]; then
  echo "error: the parameter list of a function is written (void), not (); the lines above leave it out" >&2
  exit 1
fi
echo "Every function says what it takes."
