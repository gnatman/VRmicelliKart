#include "VRManager.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>

#ifdef _WIN32
#include <Windows.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#else
#include <GL/glx.h>
#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// Forward declarations for matrix math
static void CreateProjectionMatrix(float m[4][4], XrFovf fov, float nearZ, float farZ);
static void CreateViewMatrix(float m[4][4], XrPosef pose);

static XrInstance sInstance = XR_NULL_HANDLE;
static XrSession sSession = XR_NULL_HANDLE;
static XrSystemId sSystemId = XR_NULL_SYSTEM_ID;
static XrSpace sLocalSpace = XR_NULL_HANDLE;
static bool sVRActive = false;
static VREyeData sEyeData[2];
static XrFrameState sFrameState = { XR_TYPE_FRAME_STATE };

struct SwapchainData {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
    std::vector<XrSwapchainImageOpenGLKHR> images;
    std::vector<GLuint> fbos;
};
static SwapchainData sSwapchains[2];

static void CheckXrResult(XrResult result, const char* func) {
    if (XR_FAILED(result)) {
        char errorStr[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(sInstance, result, errorStr);
        SPDLOG_ERROR("OpenXR Error in {}: {} ({})", func, errorStr, (int)result);
    }
}

void VR_Init() {
    if (sVRActive) return;

    XrInstanceCreateInfo instanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strncpy(instanceCreateInfo.applicationInfo.applicationName, "Vrmicelli Kart", XR_MAX_APPLICATION_NAME_SIZE);
    instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    
    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(NULL, 0, &extCount, NULL);
    std::vector<XrExtensionProperties> extProps(extCount, { XR_TYPE_EXTENSION_PROPERTIES });
    xrEnumerateInstanceExtensionProperties(NULL, extCount, &extCount, extProps.data());

    std::vector<const char*> extensions;
    bool hasGL = false;
    for (const auto& prop : extProps) {
        if (strcmp(prop.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) {
            hasGL = true;
            extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        }
    }

    if (!hasGL) {
        SPDLOG_WARN("VR: OpenXR OpenGL extension not found. VR disabled.");
        return;
    }

    instanceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instanceCreateInfo.enabledExtensionNames = extensions.data();

    XrResult result = xrCreateInstance(&instanceCreateInfo, &sInstance);
    if (XR_FAILED(result)) {
        SPDLOG_WARN("VR: Failed to create OpenXR instance. VR disabled.");
        return;
    }

    XrSystemGetInfo systemGetInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = xrGetSystem(sInstance, &systemGetInfo, &sSystemId);
    if (XR_FAILED(result)) {
        SPDLOG_WARN("VR: No VR system found.");
        return;
    }

    sVRActive = true;
    SPDLOG_INFO("VR: OpenXR Instance and System initialized");
}

static void VR_InitGraphics() {
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();

    if (!window || !context) {
        SPDLOG_ERROR("VR: No active SDL GL window or context found for binding");
        return;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        SPDLOG_ERROR("VR: Failed to get WM info");
        return;
    }

#ifdef _WIN32
    static XrGraphicsBindingOpenGLWin32KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
    graphicsBinding.hDC = GetDC(wmInfo.info.win.window);
    graphicsBinding.hGLRC = (HGLRC)context;
#else
    static XrGraphicsBindingOpenGLXlibKHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR };
    graphicsBinding.xDisplay = wmInfo.info.x11.display;
    graphicsBinding.visualid = 0; 
    graphicsBinding.glxFBConfig = 0; 
    graphicsBinding.glxDrawable = wmInfo.info.x11.window;
    graphicsBinding.glxContext = (GLXContext)context;
#endif

    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.systemId = sSystemId;

    XrResult result = xrCreateSession(sInstance, &sessionCreateInfo, &sSession);
    if (XR_FAILED(result)) {
        CheckXrResult(result, "xrCreateSession");
        return;
    }

    XrReferenceSpaceCreateInfo spaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
    CheckXrResult(xrCreateReferenceSpace(sSession, &spaceCreateInfo, &sLocalSpace), "xrCreateReferenceSpace");

    // Create Swapchains
    uint32_t configViewCount;
    xrEnumerateViewConfigurationViews(sInstance, sSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &configViewCount, NULL);
    std::vector<XrViewConfigurationView> configViews(configViewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(sInstance, sSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, configViewCount, &configViewCount, configViews.data());

    for (uint32_t i = 0; i < 2; i++) {
        XrSwapchainCreateInfo swapchainCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = GL_RGBA8;
        swapchainCreateInfo.width = configViews[i].recommendedImageRectWidth;
        swapchainCreateInfo.height = configViews[i].recommendedImageRectHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = 1;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

        sSwapchains[i].width = swapchainCreateInfo.width;
        sSwapchains[i].height = swapchainCreateInfo.height;

        CheckXrResult(xrCreateSwapchain(sSession, &swapchainCreateInfo, &sSwapchains[i].handle), "xrCreateSwapchain");

        uint32_t imageCount;
        xrEnumerateSwapchainImages(sSwapchains[i].handle, 0, &imageCount, NULL);
        sSwapchains[i].images.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        xrEnumerateSwapchainImages(sSwapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)sSwapchains[i].images.data());

        // Create FBOs for blitting
        sSwapchains[i].fbos.resize(imageCount);
        glGenFramebuffers(imageCount, sSwapchains[i].fbos.data());
        for (uint32_t j = 0; j < imageCount; j++) {
            glBindFramebuffer(GL_FRAMEBUFFER, sSwapchains[i].fbos[j]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sSwapchains[i].images[j].image, 0);
        }
    }

    SPDLOG_INFO("VR: Graphics binding, session, and swapchains created");
}

static XrView sViews[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };

void VR_Update() {
    if (!sVRActive) return;

    if (sSession == XR_NULL_HANDLE) {
        VR_InitGraphics();
        if (sSession == XR_NULL_HANDLE) return;
    }

    XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(sInstance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged* sessionEvent = (XrEventDataSessionStateChanged*)&event;
                if (sessionEvent->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(sSession, &beginInfo);
                } else if (sessionEvent->state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(sSession);
                }
                break;
            }
            default: break;
        }
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
    }

    XrFrameWaitInfo frameWaitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    xrWaitFrame(sSession, &frameWaitInfo, &sFrameState);

    XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = sFrameState.predictedDisplayTime;
    viewLocateInfo.space = sLocalSpace;

    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    uint32_t viewCount;
    xrLocateViews(sSession, &viewLocateInfo, &viewState, 2, &viewCount, sViews);

    if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) {
        for (int i = 0; i < 2; i++) {
            CreateProjectionMatrix(sEyeData[i].projectionMatrix, sViews[i].fov, 10.0f, 12800.0f);
            CreateViewMatrix(sEyeData[i].viewMatrix, sViews[i].pose);
        }
    }
}

