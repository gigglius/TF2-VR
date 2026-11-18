# Quest 3 Android/OpenXR stereo renderer

This is the native ARM64 platform layer for the TF2-VR Quest 3 project.

## Implemented source

- Android NativeActivity entry point and ARM64-only Gradle configuration.
- Android OpenXR loader initialization using the Java VM and Activity context.
- OpenXR HMD system, session, local/view reference spaces, and stereo view discovery.
- Vulkan instance/device creation through \`XR_KHR_vulkan_enable2\`.
- One OpenXR Vulkan color swapchain per eye, with Vulkan image views and framebuffers.
- A Vulkan render pass that draws a visible calibration pattern into both eye images.
- Stereo \`XrCompositionLayerProjection\` submission using the runtime's per-eye poses and FOVs.
- Oculus Touch left/right grip-pose actions, per-frame pose sampling, and tracking diagnostics in logcat.

## Status and limitations

This is an implementation milestone in source, **not a verified APK**. The current GPU test scene is a simple calibration pattern made from Vulkan clear operations; it is not yet the 3D TF2 scene or weapon/hand models. The project has not been built on a Quest 3 in this environment, so compile/runtime compatibility still needs device validation.

## OpenXR SDK prerequisite

Install/extract a Khronos OpenXR SDK package or Meta OpenXR Mobile SDK containing Android ARM64 OpenXR headers and loader. Set either:

- Gradle property: \`-PopenxrSdkRoot=/absolute/path/to/OpenXR-SDK\`
- Environment variable: \`OPENXR_SDK_ROOT=/absolute/path/to/OpenXR-SDK\`

The CMake script checks common SDK layouts for the \`openxr/openxr.h\` headers and \`libopenxr_loader.so\`. SDK package layouts vary. Do not commit SDK binaries or proprietary TF2 game assets.

## Build

Open \`android/\` in Android Studio with Android SDK 35, Android NDK, and CMake 3.22.1 installed. Configure the OpenXR SDK root and build the \`app\` debug variant. The app is filtered to \`arm64-v8a\`.

## Quest 3 validation checklist

1. Install the debug APK on a Quest 3 with Developer Mode enabled.
2. Launch it from the headset. The calibration pattern should appear in both eyes.
3. Move and rotate your head; the view should update with the headset pose.
4. Move both Touch controllers; logcat should report both controller poses when tracking is valid.
5. Test app pause/resume and verify the OpenXR session returns cleanly.

Useful logcat filter: \`adb logcat -s TF2VR\`.

The headset/controller checks above are required before this milestone can be marked device-verified. No physical Quest 3 test has been run yet.
