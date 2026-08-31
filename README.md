# SpectrumWorx

SpectrumWorx is an awesome spectral effect originally developed and released by Little Endian,
where development ended in 2016 and the plugin was open sourced in 2024. You can find
the original source code dump here: https://github.com/LittleEndianLtd/SpectrumWorx

As with all folks who have decided to open source great commercial products at the end
of their development, we are very grateful to Little Endian for making this decision.

In 2026, when a KvR thread brought this to our attention, we grabbed it and started modernizing.
This involved heavy use of Claude Opus 5 and Fable 5 to port the product to modern standards,
including:

- Moving from VST2 Windows and macOS only to CLAP, clap-wrapper for Windows, macOS and Linux
- Setting up reliable GitHub action pipelines and binary builds
- Modernizing the code, including removing old libraries (JUCE 2, Boost...)
- Making substantial improvements to the threading and ownership model
- Adding tests to cover the engine
- Vectorizing the skin
- Inferring technical documentation

That was a heavy three weeks of plan/iterate/generate/test/repeat cycle using machine tools almost entirely
to generate the ported code, while preserving the DSP code and operating model.

Right now, this is a bit of a work-in-prgoress as we figure out if we can move it from
a two week sprint to an official 3.0 release from the team.


## Building from Source

As a prerequisite

- You need `git`, CMake 3.28 or newer, and a C++20 toolchain — clang or gcc on
macOS and Linux, Visual Studio 2022 on Windows.
- On Linux you also need the usual JUCE development packages; X11, freetype and fontconfig
are core ones. You can see the list we use in CI on ubuntu [here](https://github.com/surge-synthesizer/sst-githubactions/blob/5bb92ec9d3401eba6a85f8edea98395b3b866e84/prepare-for-juce/action.yml#L22)
- Everything else is a submodule or is fetched by CMake at configure time. 

With those resolved you can clone and build. 

```bash
git clone https://github.com/surge-synthesizer/SpectrumWorx.git
cd SpectrumWorx
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target spectrumworx_clapfirst_all --parallel
```

That leaves all four formats in `build/sw_assets`:

```
build/sw_assets/SpectrumWorx.clap
build/sw_assets/SpectrumWorx.vst3
build/sw_assets/SpectrumWorx.component    # macOS only
build/sw_assets/SpectrumWorx[.ext]        # the standalone; .app on macOS, .exe on Windows, no ext on linux
```

Windows puts each of them in a per-format subdirectory of `sw_assets` —
`CLAP/`, `VST3/`, `Standalone-…` — rather than side by side.


On Mac and Linux you can also have cmake install the plugin locally at build time by having the 
first cmake add the `SW_COPY_AFTER_BUILD` flag

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSW_COPY_AFTER_BUILD=ON
```

which installs into `~/Library/Audio/Plug-Ins/{CLAP,VST3,Components}` on macOS and
`~/.clap` and `~/.vst3` on Linux after every build. It does nothing on Windows.

### The other targets, and the options worth knowing

- **The default target** — `cmake --build build` with no `--target` — builds the
  bundles *and* the test binaries and `sw-show-ui` alongside them.
- `ctest--test-dir build` runs the test suite once built. This is our CI gate.
- **`--target spectrumworx-installer`** builds the platform installer into
  `build/installer`. That is what a release is cut with; on Windows it wants
  Inno Setup on `PATH` at *configure* time.
- **`-DSW_BUILD_CLAP_ONLY=ON`** builds the `.clap` and nothing else. Worth it if
  you only want something a CLAP host can load: the other formats and the
  standalone CPM-fetch 54MB of SDKs at configure time and this skips all of it.
- **`-DSW_BUILD_TESTS=OFF -DSW_BUILD_TOOLS=OFF`** leaves the test binaries and
  the tools out of the build entirely.
- **`-DSW_WERROR=OFF`** at configure time will skip `-Werror` which is useful if using a new
  compiler or compiler version.

## A Note About Coding Assistants in This Project.

In Surge Synth Team for our headline properties (Surge XT, Shortcircuit XT, OB-Xf and the various
SST libraries) we have adopted a coding assistant policy which mostly mirrors the Linux kernel policy.
Basically: use them if you want, review the code, hold the person committing the pull request accountable,
don't commit the code you can't vouch for line by line.

Partly as an experiment and partly out of necessity with the pre-modern state of SpectrumWorx,
we took a different approach, relying heavily on the frontier models to make
extensive changes to the software, generating plans and tests as we went along,
using heavy agent flows with autonomous decisions at line-of-code time, reviewing and
comparing multiple models (alternating between various Opus and Fable levels).
This led to us being able to modernize this code remarkably quickly (this would have taken at least one year otherwise!),
and taught us a lot about the reach and limits of frontier models in audio software.
But, since it is different from the policy and approach we use in our other properties,
we wanted to remain transparent and open about it.

## Licence ⚖️
The source in this repository is **GPL-3.0-or-later**. A released binary links
JUCE 8 under its AGPLv3 arm, so the plugin you can download is
**AGPL-3.0-or-later**. [LICENSING.md](./LICENSING.md) has the reasoning, the
per-dependency table and what it means if you hold a commercial JUCE licence.