void VR_BeginFrame() {
    if (!VR_IsActive()) return;
    XrFrameBeginInfo frameBeginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    xrBeginFrame(sSession, &frameBeginInfo);
}

void VR_EndFrame() {
    if (!VR_IsActive()) return;

    XrCompositionLayerProjectionView projectionViews[2] = {};
    for (int i = 0; i < 2; i++) {
        projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionViews[i].pose = sViews[i].pose;
        projectionViews[i].fov = sViews[i].fov;
        projectionViews[i].subImage.swapchain = sSwapchains[i].handle;
        projectionViews[i].subImage.imageRect = { {0, 0}, {sSwapchains[i].width, sSwapchains[i].height} };
    }

    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projectionLayer.space = sLocalSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionViews;

    const XrCompositionLayerBaseHeader* layers[] = { (XrCompositionLayerBaseHeader*)&projectionLayer };

    XrFrameEndInfo frameEndInfo = { XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = sFrameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = 1;
    frameEndInfo.layers = layers;
    xrEndFrame(sSession, &frameEndInfo);
}

void VR_Terminate() {
    if (sSession != XR_NULL_HANDLE) xrDestroySession(sSession);
    if (sInstance != XR_NULL_HANDLE) xrDestroyInstance(sInstance);
    sVRActive = false;
}

bool VR_IsActive() {
    return sVRActive && sSession != XR_NULL_HANDLE;
}

VREyeData VR_GetEyeData(int eye) {
    if (eye < 0 || eye > 1) return sEyeData[0];
    return sEyeData[eye];
}

VREyeData VR_GetRigCenterData() {
    VREyeData center;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            center.projectionMatrix[i][j] = (sEyeData[0].projectionMatrix[i][j] + sEyeData[1].projectionMatrix[i][j]) * 0.5f;
            center.viewMatrix[i][j] = (sEyeData[0].viewMatrix[i][j] + sEyeData[1].viewMatrix[i][j]) * 0.5f;
        }
    }
    return center;
}

void VR_BindEye(int eye) {
    if (!VR_IsActive() || eye < 0 || eye > 1) return;

    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    uint32_t imageIndex;
    xrAcquireSwapchainImage(sSwapchains[eye].handle, &acquireInfo, &imageIndex);

    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(sSwapchains[eye].handle, &waitInfo);

    glBindFramebuffer(GL_FRAMEBUFFER, sSwapchains[eye].fbos[imageIndex]);
    glViewport(0, 0, sSwapchains[eye].width, sSwapchains[eye].height);
}

void VR_CommitEye(int eye) {
    if (!VR_IsActive() || eye < 0 || eye > 1) return;

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(sSwapchains[eye].handle, &releaseInfo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Matrix Utility Implementations
static void CreateProjectionMatrix(float m[4][4], XrFovf fov, float nearZ, float farZ) {
    float tanLeft = tanf(fov.angleLeft);
    float tanRight = tanf(fov.angleRight);
    float tanUp = tanf(fov.angleUp);
    float tanDown = tanf(fov.angleDown);
    float tanWidth = tanRight - tanLeft;
    float tanHeight = tanUp - tanDown;
    std::memset(m, 0, sizeof(float) * 16);
    m[0][0] = 2.0f / tanWidth;
    m[1][1] = 2.0f / tanHeight;
    m[2][0] = (tanRight + tanLeft) / tanWidth;
    m[2][1] = (tanUp + tanDown) / tanHeight;
    m[2][2] = -(farZ + nearZ) / (farZ - nearZ);
    m[2][3] = -1.0f;
    m[3][2] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
}

static void CreateViewMatrix(float m[4][4], XrPosef pose) {
    float x = pose.orientation.x, y = pose.orientation.y, z = pose.orientation.z, w = pose.orientation.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    float rot[4][4] = {
        {1.0f - (yy + zz), xy + wz, xz - wy, 0.0f},
        {xy - wz, 1.0f - (xx + zz), yz + wx, 0.0f},
        {xz + wy, yz - wx, 1.0f - (xx + yy), 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
    float trans[4][4] = {
        {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {-pose.position.x, -pose.position.y, -pose.position.z, 1}
    };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m[i][j] = 0;
            for (int k = 0; k < 4; k++) m[i][j] += trans[i][k] * rot[k][j];
        }
    }
}
