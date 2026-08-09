# SpectrumWorx - The Surge Synth Team Port 

SpectrumWorx is an awesome spectral effect originally released by LittleEndianLtd,
where development ended in 2016 and the plugin was open sourced in 2024. You can find
the open source original code dump here: https://github.com/LittleEndianLtd/SpectrumWorx

As with all folks who have decided to open source great commercial products at the end
of their development, we are very grateful to LittleEndian for making this decision.

In 2026, when a KVR thread bought this to our attention, we grabbed it and started modernizing.
This involved heavy use of Claude Opus 5 and Fable 5 to port the product to a modern standard
including

- Moving from VST2 Windows and Mac only to CLAP + Clap Wrapper in Win, Mac and Linux
- Setting up full reliably github action pipelines and binary builds
- Modernizing the code including removing old libraries (Juce 2, Boost)
- Making substantial improvements to the threading and ownership model 
- Add tests to cover the engine
- Move to an SVG/Vector skin
- Infer technical documentation

That was a heavy 2 weeks of plan/iterate/generate/test/repeat using machine tools almost entirely
to generate the ported code, while preserving the DSP code and operationg model

Anyway so now this is a bit of a work-in-prgoress as we figure out if we can move it from
a 2 week sprint to a '3.0' release from the team.

![img](https://github.com/LittleEndianLtd/SpectrumWorx/blob/main/doc/All_windows_numbered.png)


## Licence ⚖️
The source in this repository is **GPL-3.0-or-later**. A released binary links
JUCE 8 under its AGPLv3 arm, so the plugin you can download is
**AGPL-3.0-or-later**. [LICENSING.md](./LICENSING.md) has the reasoning, the
per-dependency table and what it means if you hold a commercial JUCE licence.
