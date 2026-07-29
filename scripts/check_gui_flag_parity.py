#!/usr/bin/env python3
#
# Fails if any sw-dsp source stops compiling at LE_SW_GUI=1, or if a source that
# was configuration-independent stops being so.
#
# The GUI flag is not cosmetic. At LE_SW_GUI=1 four setters on
# Engine::ModuleParameters become virtual, and in a *release* build that gives
# the class a vptr: sizeof 64 -> 72, pLFOs_ at 56 -> 64, and the ModuleDSP* ->
# ModuleParameters* base adjustment flips +8 -> +0. In a *debug* build the two
# are already identical, because ModuleNode has an !NDEBUG-only virtual that
# pays for the vptr anyway.
#
# That asymmetry is why this check exists and why it compiles with -DNDEBUG: a
# translation unit that quietly becomes configuration-dependent would look fine
# in every debug build and corrupt memory in release.
#
# Both settings only need to agree for as long as both exist. When LE_SW_GUI=1
# becomes the only configuration this script goes with the flag.
#
# Usage: check_gui_flag_parity.py <build-dir> [--quiet]
#
# SPDX-License-Identifier: GPL-3.0-or-later

import concurrent.futures
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

# Known to be configuration-dependent, and legitimately so: these are the module
# class hierarchy, the factory that instantiates it, and the chain that walks it.
# A source arriving here that is not on this list is the thing worth catching.
EXPECTED_DEPENDENT = {
    "core/automatedModuleChain.cpp",
    "core/modules/automatedModule.cpp",
    "core/modules/factory.cpp",
    "core/spectrumWorxCore.cpp",
    "le/spectrumworx/engine/module.cpp",
    "le/spectrumworx/engine/moduleChainImpl.cpp",
    "le/spectrumworx/engine/moduleParameters.cpp",
    "le/spectrumworx/engine/processor.cpp",
}


def sources(root):
    text = open(os.path.join(root, "src", "dsp.cmake")).read()
    return re.findall(r"^\s{8}(\S+\.cpp)$", text, re.M)


def juce_flags(db):
    """LE_SW_GUI=1 reaches gui/resources.hpp, which needs JUCE. sw-dsp does not
    carry it until the flag flips, so borrow it from the target that does."""
    entry = next(e for e in db if e["file"].endswith("gui/resources.cpp"))
    return [
        f
        for f in entry["command"].split()
        if ("JUCE" in f or "juce" in f)
        and (f.startswith("-I") or f.startswith("-isystem") or f.startswith("-D"))
    ]


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: check_gui_flag_parity.py <build-dir> [--quiet]")
    build = sys.argv[1]
    quiet = "--quiet" in sys.argv
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    with open(os.path.join(build, "compile_commands.json")) as f:
        db = json.load(f)
    juce = juce_flags(db)

    all_sources = sources(root)
    failures, unexpected, missing = [], [], []

    def check(source, scratch):
        """Compiles one source both ways. Returns (source, failure, differs)."""
        entry = [e for e in db if e["file"].endswith("/src/" + source)]
        if not entry:
            return source, ("missing", None), False
        entry = entry[0]
        obj = os.path.join(scratch, source.replace("/", "_") + ".o")
        base = re.sub(r"-o \S+", "-o " + obj, entry["command"])
        digests = []
        for gui in ("0", "1"):
            command = (
                base.replace("-DLE_SW_GUI=0", "-DLE_SW_GUI=" + gui)
                + " -DNDEBUG "
                + " ".join(juce)
            )
            result = subprocess.run(
                command, shell=True, cwd=entry["directory"], capture_output=True
            )
            if result.returncode != 0:
                return source, (gui, result.stderr.decode()), False
            with open(obj, "rb") as f:
                digests.append(hashlib.sha256(f.read()).digest())
        os.remove(obj)
        return source, None, digests[0] != digests[1]

    with tempfile.TemporaryDirectory() as scratch:
        with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
            for source, failure, differs in pool.map(
                lambda s: check(s, scratch), all_sources
            ):
                if failure and failure[0] == "missing":
                    missing.append(source)
                elif failure:
                    failures.append((source, failure[0], failure[1]))
                elif differs and source not in EXPECTED_DEPENDENT:
                    unexpected.append(source)

    for source, gui, error in failures:
        lines = [l for l in error.splitlines() if "error:" in l or "fatal" in l]
        print(f"FAIL {source} does not compile at LE_SW_GUI={gui}")
        for line in lines[:5]:
            print("       " + line)
    for source in unexpected:
        print(f"FAIL {source} is newly configuration-dependent")
        print("       If that is intended, add it to EXPECTED_DEPENDENT and say why.")
        print("       If it is not, it will work in debug and corrupt memory in release.")
    for source in missing:
        print(f"WARN {source} has no compile command; configure the build first")

    if failures or unexpected:
        return 1
    if not quiet:
        print(
            f"LE_SW_GUI parity: {len(all_sources)} sources compile both ways, "
            f"{len(EXPECTED_DEPENDENT)} configuration-dependent as expected"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
