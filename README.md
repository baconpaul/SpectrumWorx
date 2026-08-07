# SpectrumWorx 🔊
SpectrumWorx - the ultimate sound mangler (VST/AU).

![img](https://github.com/LittleEndianLtd/SpectrumWorx/blob/main/doc/All_windows_numbered.png)

## Current state? 🔮
Ported. The 2016 tree did not compile; this one builds CLAP, VST3, AUv2 and a
standalone on every push — macOS universal, Windows x64, Linux x64 — and runs in
DAWs on all three. A push to `main` produces a signed and notarised installer.

[`doc/tech/`](./doc/tech/) describes how it works and what is left to do.
## Where to start? 🚀
- [doc/tech/README.md](./doc/tech/README.md) — how the engine, the parameters,
  the preset format and the threading work, plus what is left to do.

## Licence ⚖️
The source in this repository is **GPL-3.0-or-later**. A released binary links
JUCE 8 under its AGPLv3 arm, so the plugin you can download is
**AGPL-3.0-or-later**. [LICENSING.md](./LICENSING.md) has the reasoning, the
per-dependency table and what it means if you hold a commercial JUCE licence.
