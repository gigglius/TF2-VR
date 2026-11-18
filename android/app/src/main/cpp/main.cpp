#include <android/log.h>
#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "TF2VR", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "TF2VR", __VA_ARGS__)

namespace {
struct EyeTarget {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<XrSwapchainImageVulkan2KHR> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
};

struct App {
    android_app* android = nullptr;
    bool resumed = false;
    bool initialized = false;

    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace localSpace = XR_NULL_HANDLE;
    XrSpace viewSpace = XR_NULL_HANDLE;
    XrActionSet actionSet = XR_NULL_HANDLE;
    XrAction gripPoseAction = XR_NULL_HANDLE;
    XrAction hapticAction = XR_NULL_HANDLE;
    XrPath leftHandPath = XR_NULL_PATH;
    XrPath rightHandPath = XR_NULL_PATH;
    XrSpace leftGripSpace = XR_NULL_HANDLE;
    XrSpace rightGripSpace = XR_NULL_HANDLE;

    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    std::vector<EyeTarget> eyes;
    std::vector<XrViewConfigurationView> viewConfig;
    std::vector<XrView> views;
    std::vector<XrCompositionLayerProjectionView> projectionViews;

    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    bool stereoReady = false;
    bool leftControllerTracked = false;
    bool rightControllerTracked = false;
    bool trackingStatusLogged = false;
    bool controllerStatusLogged = false;
    bool leftHapticTestSent = false;
    bool rightHapticTestSent = false;
    XrPosef leftControllerPose{};
    XrPosef rightControllerPose{};
};

bool xr_ok(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        LOGE("%s failed: %d", operation, result);
        return false;
    }
    return true;
}

bool vk_ok(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        LOGE("%s failed: %d", operation, result);
        return false;
    }
    return true;
}

void handle_app_command(android_app* state, int32_t command) {
    auto* app = static_cast<App*>(state->userData);
    if (!app) return;
    if (command == APP_CMD_RESUME) app->resumed = true;
    if (command == APP_CMD_PAUSE) app->resumed = false;
}

bool initialize_openxr_loader(App& app) {
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (!xr_ok(xrGetInstanceProcAddr(
            XR_NULL_HANDLE, "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)),
            "load xrInitializeLoaderKHR")) return false;

    if (!initializeLoader) {
        LOGE("OpenXR loader does not expose xrInitializeLoaderKHR");
        return false;
    }

    XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInfo.applicationVM = app.android->activity->vm;
    loaderInfo.applicationContext = app.android->activity->clazz;
    return xr_ok(initializeLoader(
        reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)),
        "initialize Android OpenXR loader");
}

