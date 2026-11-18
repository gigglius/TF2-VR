# Quest 3 Android/OpenXR foundation

This is the native ARM64 platform layer for the TF2-VR Quest 3 project.

## Current implementation

- Android NativeActivity entry point and ARM64-only Gradle configuration.
- Android OpenXR loader initialization using the Java VM and Activity context.
- OpenXR HMD system, session, local/view reference spaces, and stereo view discovery.
- Vulkan instance/device creation through `XR_KHR_vulkan_enable2`.
- OpenXR action set with left/right Oculus Touch grip-pose bindings.
- Per-frame head-view and controller-space pose location.

## Important status

This is an early platform foundation, **not yet a complete rendered VR demo**. The frame loop currently locates stereo eye views and Touch grip poses but submits no projection layer. The next implementation task is to create the OpenXR Vulkan swapchain, render pass/framebuffers, and submit both eye images. It has not been built or tested on a Quest 3 yet.

## OpenXR SDK prerequisite

Install/extract a Khronos OpenXR SDK package that includes Android ARM64 headers and loader, or use the Meta OpenXR Mobile SDK's compatible OpenXR headers/loader. Set one of:

- Gradle property: `-PopenxrSdkRoot=/absolute/path/to/OpenXR-SDK`
- Environment variable: `OPENXR_SDK_ROOT=/absolute/path/to/OpenXR-SDK`

The expected loader path is:

```
$OPENXR_SDK_ROOT/Libs/Android/arm64-v8a/Release/libopenxr_loader.so
```

The CMake configuration also accepts headers under `Include/openxr` or `OpenXR/Include/openxr`. SDK layouts vary; adjust the paths if your package uses a different layout. Do not commit SDK binaries or proprietary game assets.

## Build

Open `android/` in Android Studio with Android SDK 35, NDK, and CMake 3.22.1 installed, then configure the OpenXR SDK path above and build the `app` debug variant. The project intentionally filters to `arm64-v8a`.

Before calling this milestone complete:
1. Resolve any SDK-layout/build issues on the actual toolchain.
2. Add Vulkan swapchain rendering and submit a stereo projection layer.
3. Verify head pose and both Touch grip poses on physical Quest 3.
4. Test pause/resume, session loss, and clean shutdown.
