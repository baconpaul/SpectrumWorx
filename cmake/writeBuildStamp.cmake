# Writes src/configuration/buildStamp.cpp.in out with this moment's date and
# time and the working tree's commit.
#
# Run: cmake -D TEMPLATE=<file> -D OUTPUT=<file> -D SOURCE_DIR=<dir>
#            -P writeBuildStamp.cmake
#
# This is a script, not a module: it has to run at *build* time, once per build,
# which is a thing only a separate `cmake -P` process can do. src/buildStamp.cmake
# is what arranges for it to be run. \see the note there for why any of this
# exists.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# See the note in tests/checkODRHeaderScope.cmake: `cmake -P` sets no policies.
cmake_minimum_required(VERSION 3.28)

# Local time rather than UTC, unlike sst-plugininfra's version_information. What
# this answers is "is the plugin the host just loaded the one I built a minute
# ago", and a minute ago is a local-clock fact.
string(TIMESTAMP SW_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP SW_BUILD_TIME "%H:%M:%S")

set(SW_BUILD_COMMIT "no-git")

find_package(Git QUIET)
if (GIT_FOUND)
    # ERROR_QUIET on both, deliberately. This runs on every build, and a source
    # tree that is not a repository -- a release tarball -- would otherwise print
    # "fatal: not a git repository" every time somebody typed make. The failure
    # is not swallowed: it becomes the word "no-git" in the string the editor
    # draws, which is where a person can actually see it.
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=9 HEAD
                    WORKING_DIRECTORY "${SOURCE_DIR}"
                    OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE status ERROR_QUIET)
    if (status EQUAL 0)
        set(SW_BUILD_COMMIT "${commit}")

        # Tracked files only: an untracked scratch file in the tree is not
        # something the binary was built from, and marking every build dirty
        # would make the mark mean nothing.
        execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no
                        WORKING_DIRECTORY "${SOURCE_DIR}"
                        OUTPUT_VARIABLE modifications OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
        if (modifications)
            string(APPEND SW_BUILD_COMMIT "+")
        endif ()
    endif ()
endif ()

configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