bool create_vulkan(App& app) {
    PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getGraphicsDevice = nullptr;
    PFN_xrCreateVulkanInstanceKHR createXrVulkanInstance = nullptr;
    PFN_xrCreateVulkanDeviceKHR createXrVulkanDevice = nullptr;

    if (!xr_ok(xrGetInstanceProcAddr(app.instance, "xrGetVulkanGraphicsRequirements2KHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)), "load Vulkan requirements") ||
        !xr_ok(xrGetInstanceProcAddr(app.instance, "xrGetVulkanGraphicsDevice2KHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getGraphicsDevice)), "load Vulkan graphics device") ||
        !xr_ok(xrGetInstanceProcAddr(app.instance, "xrCreateVulkanInstanceKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&createXrVulkanInstance)), "load Vulkan instance creator") ||
        !xr_ok(xrGetInstanceProcAddr(app.instance, "xrCreateVulkanDeviceKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&createXrVulkanDevice)), "load Vulkan device creator")) {
        return false;
    }

    XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if (!xr_ok(getRequirements(app.instance, app.systemId, &requirements), "query OpenXR Vulkan requirements")) return false;

    const uint32_t requestedApi = VK_API_VERSION_1_1;
    const XrVersion requestedXrVersion = XR_MAKE_VERSION(1, 1, 0);
    if (requestedXrVersion < requirements.minApiVersionSupported ||
        requestedXrVersion > requirements.maxApiVersionSupported) {
        LOGE("Vulkan 1.1 is outside the runtime-supported Vulkan version range");
        return false;
    }

    VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "TF2 VR";
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName = "TF2-VR";
    applicationInfo.engineVersion = 1;
    applicationInfo.apiVersion = requestedApi;

    VkInstanceCreateInfo instanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pApplicationInfo = &applicationInfo;

    XrVulkanInstanceCreateInfoKHR xrInstanceInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrInstanceInfo.systemId = app.systemId;
    xrInstanceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrInstanceInfo.vulkanCreateInfo = &instanceCreateInfo;

    VkResult vulkanResult = VK_SUCCESS;
    if (!xr_ok(createXrVulkanInstance(app.instance, &xrInstanceInfo,
                &app.vkInstance, &vulkanResult), "xrCreateVulkanInstanceKHR") ||
        !vk_ok(vulkanResult, "create Vulkan instance")) return false;

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = app.systemId;
    deviceGetInfo.vulkanInstance = app.vkInstance;
    if (!xr_ok(getGraphicsDevice(app.instance, &deviceGetInfo, &app.physicalDevice),
               "xrGetVulkanGraphicsDevice2KHR")) return false;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphicsQueue = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            app.graphicsQueueFamily = i;
            foundGraphicsQueue = true;
            break;
        }
    }
    if (!foundGraphicsQueue) {
        LOGE("No Vulkan graphics queue family found");
        return false;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = app.graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    XrVulkanDeviceCreateInfoKHR xrDeviceInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrDeviceInfo.systemId = app.systemId;
    xrDeviceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrDeviceInfo.vulkanPhysicalDevice = app.physicalDevice;
    xrDeviceInfo.vulkanCreateInfo = &deviceCreateInfo;

    if (!xr_ok(createXrVulkanDevice(app.instance, &xrDeviceInfo, &app.device, &vulkanResult),
               "xrCreateVulkanDeviceKHR") ||
        !vk_ok(vulkanResult, "create Vulkan device")) return false;

    vkGetDeviceQueue(app.device, app.graphicsQueueFamily, 0, &app.graphicsQueue);
    LOGI("Vulkan device ready");
    return true;
}

bool create_render_resources(App& app, VkFormat colorFormat) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    if (!vk_ok(vkCreateRenderPass(app.device, &renderPassInfo, nullptr, &app.renderPass),
               "create eye render pass")) return false;

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = app.graphicsQueueFamily;
    if (!vk_ok(vkCreateCommandPool(app.device, &poolInfo, nullptr, &app.commandPool),
               "create render command pool")) return false;

    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = app.commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = 1;
    if (!vk_ok(vkAllocateCommandBuffers(app.device, &commandInfo, &app.commandBuffer),
               "allocate render command buffer")) return false;

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vk_ok(vkCreateFence(app.device, &fenceInfo, nullptr, &app.renderFence),
                 "create render fence");
}

bool create_eye_swapchains(App& app) {
    uint32_t formatCount = 0;
    if (!xr_ok(xrEnumerateSwapchainFormats(app.session, 0, &formatCount, nullptr),
               "count swapchain formats")) return false;
    std::vector<int64_t> formats(formatCount);
    if (!xr_ok(xrEnumerateSwapchainFormats(app.session, formatCount, &formatCount, formats.data()),
               "enumerate swapchain formats")) return false;

    VkFormat chosenFormat = VK_FORMAT_UNDEFINED;
    const VkFormat preferred[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM
    };
    for (VkFormat candidate : preferred) {
        if (std::find(formats.begin(), formats.end(), static_cast<int64_t>(candidate)) != formats.end()) {
            chosenFormat = candidate;
            break;
        }
    }
    if (chosenFormat == VK_FORMAT_UNDEFINED) {
        LOGE("Runtime exposes no supported 8-bit RGBA/BGRA swapchain format");
        return false;
    }

    if (!create_render_resources(app, chosenFormat)) return false;
    app.eyes.resize(app.viewConfig.size());

    for (uint32_t eyeIndex = 0; eyeIndex < app.viewConfig.size(); ++eyeIndex) {
        EyeTarget& eye = app.eyes[eyeIndex];
        eye.width = app.viewConfig[eyeIndex].recommendedImageRectWidth;
        eye.height = app.viewConfig[eyeIndex].recommendedImageRectHeight;

        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.format = chosenFormat;
        swapchainInfo.sampleCount = 1;
        swapchainInfo.width = eye.width;
        swapchainInfo.height = eye.height;
        swapchainInfo.faceCount = 1;
        swapchainInfo.arraySize = 1;
        swapchainInfo.mipCount = 1;
        if (!xr_ok(xrCreateSwapchain(app.session, &swapchainInfo, &eye.swapchain),
                   "create eye swapchain")) return false;

        uint32_t imageCount = 0;
        if (!xr_ok(xrEnumerateSwapchainImages(eye.swapchain, 0, &imageCount, nullptr),
                   "count eye swapchain images")) return false;
        eye.images.resize(imageCount);
        for (auto& image : eye.images) image = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR};
        if (!xr_ok(xrEnumerateSwapchainImages(
                eye.swapchain, imageCount, &imageCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),
                "enumerate Vulkan swapchain images")) return false;

        eye.imageViews.resize(imageCount, VK_NULL_HANDLE);
        eye.framebuffers.resize(imageCount, VK_NULL_HANDLE);
        for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = eye.images[imageIndex].image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = chosenFormat;
            viewInfo.components = {
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
            };
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            if (!vk_ok(vkCreateImageView(app.device, &viewInfo, nullptr, &eye.imageViews[imageIndex]),
                       "create swapchain image view")) return false;

            VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = app.renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &eye.imageViews[imageIndex];
            framebufferInfo.width = eye.width;
            framebufferInfo.height = eye.height;
            framebufferInfo.layers = 1;
            if (!vk_ok(vkCreateFramebuffer(app.device, &framebufferInfo, nullptr,
                                           &eye.framebuffers[imageIndex]),
                       "create eye framebuffer")) return false;
        }
    }

    app.projectionViews.resize(app.eyes.size());
    app.stereoReady = app.eyes.size() == 2;
    LOGI("Created %zu eye swapchains", app.eyes.size());
    return app.stereoReady;
}

bool create_actions(App& app) {
    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    if (!xr_ok(xrCreateActionSet(app.instance, &setInfo, &app.actionSet), "create gameplay action set")) {
        return false;
    }

    if (!xr_ok(xrStringToPath(app.instance, "/user/hand/left", &app.leftHandPath), "create left hand path") ||
        !xr_ok(xrStringToPath(app.instance, "/user/hand/right", &app.rightHandPath), "create right hand path")) {
        return false;
    }

    XrPath handPaths[] = {app.leftHandPath, app.rightHandPath};
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    actionInfo.countSubactionPaths = 2;
    actionInfo.subactionPaths = handPaths;
    std::strncpy(actionInfo.actionName, "grip_pose", XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(actionInfo.localizedActionName, "Grip Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    if (!xr_ok(xrCreateAction(app.actionSet, &actionInfo, &app.gripPoseAction),
               "create Touch grip pose action")) return false;

    XrActionCreateInfo hapticInfo{XR_TYPE_ACTION_CREATE_INFO};
    hapticInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    hapticInfo.countSubactionPaths = 2;
    hapticInfo.subactionPaths = handPaths;
    std::strncpy(hapticInfo.actionName, "hand_haptic", XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(hapticInfo.localizedActionName, "Hand Haptics", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    if (!xr_ok(xrCreateAction(app.actionSet, &hapticInfo, &app.hapticAction),
               "create Touch haptic output action")) return false;

    XrPath interactionProfile = XR_NULL_PATH;
    XrPath leftPosePath = XR_NULL_PATH;
    XrPath rightPosePath = XR_NULL_PATH;
    XrPath leftHapticPath = XR_NULL_PATH;
    XrPath rightHapticPath = XR_NULL_PATH;
    if (!xr_ok(xrStringToPath(app.instance, "/interaction_profiles/oculus/touch_controller", &interactionProfile),
               "create Touch interaction profile path") ||
        !xr_ok(xrStringToPath(app.instance, "/user/hand/left/input/grip/pose", &leftPosePath),
               "create left grip binding path") ||
        !xr_ok(xrStringToPath(app.instance, "/user/hand/right/input/grip/pose", &rightPosePath),
               "create right grip binding path") ||
        !xr_ok(xrStringToPath(app.instance, "/user/hand/left/output/haptic", &leftHapticPath),
               "create left haptic binding path") ||
        !xr_ok(xrStringToPath(app.instance, "/user/hand/right/output/haptic", &rightHapticPath),
               "create right haptic binding path")) return false;

    XrActionSuggestedBinding suggestedBindings[] = {
        {app.gripPoseAction, leftPosePath},
        {app.gripPoseAction, rightPosePath},
        {app.hapticAction, leftHapticPath},
        {app.hapticAction, rightHapticPath}
    };
    XrInteractionProfileSuggestedBinding bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    bindings.interactionProfile = interactionProfile;
    bindings.countSuggestedBindings = 4;
    bindings.suggestedBindings = suggestedBindings;
    if (!xr_ok(xrSuggestInteractionProfileBindings(app.instance, &bindings),
               "suggest Oculus Touch bindings")) return false;

    XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceInfo.action = app.gripPoseAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
    actionSpaceInfo.subactionPath = app.leftHandPath;
    if (!xr_ok(xrCreateActionSpace(app.session, &actionSpaceInfo, &app.leftGripSpace),
               "create left Touch grip space")) return false;
    actionSpaceInfo.subactionPath = app.rightHandPath;
    if (!xr_ok(xrCreateActionSpace(app.session, &actionSpaceInfo, &app.rightGripSpace),
               "create right Touch grip space")) return false;

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &app.actionSet;
    return xr_ok(xrAttachSessionActionSets(app.session, &attachInfo), "attach gameplay action set");
}

bool create_session(App& app) {
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!xr_ok(xrGetSystem(app.instance, &systemInfo, &app.systemId), "get HMD system")) return false;

    if (!create_vulkan(app)) return false;

    XrGraphicsBindingVulkan2KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    graphicsBinding.instance = app.vkInstance;
    graphicsBinding.physicalDevice = app.physicalDevice;
    graphicsBinding.device = app.device;
    graphicsBinding.queueFamilyIndex = app.graphicsQueueFamily;
    graphicsBinding.queueIndex = 0;

    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &graphicsBinding;
    sessionInfo.systemId = app.systemId;
    if (!xr_ok(xrCreateSession(app.instance, &sessionInfo, &app.session), "create OpenXR session")) return false;

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    if (!xr_ok(xrCreateReferenceSpace(app.session, &spaceInfo, &app.localSpace),
               "create local reference space")) return false;
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if (!xr_ok(xrCreateReferenceSpace(app.session, &spaceInfo, &app.viewSpace),
               "create view reference space")) return false;

    uint32_t viewCount = 0;
    if (!xr_ok(xrEnumerateViewConfigurationViews(app.instance, app.systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr),
            "count stereo views")) return false;
    app.viewConfig.resize(viewCount);
    app.views.resize(viewCount);
    for (auto& view : app.viewConfig) view = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    for (auto& view : app.views) view = {XR_TYPE_VIEW};
    if (!xr_ok(xrEnumerateViewConfigurationViews(app.instance, app.systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, app.viewConfig.data()),
            "enumerate stereo views")) return false;

    if (viewCount != 2) {
        LOGE("Expected two primary stereo views, runtime reported %u", viewCount);
        return false;
    }

    if (!create_actions(app)) return false;
    return create_eye_swapchains(app);
}

bool create_instance(App& app) {
    if (!initialize_openxr_loader(app)) return false;

    const char* extensions[] = {
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
        XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
    };

    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = app.android->activity->vm;
    androidInfo.applicationActivity = app.android->activity->clazz;

    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.next = &androidInfo;
    std::strncpy(instanceInfo.applicationInfo.applicationName, "TF2 VR",
                 XR_MAX_APPLICATION_NAME_SIZE - 1);
    instanceInfo.applicationInfo.applicationVersion = 1;
    std::strncpy(instanceInfo.applicationInfo.engineName, "TF2-VR",
                 XR_MAX_ENGINE_NAME_SIZE - 1);
    instanceInfo.applicationInfo.engineVersion = 1;
    instanceInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.enabledExtensionNames = extensions;

    if (!xr_ok(xrCreateInstance(&instanceInfo, &app.instance), "create OpenXR instance")) return false;
    return create_session(app);
}

void poll_openxr_events(App& app) {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(app.instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
            app.sessionState = changed->state;
            if (changed->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                app.sessionRunning = xr_ok(xrBeginSession(app.session, &beginInfo), "begin OpenXR session");
            } else if (changed->state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(app.session);
                app.sessionRunning = false;
            } else if (changed->state == XR_SESSION_STATE_EXITING ||
                       changed->state == XR_SESSION_STATE_LOSS_PENDING) {
                app.sessionRunning = false;
            }
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool render_eye(App& app, uint32_t eyeIndex, uint32_t imageIndex) {
    EyeTarget& eye = app.eyes[eyeIndex];
    if (!vk_ok(vkWaitForFences(app.device, 1, &app.renderFence, VK_TRUE, UINT64_MAX),
               "wait for previous render") ||
        !vk_ok(vkResetCommandBuffer(app.commandBuffer, 0), "reset render command buffer")) return false;

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!vk_ok(vkBeginCommandBuffer(app.commandBuffer, &beginInfo), "begin render command buffer")) return false;

    VkClearValue clearValue{};
    if (eyeIndex == 0) {
        clearValue.color = {{0.035f, 0.055f, 0.12f, 1.0f}};
    } else {
        clearValue.color = {{0.035f, 0.075f, 0.10f, 1.0f}};
    }

    VkRenderPassBeginInfo renderBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderBegin.renderPass = app.renderPass;
    renderBegin.framebuffer = eye.framebuffers[imageIndex];
    renderBegin.renderArea.offset = {0, 0};
    renderBegin.renderArea.extent = {eye.width, eye.height};
    renderBegin.clearValueCount = 1;
    renderBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(app.commandBuffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);

    // A simple GPU-cleared calibration pattern makes each eye's rendered image visible
    // before the full 3D TF2 scene and shader pipeline are integrated.
    VkClearAttachment accent{};
    accent.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    accent.colorAttachment = 0;
    accent.clearValue.color = {{0.12f, 0.42f, 0.82f, 1.0f}};
    VkClearRect panel{};
    panel.rect.offset = {static_cast<int32_t>(eye.width / 4), static_cast<int32_t>(eye.height / 3)};
    panel.rect.extent = {eye.width / 2, eye.height / 3};
    panel.baseArrayLayer = 0;
    panel.layerCount = 1;
    vkCmdClearAttachments(app.commandBuffer, 1, &accent, 1, &panel);

    VkClearAttachment marker{};
    marker.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    marker.colorAttachment = 0;
    marker.clearValue.color = {{0.95f, 0.55f, 0.10f, 1.0f}};
    VkClearRect markerRect{};
    markerRect.rect.offset = {static_cast<int32_t>(eye.width / 2 - eye.width / 32),
                              static_cast<int32_t>(eye.height / 2 - eye.height / 32)};
    markerRect.rect.extent = {std::max(1u, eye.width / 16), std::max(1u, eye.height / 16)};
    markerRect.baseArrayLayer = 0;
    markerRect.layerCount = 1;
    vkCmdClearAttachments(app.commandBuffer, 1, &marker, 1, &markerRect);

    // Two status lamps in the headset view: green means that hand pose is currently valid.
    const uint32_t lampSize = std::max(8u, eye.width / 48);
    VkClearAttachment trackingLamp{};
    trackingLamp.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    trackingLamp.colorAttachment = 0;
    VkClearRect trackingRect{};
    trackingRect.rect.extent = {lampSize, lampSize};
    trackingRect.rect.offset = {static_cast<int32_t>(eye.width / 12),
                                static_cast<int32_t>(eye.height / 12)};
    trackingRect.baseArrayLayer = 0;
    trackingRect.layerCount = 1;
    trackingLamp.clearValue.color = app.leftControllerTracked
        ? VkClearColorValue{{0.10f, 0.90f, 0.20f, 1.0f}}
        : VkClearColorValue{{0.90f, 0.10f, 0.10f, 1.0f}};
    vkCmdClearAttachments(app.commandBuffer, 1, &trackingLamp, 1, &trackingRect);

    trackingRect.rect.offset.x += static_cast<int32_t>(lampSize * 2);
    trackingLamp.clearValue.color = app.rightControllerTracked
        ? VkClearColorValue{{0.10f, 0.90f, 0.20f, 1.0f}}
        : VkClearColorValue{{0.90f, 0.10f, 0.10f, 1.0f}};
    vkCmdClearAttachments(app.commandBuffer, 1, &trackingLamp, 1, &trackingRect);

    vkCmdEndRenderPass(app.commandBuffer);
    if (!vk_ok(vkEndCommandBuffer(app.commandBuffer), "end render command buffer")) return false;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &app.commandBuffer;
    if (!vk_ok(vkResetFences(app.device, 1, &app.renderFence), "reset render fence") ||
        !vk_ok(vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, app.renderFence),
               "submit eye render")) return false;
    return vk_ok(vkWaitForFences(app.device, 1, &app.renderFence, VK_TRUE, UINT64_MAX),
                 "wait for eye render completion");
}

void update_touch_tracking(App& app, XrTime displayTime);

bool render_and_submit_frame(App& app) {
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    if (!xr_ok(xrWaitFrame(app.session, &waitInfo, &frameState), "wait for XR frame")) return false;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    if (!xr_ok(xrBeginFrame(app.session, &beginInfo), "begin XR frame")) return false;

    update_touch_tracking(app, frameState.predictedDisplayTime);

    bool rendered = false;
    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
    };

    if (frameState.shouldRender) {
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = app.localSpace;

        uint32_t locatedViewCount = 0;
        if (xr_ok(xrLocateViews(app.session, &locateInfo, &viewState,
                static_cast<uint32_t>(app.views.size()), &locatedViewCount, app.views.data()),
                "locate stereo views") && locatedViewCount == app.eyes.size() &&
            (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0 &&
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0) {

            if (!app.trackingStatusLogged) {
                LOGI("Headset tracking valid; stereo views located");
                app.trackingStatusLogged = true;
            }

            bool allEyesRendered = true;
            for (uint32_t eyeIndex = 0; eyeIndex < app.eyes.size(); ++eyeIndex) {
                EyeTarget& eye = app.eyes[eyeIndex];
                XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                uint32_t imageIndex = 0;
                if (!xr_ok(xrAcquireSwapchainImage(eye.swapchain, &acquireInfo, &imageIndex),
                           "acquire eye image")) {
                    allEyesRendered = false;
                    break;
                }

                XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                imageWait.timeout = XR_INFINITE_DURATION;
                if (!xr_ok(xrWaitSwapchainImage(eye.swapchain, &imageWait), "wait for eye image") ||
                    imageIndex >= eye.framebuffers.size() ||
                    !render_eye(app, eyeIndex, imageIndex)) {
                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    xrReleaseSwapchainImage(eye.swapchain, &releaseInfo);
                    allEyesRendered = false;
                    break;
                }

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                if (!xr_ok(xrReleaseSwapchainImage(eye.swapchain, &releaseInfo),
                           "release rendered eye image")) {
                    allEyesRendered = false;
                    break;
                }

                XrCompositionLayerProjectionView& projectionView = app.projectionViews[eyeIndex];
                projectionView = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projectionView.pose = app.views[eyeIndex].pose;
                projectionView.fov = app.views[eyeIndex].fov;
                projectionView.subImage.swapchain = eye.swapchain;
                projectionView.subImage.imageRect.offset = {0, 0};
                projectionView.subImage.imageRect.extent = {
                    static_cast<int32_t>(eye.width), static_cast<int32_t>(eye.height)
                };
                projectionView.subImage.imageArrayIndex = 0;
            }

            if (allEyesRendered) {
                projectionLayer.space = app.localSpace;
                projectionLayer.viewCount = static_cast<uint32_t>(app.projectionViews.size());
                projectionLayer.views = app.projectionViews.data();
                rendered = true;
            }
        }
    }

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = rendered ? 1u : 0u;
    endInfo.layers = rendered ? layers : nullptr;
    return xr_ok(xrEndFrame(app.session, &endInfo), "end XR frame");
}

bool pulse_controller_haptics(App& app, XrPath handPath, XrDuration duration,
                             float amplitude, float frequency = XR_FREQUENCY_UNSPECIFIED) {
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = duration;
    vibration.frequency = frequency;
    vibration.amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    XrHapticActionInfo hapticInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    hapticInfo.action = app.hapticAction;
    hapticInfo.subactionPath = handPath;
    return xr_ok(xrApplyHapticFeedback(app.session, &hapticInfo,
        reinterpret_cast<const XrHapticBaseHeader*>(&vibration)), "send controller rumble");
}

void update_touch_tracking(App& app, XrTime displayTime) {
    XrActiveActionSet activeSet{app.actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;
    if (XR_FAILED(xrSyncActions(app.session, &syncInfo))) return;

    XrActionStateGetInfo leftGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    leftGetInfo.action = app.gripPoseAction;
    leftGetInfo.subactionPath = app.leftHandPath;
    XrActionStatePose leftState{XR_TYPE_ACTION_STATE_POSE};
    xrGetActionStatePose(app.session, &leftGetInfo, &leftState);

    XrActionStateGetInfo rightGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    rightGetInfo.action = app.gripPoseAction;
    rightGetInfo.subactionPath = app.rightHandPath;
    XrActionStatePose rightState{XR_TYPE_ACTION_STATE_POSE};
    xrGetActionStatePose(app.session, &rightGetInfo, &rightState);

    XrSpaceLocation leftLocation{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation rightLocation{XR_TYPE_SPACE_LOCATION};
    const XrResult leftResult = xrLocateSpace(app.leftGripSpace, app.localSpace, displayTime, &leftLocation);
    const XrResult rightResult = xrLocateSpace(app.rightGripSpace, app.localSpace, displayTime, &rightLocation);

    app.leftControllerTracked = leftState.isActive && XR_SUCCEEDED(leftResult) &&
        (leftLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
        (leftLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    app.rightControllerTracked = rightState.isActive && XR_SUCCEEDED(rightResult) &&
        (rightLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
        (rightLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    if (app.leftControllerTracked) app.leftControllerPose = leftLocation.pose;
    if (app.rightControllerTracked) app.rightControllerPose = rightLocation.pose;
    if (app.leftControllerTracked && !app.leftHapticTestSent) {
        if (pulse_controller_haptics(app, app.leftHandPath, 100 * 1000 * 1000LL, 0.30f))
            app.leftHapticTestSent = true;
    }
    if (app.rightControllerTracked && !app.rightHapticTestSent) {
        if (pulse_controller_haptics(app, app.rightHandPath, 100 * 1000 * 1000LL, 0.30f))
            app.rightHapticTestSent = true;
    }
    if (app.leftControllerTracked && app.rightControllerTracked && !app.controllerStatusLogged) {
        LOGI("Both Touch controllers tracked; left=(%.2f, %.2f, %.2f), right=(%.2f, %.2f, %.2f)",
             app.leftControllerPose.position.x, app.leftControllerPose.position.y, app.leftControllerPose.position.z,
             app.rightControllerPose.position.x, app.rightControllerPose.position.y, app.rightControllerPose.position.z);
        app.controllerStatusLogged = true;
        LOGI("Sent short controller haptic confirmation pulses");
    }
}

void destroy_app(App& app) {
    if (app.device != VK_NULL_HANDLE) vkDeviceWaitIdle(app.device);

    for (EyeTarget& eye : app.eyes) {
        for (VkFramebuffer framebuffer : eye.framebuffers) {
            if (framebuffer) vkDestroyFramebuffer(app.device, framebuffer, nullptr);
        }
        for (VkImageView imageView : eye.imageViews) {
            if (imageView) vkDestroyImageView(app.device, imageView, nullptr);
        }
        if (eye.swapchain) xrDestroySwapchain(eye.swapchain);
    }
    app.eyes.clear();

    if (app.leftGripSpace) xrDestroySpace(app.leftGripSpace);
    if (app.rightGripSpace) xrDestroySpace(app.rightGripSpace);
    if (app.viewSpace) xrDestroySpace(app.viewSpace);
    if (app.localSpace) xrDestroySpace(app.localSpace);
    if (app.session) xrDestroySession(app.session);
    if (app.actionSet) xrDestroyActionSet(app.actionSet);
    if (app.instance) xrDestroyInstance(app.instance);

    if (app.renderFence) vkDestroyFence(app.device, app.renderFence, nullptr);
    if (app.commandPool) vkDestroyCommandPool(app.device, app.commandPool, nullptr);
    if (app.renderPass) vkDestroyRenderPass(app.device, app.renderPass, nullptr);
    if (app.device) vkDestroyDevice(app.device, nullptr);
    if (app.vkInstance) vkDestroyInstance(app.vkInstance, nullptr);
}

} // namespace

void android_main(android_app* state) {
    app_dummy();
    App app;
    app.android = state;
    state->userData = &app;
    state->onAppCmd = handle_app_command;

    LOGI("TF2-VR Quest 3 native ARM64 startup");
    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        ALooper_pollOnce(10, nullptr, &events, reinterpret_cast<void**>(&source));
        if (source) source->process(state, source);

        if (state->destroyRequested) {
            destroy_app(app);
            return;
        }

        if (app.resumed && !app.initialized) {
            app.initialized = create_instance(app);
            if (!app.initialized) {
                LOGE("OpenXR initialization failed. Check Quest logcat and SDK/runtime setup.");
                destroy_app(app);
                return;
            }
            LOGI("OpenXR + Vulkan stereo targets initialized");
        }

        if (app.initialized) {
            poll_openxr_events(app);
            if (app.sessionRunning && app.resumed) {
                render_and_submit_frame(app);
            }
        }
    }
}
