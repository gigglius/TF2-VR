# TF2-VR — Team Fortress 2 for Meta Quest 3

A research and development project exploring a standalone Android/ARM64 and OpenXR port of modern Team Fortress 2 for Meta Quest 3.

**Goal:** preserve the modern TF2 game experience—nine classes, weapons, maps, movement, and multiplayer—while adding native VR controls and stereoscopic rendering. This is an ambitious engine-porting project, not a Unity remake.

## Status

Android/OpenXR bring-up source is in progress on [`quest3/openxr-foundation`](https://github.com/gigglius/TF2-VR/tree/quest3/openxr-foundation). The branch now contains an Android ARM64 NativeActivity, OpenXR session setup, Vulkan per-eye swapchains, a calibration-pattern projection layer, and Touch grip-pose sampling. It has **not yet been compiled or verified on a physical Quest 3**, and no APK is available yet.

## Source foundation

- [Valve's Source SDK 2013](https://github.com/ValveSoftware/source-sdk-2013) now includes TF2 client and server game code.
- The SDK is not a complete, ready-to-build Android engine. Its published build instructions target Windows and Linux/Steam Runtime. A Quest port will require substantial platform, renderer, input, audio, filesystem, and networking work.
- Game content and binaries are not included here. Use content obtained through legitimate channels and do not commit proprietary TF2 assets, Steam credentials, or copyrighted game files.

## Target

- Device: Meta Quest 3 (standalone)
- OS / ABI: Android, arm64-v8a
- XR: OpenXR
- Rendering: native stereoscopic VR, with performance profiling for Quest
- Input: tracked Touch controllers, headset tracking, VR locomotion and weapon interaction
- Game: modern TF2 code and compatible game content, subject to Valve's SDK license and applicable game terms

## Milestones

1. **Source audit:** map TF2 game-code dependencies to engine, platform, and Steam services; document build blockers.
2. **Android shell:** create a minimal Android ARM64 app with native C++ and OpenXR session lifecycle.
3. **Engine feasibility:** establish a reproducible Android toolchain and compile isolated engine dependencies.
4. **First VR scene:** complete and device-verify the Vulkan stereo calibration scene and head/controller tracking on Quest 3.
5. **Game integration:** connect the Source engine/game layer, filesystem and authorized game content.
6. **Gameplay:** implement VR weapon handling, locomotion, HUD interaction, and comfort settings.
7. **Multiplayer:** validate networking, server compatibility, and platform-service constraints.
8. **Optimization and release:** profile on-device, package APK, test install/update flow, and publish build instructions.

## Repository layout (planned)

- `docs/` — architecture, dependency audit, and milestone notes
- `android/` — Android application and native OpenXR/Vulkan platform layer (see [build and Quest test notes](android/README.md))
- `native/` — platform-neutral VR and engine integration code
- `.github/workflows/` — validation and build automation

## Development principles

- Keep upstream Valve code separate from project-specific patches where practical.
- Do not claim an APK/build works until it has been built and tested on-device.
- Keep secrets and proprietary game files out of Git.
- Record exact upstream revisions and reproducible build steps.

## License

This project must comply with the [Source 1 SDK License](https://github.com/ValveSoftware/source-sdk-2013/blob/master/LICENSE), Valve's applicable game terms, and the licenses of all third-party dependencies. Valve's SDK terms are non-commercial; review them before distributing source, binaries, or a mod. This repository does not grant rights to TF2 assets or trademarks.
