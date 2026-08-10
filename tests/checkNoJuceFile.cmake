# Checks that nothing under src/ names juce::File except the header whose job it
# is and the two file choosers that cannot avoid it.
#
#   `juce::File` is JUCE's path type and it carries policy this codebase does not
# want: it normalises separators, resolves a relative path against the working
# directory, and compares case-insensitively everywhere but Linux. It also has to
# be converted at every boundary, and *that* was issue #28 -- a Documents folder
# whose name is not ASCII came back as mojibake because one such conversion used
# `juce::String( char const * )` instead of `fromUTF8()`, and the plugin created
# the mangled directory rather than finding the real one.
#
#   So the path type is `fs::path`, from the engine up to the editor, and the
# conversions live in one header with a test file to themselves. What remains is
# the outside edge: `juce::FileChooser` is handed a `juce::File` and answers with
# one, and there is no way to ask it for anything else.
#
#   The fix for anything reported here is to take the `fs::path` overload, or --
# if you really are at a chooser -- to convert with io/jucePath.hpp and keep the
# `juce::File` inside the one expression that needs it.
#
# Run: cmake -D SOURCE_DIR=<dir> -P checkNoJuceFile.cmake
#
# \note `juce::FileChooser`, `juce::FileInputStream` and friends are *not* matched
# and are not the point: what is being kept out is the type that would otherwise
# spread through signatures. The match requires a non-identifier character after
# the name.
#
# \note A source scan, like checkNoAssertSideEffects.cmake beside it and unlike
# the two compile_commands.json gates: what is checked is what the text says, so
# this needs no build directory and runs under every generator.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# See the note in checkNoJuceInDSP.cmake: `cmake -P` sets no policies of its own.
cmake_minimum_required(VERSION 3.28)

################################################################################
#
# \note Source text is never put through a CMake list, for the reason
# checkNoAssertSideEffects.cmake gives at length: `file(STRINGS)` merges lines
# whenever the content holds an unmatched `[`, and C++ is full of them. Lines are
# peeled off a plain string instead.
#
################################################################################

# The one header that exists to name it, and the two choosers.
set(allowed
        "src/io/jucePath.hpp"
        "src/gui/editor/spectrumWorxEditor.cpp"
        "src/gui/preset_browser/presetBrowser.cpp"
)

file(GLOB_RECURSE sources
        "${SOURCE_DIR}/src/*.cpp"
        "${SOURCE_DIR}/src/*.hpp"
        "${SOURCE_DIR}/src/*.inl"
)

# \note A string rather than a list, and for the same reason the line peeling
# above is hand-rolled: an offending line is C++ and almost always ends in a
# semicolon, which a CMake list would read as a separator -- so one hit came back
# as two entries, the second of them empty, and the reported count was wrong.
set(offenderReport "")
set(offenderCount 0)
set(checkedCount 0)

foreach (sourceFile IN LISTS sources)
    file(RELATIVE_PATH relativePath "${SOURCE_DIR}" "${sourceFile}")
    if (relativePath IN_LIST allowed)
        continue()
    endif ()

    file(READ "${sourceFile}" contents)

    # Most of the tree never mentions it, and peeling a whole file a line at a
    # time is the expensive part of this check.
    string(FIND "${contents}" "juce::File" position)
    if (position EQUAL -1)
        continue()
    endif ()

    math(EXPR checkedCount "${checkedCount} + 1")

    set(lineNumber 0)
    while (NOT contents STREQUAL "")
        string(FIND "${contents}" "\n" newlinePosition)
        if (newlinePosition EQUAL -1)
            set(line "${contents}")
            set(contents "")
        else ()
            string(SUBSTRING "${contents}" 0 ${newlinePosition} line)
            math(EXPR afterNewline "${newlinePosition} + 1")
            string(SUBSTRING "${contents}" ${afterNewline} -1 contents)
        endif ()
        math(EXPR lineNumber "${lineNumber} + 1")

        # \note Prose is skipped, and there is a lot of it: this tree explains
        # what used to stand where, so a good many notes have to say the name of
        # the type that was removed. Every doc comment here opens with `///`,
        # `//`, `/*` or a continuation `*`, which is what this recognises -- a
        # block comment whose continuation lines start with something else would
        # be read as code, and that is the safe direction to be wrong in.
        string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
        if (trimmed MATCHES "^(//|\\*|/\\*)")
            continue()
        endif ()

        # The trailing character is what keeps juce::FileChooser and
        # juce::FileInputStream out of this: only the bare type matches.
        if (line MATCHES "juce::File($|[^A-Za-z0-9_])")
            string(APPEND offenderReport "\n    ${relativePath}:${lineNumber}: ${trimmed}")
            math(EXPR offenderCount "${offenderCount} + 1")
        endif ()
    endwhile ()
endforeach ()

if (offenderCount GREATER 0)
    string(REPLACE ";" "\n    " allowedList "${allowed}")
    message(FATAL_ERROR
            "${offenderCount} use(s) of juce::File outside the file-chooser edge:\
${offenderReport}\n\n\
The path type in src/ is `fs::path`. Take its overload, or -- if this really is a \
juce::FileChooser, which is handed a juce::File and answers with one -- convert \
with src/io/jucePath.hpp and keep the juce::File inside that one expression.\n\n\
Only these may name it:\n    ${allowedList}")
endif ()

message(STATUS
        "${checkedCount} source(s) mention juce::File; none outside the file-chooser edge.")
