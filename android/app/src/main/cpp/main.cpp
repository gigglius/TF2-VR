#include <android/log.h>
#include <android_native_app_glue.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstring>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "TF2VR", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "TF2VR", __VA_ARGS__)

namespace {
struct App {
    android_app* android = nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrSpace localSpace = XR_NULL_HANDLE;
    XrSpace viewSpace = XR_NULL_HANDLE;
    XrActionSet actionSet = XR_NULL_HANDLE;
    XrAction leftGripPose = XR_NULL_HANDLE;
    XrAction rightGripPose = XR_NULL_HANDLE;
    XrSpace leftGripSpace = XR_NULL_HANDLE;
    XrSpace rightGripSpace = XR_NULL_HANDLE;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    std::vector<XrViewConfigurationView> viewConfig;
    std::vector<XrView> views;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    bool resumed = false;
};

bool xr_ok(XrResult result, const char* what) {
    if (XR_FAILED(result)) {
        LOGE("%s failed: %d", what, result);
        return false;
    }
    return true;
}

bool vk_ok(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        LOGE("%s failed: %d", what, result);
        return false;
    }
    return true;
}

bool init_loader(App& a) {
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (!xr_ok(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)),
            "xrGetInstanceProcAddr(xrInitializeLoaderKHR)")) return false;

    if (initializeLoader) {
        XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInfo.applicationVM = a.android->activity->vm;
        loaderInfo.applicationContext = a.android->activity->clazz;
        if (!xr_ok(initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)),
                   "xrInitializeLoaderKHR")) return false;
    }
    return true;
}

bool create_vulkan(App& a) {
    PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getGraphicsDevice = nullptr;
    PFN_xrCreateVulkanInstanceKHR createXrVkInstance = nullptr;
    PFN_xrCreateVulkanDeviceKHR createXrVkDevice = nullptr;

    if (!xr_ok(xrGetInstanceProcAddr(a.instance, "xrGetVulkanGraphicsRequirements2KHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)), "load Vulkan requirements") ||
        !xr_ok(xrGetInstanceProcAddr(a.instance, "xrGetVulkanGraphicsDevice2KHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&getGraphicsDevice)), "load Vulkan device query") ||
        !xr_ok(xrGetInstanceProcAddr(a.instance, "xrCreateVulkanInstanceKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&createXrVkInstance)), "load Vulkan instance creator") ||
        !xr_ok(xrGetInstanceProcAddr(a.instance, "xrCreateVulkanDeviceKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&createXrVkDevice)), "load Vulkan device creator")) return false;

    XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if (!xr_ok(getRequirements(a.instance, &requirements), "query Vulkan requirements")) return false;

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "TF2 VR";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "TF2-VR";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* instanceExtensions[] = {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = 0;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

    XrVulkanInstanceCreateInfoKHR xrInstanceInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrInstanceInfo.systemId = XR_NULL_PATH;
    xrInstanceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrInstanceInfo.vulkanCreateInfo = &instanceInfo;
    VkResult vkResult = VK_SUCCESS;
    if (!xr_ok(createXrVkInstance(a.instance, &xrInstanceInfo, &a.vkInstance, &vkResult),
               "xrCreateVulkanInstanceKHR") || !vk_ok(vkResult, "vkCreateInstance")) return false;

    XrVulkanGraphicsDeviceGetInfoKHR deviceInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceInfo.systemId = XR_NULL_PATH;
    deviceInfo.vulkanInstance = a.vkInstance;
    if (!xr_ok(getGraphicsDevice(a.instance, &deviceInfo, &a.physicalDevice),
               "xrGetVulkanGraphicsDevice2KHR")) return false;

    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(a.physicalDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(a.physicalDevice, &queueCount, queues.data());
    bool found = false;
    for (uint32_t i = 0; i < queueCount; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            a.graphicsQueueFamily = i;
            found = true;
            break;
        }
    }
    if (!found) {
        LOGE("No Vulkan graphics queue found");
        return false;
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = a.graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreate{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreate.queueCreateInfoCount = 1;
    deviceCreate.pQueueCreateInfos = &queueInfo;
    deviceCreate.enabledExtensionCount = 1;
    deviceCreate.ppEnabledExtensionNames = deviceExtensions;

    XrVulkanDeviceCreateInfoKHR xrDeviceInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrDeviceInfo.systemId = XR_NULL_PATH;
    xrDeviceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrDeviceInfo.vulkanPhysicalDevice = a.physicalDevice;
    xrDeviceInfo.vulkanCreateInfo = &deviceCreate;
    if (!xr_ok(createXrVkDevice(a.instance, &xrDeviceInfo, &a.device, &vkResult),
               "xrCreateVulkanDeviceKHR") || !vk_ok(vkResult, "vkCreateDevice")) return false;

    vkGetDeviceQueue(a.device, a.graphicsQueueFamily, 0, &a.graphicsQueue);
    LOGI("Vulkan device and graphics queue created");
    return true;
}

bool create_actions(App& a) {
    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    if (!xr_ok(xrCreateActionSet(a.instance, &setInfo, &a.actionSet), "create action set")) return false;

    XrPath leftPath = XR_NULL_PATH, rightPath = XR_NULL_PATH;
    xrStringToPath(a.instance, "/user/hand/left", &leftPath);
    xrStringToPath(a.instance, "/user/hand/right", &rightPath);

    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    actionInfo.countSubactionPaths = 2;
    XrPath hands[] = {leftPath, rightPath};
    actionInfo.subactionPaths = hands;
    std::strncpy(actionInfo.actionName, "grip_pose", XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(actionInfo.localizedActionName, "Grip Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    if (!xr_ok(xrCreateAction(a.actionSet, &actionInfo, &a.leftGripPose), "create grip pose action")) return false;
    a.rightGripPose = a.leftGripPose;

    XrInteractionProfileSuggestedBinding bindings{};
    bindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
    xrStringToPath(a.instance, "/interaction_profiles/oculus/touch_controller", &bindings.interactionProfile);
    XrPath leftPose = XR_NULL_PATH, rightPose = XR_NULL_PATH;
    xrStringToPath(a.instance, "/user/hand/left/input/grip/pose", &leftPose);
    xrStringToPath(a.instance, "/user/hand/right/input/grip/pose", &rightPose);
    XrActionSuggestedBinding suggested[] = {{a.leftGripPose, leftPose}, {a.leftGripPose, rightPose}};
    bindings.countSuggestedBindings = 2;
    bindings.suggestedBindings = suggested;
    if (!xr_ok(xrSuggestInteractionProfileBindings(a.instance, &bindings), "suggest Touch bindings")) return false;

    XrActionSpaceCreateInfo spaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    spaceInfo.action = a.leftGripPose;
    spaceInfo.poseInActionSpace.orientation.w = 1.0f;
    spaceInfo.subactionPath = leftPath;
    if (!xr_ok(xrCreateActionSpace(a.session, &spaceInfo, &a.leftGripSpace), "create left grip space")) return false;
    spaceInfo.subactionPath = rightPath;
    if (!xr_ok(xrCreateActionSpace(a.session, &spaceInfo, &a.rightGripSpace), "create right grip space")) return false;

    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets = &a.actionSet;
    return xr_ok(xrAttachSessionActionSets(a.session, &attach), "attach action set");
}

bool create_session(App& a) {
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    if (!xr_ok(xrGetSystem(a.instance, &systemInfo, &systemId), "get HMD system")) return false;

    if (!create_vulkan(a)) return false;

    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = a.vkInstance;
    binding.physicalDevice = a.physicalDevice;
    binding.device = a.device;
    binding.queueFamilyIndex = a.graphicsQueueFamily;
    binding.queueIndex = 0;

    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = systemId;
    if (!xr_ok(xrCreateSession(a.instance, &sessionInfo, &a.session), "create OpenXR session")) return false;

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    if (!xr_ok(xrCreateReferenceSpace(a.session, &spaceInfo, &a.localSpace), "create local space")) return false;
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if (!xr_ok(xrCreateReferenceSpace(a.session, &spaceInfo, &a.viewSpace), "create view space")) return false;

    uint32_t count = 0;
    if (!xr_ok(xrEnumerateViewConfigurationViews(a.instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr), "count stereo views")) return false;
    a.viewConfig.resize(count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    a.views.resize(count, {XR_TYPE_VIEW});
    if (!xr_ok(xrEnumerateViewConfigurationViews(a.instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, a.viewConfig.data()), "enumerate stereo views")) return false;

    if (!create_actions(a)) return false;
    LOGI("OpenXR session ready; stereo views: %u", count);
    return true;
}

bool create_instance(App& a) {
    if (!init_loader(a)) return false;

    const char* extensions[] = {
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
        XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
    };
    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = a.android->activity->vm;
    androidInfo.applicationActivity = a.android->activity->clazz;

    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    info.next = &androidInfo;
    std::strncpy(info.applicationInfo.applicationName, "TF2 VR", XR_MAX_APPLICATION_NAME_SIZE - 1);
    info.applicationInfo.applicationVersion = 1;
    std::strncpy(info.applicationInfo.engineName, "TF2-VR", XR_MAX_ENGINE_NAME_SIZE - 1);
    info.applicationInfo.engineVersion = 1;
    info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    info.enabledExtensionCount = 2;
    info.enabledExtensionNames = extensions;
    if (!xr_ok(xrCreateInstance(&info, &a.instance), "create OpenXR instance")) return false;
    return create_session(a);
}

void poll_events(App& a) {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(a.instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* changed = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
            a.sessionState = changed->state;
            if (changed->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                a.sessionRunning = xr_ok(xrBeginSession(a.session, &begin), "begin session");
            } else if (changed->state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(a.session);
                a.sessionRunning = false;
            } else if (changed->state == XR_SESSION_STATE_EXITING || changed->state == XR_SESSION_STATE_LOSS_PENDING) {
                a.sessionRunning = false;
            }
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void frame(App& a) {
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(a.session, &waitInfo, &frameState))) return;
    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(a.session, &beginInfo))) return;

    XrViewState viewState{XR_TYPE_VIEW_STATE};
    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime = frameState.predictedDisplayTime;
    locate.space = a.localSpace;
    uint32_t viewCount = 0;
    if (XR_SUCCEEDED(xrLocateViews(a.session, &locate, &viewState,
            static_cast<uint32_t>(a.views.size()), &viewCount, a.views.data()))) {
        // Headset position/orientation for each eye is now available in a.views.
        // Next renderer step consumes these poses and submits Vulkan swapchain layers.
    }

    XrActiveActionSet active{a.actionSet, XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    xrSyncActions(a.session, &sync);

    XrSpaceLocation left{XR_TYPE_SPACE_LOCATION}, right{XR_TYPE_SPACE_LOCATION};
    xrLocateSpace(a.leftGripSpace, a.localSpace, frameState.predictedDisplayTime, &left);
    xrLocateSpace(a.rightGripSpace, a.localSpace, frameState.predictedDisplayTime, &right);
    const bool leftTracked = (left.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0;
    const bool rightTracked = (right.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0;
    (void)leftTracked;
    (void)rightTracked;
    // Left/right grip poses are ready for the weapon/hand layer.

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 0;
    endInfo.layers = nullptr;
    xrEndFrame(a.session, &endInfo);
}

void shutdown(App& a) {
    if (a.leftGripSpace) xrDestroySpace(a.leftGripSpace);
    if (a.rightGripSpace) xrDestroySpace(a.rightGripSpace);
    if (a.viewSpace) xrDestroySpace(a.viewSpace);
    if (a.localSpace) xrDestroySpace(a.localSpace);
    if (a.actionSet) xrDestroyActionSet(a.actionSet);
    if (a.session) xrDestroySession(a.session);
    if (a.instance) xrDestroyInstance(a.instance);
    if (a.device) vkDestroyDevice(a.device, nullptr);
    if (a.vkInstance) vkDestroyInstance(a.vkInstance, nullptr);
}
}

void android_main(android_app* state) {
    app_dummy();
    App app;
    app.android = state;
    state->userData = &app;
    LOGI("TF2-VR Android ARM64 native entry");

    bool initialized = false;
    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        while (ALooper_pollOnce(app.sessionRunning ? 0 : -1, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                shutdown(app);
                return;
            }
            if (state->activityState == APP_CMD_RESUME && !initialized) {
                initialized = create_instance(app);
                if (!initialized) LOGE("OpenXR startup failed; inspect device logcat");
            }
        }
        if (initialized) {
            poll_events(app);
            if (app.sessionRunning) frame(app);
        }
    }
}
