/************************************************************************************

Copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.

Your use of this SDK or tool is subject to the Oculus SDK License Agreement, available at
https://developer.oculus.com/licenses/oculussdk/

Unless required by applicable law or agreed to in writing, the Oculus SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Plugin_Deprecated_h
#define OVR_Plugin_Deprecated_h

#include "OVR_Plugin.h"
#include "OVR_Plugin_Types_Deprecated.h"

#ifdef __cplusplus
extern "C" {
#endif

// Deprecated to avoid an extra shutdown/initialize on the mobile side
OVRP_EXPORT ovrpResult ovrp_PreInitialize3(void* activity);

// Deprecated for getting extra Vulkan paramters from Unity Oculus XR Plugin

OVRP_EXPORT ovrpResult ovrp_Initialize5(
    ovrpRenderAPIType apiType,
    ovrpLogCallback logCallback,
    void* activity,
    void* vkInstance,
    void* vkPhysicalDevice,
    void* vkDevice,
    void* vkQueue,
    int initializeFlags,
    OVRP_CONSTREF(ovrpVersion) version);

// Deprecated by VRAPI_Vulkan changes
OVRP_EXPORT ovrpResult ovrp_PreInitialize2();

OVRP_EXPORT ovrpResult ovrp_Initialize4(
    ovrpRenderAPIType apiType,
    ovrpLogCallback logCallback,
    void* activity,
    void* instance,
    int initializeFlags,
    OVRP_CONSTREF(ovrpVersion) version);

// Deprecated by CAPI_Vulkan changes
OVRP_EXPORT ovrpResult ovrp_Initialize3(
    ovrpRenderAPIType apiType,
    ovrpLogCallback logCallback,
    void* activity,
    int initializeFlags,
    OVRP_CONSTREF(ovrpVersion));

OVRP_EXPORT ovrpResult ovrp_BeginFrame3(int frameIndex);

OVRP_EXPORT ovrpResult
ovrp_EndFrame3(int frameIndex, ovrpLayerSubmit const* const* layerSubmitPtrs, int layerSubmitCount);

// Deprecated by WaitToBeginFrame/BeginFrame/EndFrame changes
OVRP_EXPORT ovrpResult ovrp_BeginFrame2(int frameIndex);

OVRP_EXPORT ovrpResult
ovrp_EndFrame2(int frameIndex, ovrpLayerSubmit const* const* layerSubmitPtrs, int layerSubmitCount);

// Deprecated by ovrpResult changes
OVRP_EXPORT ovrpBool ovrp_PreInitialize();
OVRP_EXPORT ovrpBool ovrp_Shutdown();
OVRP_EXPORT const char* ovrp_GetVersion();
OVRP_EXPORT const char* ovrp_GetNativeSDKVersion();
OVRP_EXPORT void* ovrp_GetNativeSDKPointer();
OVRP_EXPORT const void* ovrp_GetDisplayAdapterId();
OVRP_EXPORT const void* ovrp_GetAudioOutId();
OVRP_EXPORT const void* ovrp_GetAudioOutDeviceId();
OVRP_EXPORT const void* ovrp_GetAudioInId();
OVRP_EXPORT const void* ovrp_GetAudioInDeviceId();
OVRP_EXPORT ovrpBool ovrp_SetupDistortionWindow2(int flags);
OVRP_EXPORT ovrpBool ovrp_DestroyDistortionWindow();

OVRP_EXPORT ovrpBool
ovrp_SetupMirrorTexture(void* device, int height, int width, ovrpTextureFormat format, ovrpTextureHandle* result);

OVRP_EXPORT ovrpBool ovrp_DestroyMirrorTexture();
OVRP_EXPORT float ovrp_GetAdaptiveGpuPerformanceScale();
OVRP_EXPORT float ovrp_GetAppCpuStartToGpuEndTime();
OVRP_EXPORT ovrpVector2f ovrp_GetEyePixelsPerTanAngleAtCenter(int eyeIndex);
OVRP_EXPORT ovrpVector3f ovrp_GetHmdToEyeOffset(int eyeIndex);
OVRP_EXPORT ovrpBool ovrp_Update2(ovrpStep step, int frameIndex, double predictionSeconds);
OVRP_EXPORT ovrpBool ovrp_BeginFrame(int frameIndex);
OVRP_EXPORT ovrpBool ovrp_GetTrackingOrientationSupported();
OVRP_EXPORT ovrpBool ovrp_GetTrackingOrientationEnabled();
OVRP_EXPORT ovrpBool ovrp_SetTrackingOrientationEnabled(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetTrackingPositionSupported();
OVRP_EXPORT ovrpBool ovrp_GetTrackingPositionEnabled();
OVRP_EXPORT ovrpBool ovrp_SetTrackingPositionEnabled(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetTrackingIPDEnabled();
OVRP_EXPORT ovrpBool ovrp_SetTrackingIPDEnabled(ovrpBool value);
OVRP_EXPORT ovrpPosef ovrp_GetTrackingCalibratedOrigin();
OVRP_EXPORT ovrpBool ovrpi_SetTrackingCalibratedOrigin();
OVRP_EXPORT ovrpTrackingOrigin ovrp_GetTrackingOriginType();
OVRP_EXPORT ovrpBool ovrp_SetTrackingOriginType(ovrpTrackingOrigin originType);
OVRP_EXPORT ovrpBool ovrp_RecenterTrackingOrigin(unsigned int flags);
OVRP_EXPORT ovrpBool ovrp_GetNodePresent(ovrpNode nodeId);
OVRP_EXPORT ovrpBool ovrp_GetNodeOrientationTracked(ovrpNode nodeId);
OVRP_EXPORT ovrpBool ovrp_GetNodePositionTracked(ovrpNode nodeId);
OVRP_EXPORT ovrpBool ovrp_SetNodePositionTracked(ovrpNode nodeId, ovrpBool tracked);
OVRP_EXPORT ovrpPoseStatef ovrp_GetNodePoseState(ovrpStep step, ovrpNode nodeId);
OVRP_EXPORT ovrpControllerState ovrp_GetControllerState(ovrpController controllerMask);
OVRP_EXPORT ovrpControllerState2 ovrp_GetControllerState2(ovrpController controllerMask);
OVRP_EXPORT ovrpResult ovrp_GetControllerState3(ovrpController controllerMask, ovrpControllerState2* controllerState);
OVRP_EXPORT ovrpController ovrp_GetActiveController();
OVRP_EXPORT ovrpController ovrp_GetConnectedControllers();

OVRP_EXPORT ovrpBool ovrp_SetControllerVibration(ovrpController controllerMask, float frequency, float amplitude);

OVRP_EXPORT ovrpHapticsDesc ovrp_GetControllerHapticsDesc(ovrpController controllerMask);
OVRP_EXPORT ovrpHapticsState ovrp_GetControllerHapticsState(ovrpController controllerMask);

OVRP_EXPORT ovrpBool ovrp_SetControllerHaptics(ovrpController controllerMask, ovrpHapticsBuffer hapticsBuffer);

OVRP_EXPORT int ovrp_GetSystemCpuLevel();
OVRP_EXPORT ovrpBool ovrp_SetSystemCpuLevel(int value);
OVRP_EXPORT ovrpBool ovrp_SetAppCPUPriority(ovrpBool priority);
OVRP_EXPORT ovrpBool ovrp_GetAppCPUPriority();
OVRP_EXPORT int ovrp_GetSystemGpuLevel();
OVRP_EXPORT ovrpBool ovrp_SetSystemGpuLevel(int value);
OVRP_EXPORT ovrpBool ovrp_GetSystemPowerSavingMode();
OVRP_EXPORT float ovrp_GetSystemDisplayFrequency();
OVRP_EXPORT int ovrp_GetSystemVSyncCount();
OVRP_EXPORT ovrpBool ovrp_SetSystemVSyncCount(int value);
OVRP_EXPORT float ovrp_GetSystemVolume();
OVRP_EXPORT ovrpBool ovrp_GetSystemHeadphonesPresent();
OVRP_EXPORT ovrpBatteryStatus ovrp_GetSystemBatteryStatus();
OVRP_EXPORT float ovrp_GetSystemBatteryLevel();
OVRP_EXPORT float ovrp_GetSystemBatteryTemperature();
OVRP_EXPORT const char* ovrp_GetSystemProductName();
OVRP_EXPORT ovrpSystemRegion ovrp_GetSystemRegion();
OVRP_EXPORT ovrpBool ovrp_ShowSystemUI(ovrpUI ui);
OVRP_EXPORT ovrpBool ovrp_GetAppHasVrFocus();
OVRP_EXPORT ovrpBool ovrp_GetAppShouldQuit();
OVRP_EXPORT ovrpBool ovrp_GetAppShouldRecenter();
OVRP_EXPORT ovrpBool ovrp_GetAppShouldRecreateDistortionWindow();
OVRP_EXPORT const char* ovrp_GetAppLatencyTimings();

OVRP_EXPORT ovrpBool ovrp_SetAppEngineInfo(const char* engineName, const char* engineVersion, ovrpBool isEditor);

OVRP_EXPORT ovrpBool ovrp_GetUserPresent();
OVRP_EXPORT float ovrp_GetUserIPD();
OVRP_EXPORT ovrpBool ovrp_SetUserIPD(float value);
OVRP_EXPORT float ovrp_GetUserEyeHeight();
OVRP_EXPORT ovrpBool ovrp_SetUserEyeHeight(float value);
OVRP_EXPORT ovrpVector2f ovrp_GetUserNeckEyeDistance();
OVRP_EXPORT ovrpBool ovrp_SetUserNeckEyeDistance(ovrpVector2f value);
OVRP_EXPORT ovrpBool ovrp_SetupDisplayObjects(void* device, void* display, void* window);
OVRP_EXPORT ovrpBool ovrp_GetSystemMultiViewSupported();
OVRP_EXPORT ovrpBool ovrp_GetEyeTextureArraySupported();
OVRP_EXPORT ovrpBool ovrp_GetBoundaryConfigured();

OVRP_EXPORT ovrpBoundaryTestResult ovrp_TestBoundaryNode(ovrpNode node, ovrpBoundaryType boundaryType);

OVRP_EXPORT ovrpBoundaryTestResult ovrp_TestBoundaryPoint(ovrpVector3f point, ovrpBoundaryType boundaryType);

OVRP_EXPORT ovrpBool ovrp_GetBoundaryGeometry2(ovrpBoundaryType boundaryType, ovrpVector3f* points, int* pointsCount);

OVRP_EXPORT ovrpVector3f ovrp_GetBoundaryDimensions(ovrpBoundaryType boundaryType);
OVRP_EXPORT ovrpBool ovrp_GetBoundaryVisible();
OVRP_EXPORT ovrpBool ovrp_SetBoundaryVisible(ovrpBool value);
OVRP_EXPORT ovrpSystemHeadset ovrp_GetSystemHeadsetType();
OVRP_EXPORT ovrpAppPerfStats ovrp_GetAppPerfStats();
OVRP_EXPORT ovrpBool ovrp_ResetAppPerfStats();
OVRP_EXPORT float ovrp_GetAppFramerate();
OVRP_EXPORT int ovrp_GetSystemRecommendedMSAALevel();
OVRP_EXPORT ovrpBool ovrp_SetInhibitSystemUX(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_SetDebugDumpEnabled(ovrpBool value);

// Deprecated by UE4 integration changes
OVRP_EXPORT ovrpBool
ovrp_Initialize2(ovrpRenderAPIType apiType, ovrpLogCallback logCallback, ovrpBool supportsMixedRendering);

OVRP_EXPORT ovrpBool ovrp_SetupDistortionWindow();

OVRP_EXPORT ovrpBool ovrp_SetupEyeTexture2(
    ovrpEye eyeId,
    int stage,
    void* device,
    int height,
    int width,
    int samples,
    ovrpTextureFormat format,
    void* result);

OVRP_EXPORT ovrpBool ovrp_DestroyEyeTexture(ovrpEye eyeId, int stage);
OVRP_EXPORT ovrpSizei ovrp_GetEyeTextureSize(ovrpEye eyeId);
OVRP_EXPORT int ovrp_GetEyeTextureStageCount();
OVRP_EXPORT float ovrp_GetEyeRecommendedResolutionScale();
OVRP_EXPORT ovrpBool ovrp_GetEyeTextureFlippedY();
OVRP_EXPORT ovrpBool ovrp_SetEyeTextureFlippedY(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetEyeTextureShared();
OVRP_EXPORT ovrpBool ovrp_SetEyeTextureShared(ovrpBool value);
OVRP_EXPORT float ovrp_GetEyeTextureScale();
OVRP_EXPORT ovrpBool ovrp_SetEyeTextureScale(float value);
OVRP_EXPORT float ovrp_GetEyeViewportScale();
OVRP_EXPORT ovrpBool ovrp_SetEyeViewportScale(float value);

OVRP_EXPORT ovrpBool ovrp_GetEyeOcclusionMesh(int eyeIndex, float** vertices, int** indices, int* indexCount);

OVRP_EXPORT ovrpBool ovrp_GetEyeOcclusionMeshEnabled();
OVRP_EXPORT ovrpBool ovrp_SetEyeOcclusionMeshEnabled(ovrpBool value);
OVRP_EXPORT ovrpTextureFormat ovrp_GetDesiredEyeTextureFormat();
OVRP_EXPORT ovrpBool ovrp_SetDesiredEyeTextureFormat(ovrpTextureFormat value);
OVRP_EXPORT ovrpBool ovrp_GetEyePreviewRect(int eyeIndex, ovrpRecti* outputRect);
OVRP_EXPORT ovrpBool ovrp_GetAppChromaticCorrection();
OVRP_EXPORT ovrpBool ovrp_SetAppChromaticCorrection(ovrpBool value);
OVRP_EXPORT ovrpResult ovrp_GetReorientHMDOnControllerRecenter(ovrpBool* recenter);
OVRP_EXPORT ovrpResult ovrp_SetReorientHMDOnControllerRecenter(ovrpBool recenter);
OVRP_EXPORT ovrpBool ovrp_EndEye(ovrpEye eye);
OVRP_EXPORT ovrpBool ovrp_EndFrame(int frameIndex);
OVRP_EXPORT ovrpBool ovrpi_SetTrackingCalibratedOrigin();
OVRP_EXPORT ovrpPosef ovrp_GetNodeVelocity2(ovrpStep step, ovrpNode nodeId);
OVRP_EXPORT ovrpPosef ovrp_GetNodeAcceleration2(ovrpStep step, ovrpNode nodeId);
OVRP_EXPORT ovrpFrustumf ovrp_GetNodeFrustum(ovrpNode nodeId);
OVRP_EXPORT ovrpBool ovrp_GetAppMonoscopic();
OVRP_EXPORT ovrpBool ovrp_SetAppMonoscopic(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetAppSRGB();
OVRP_EXPORT ovrpBool ovrp_SetAppSRGB(ovrpBool value);
OVRP_EXPORT float ovrp_GetUserEyeDepth();
OVRP_EXPORT ovrpBool ovrp_SetUserEyeDepth(float value);
OVRP_EXPORT ovrpBool ovrp_SetEyeTextureArrayEnabled(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetEyeTextureArrayEnabled();
OVRP_EXPORT ovrpResult ovrp_GetAppAsymmetricFov(ovrpBool* useAsymmetricFov);
OVRP_EXPORT ovrpResult ovrp_SetAppAsymmetricFov(ovrpBool value);


OVRP_EXPORT ovrpBool ovrp_SetOverlayQuad3(
    unsigned int flags,
    void* textureLeft,
    void* textureRight,
    void* device,
    ovrpPosef pose,
    ovrpVector3f scale,
    int layerIndex);

OVRP_EXPORT ovrpResult ovrp_EnqueueSetupLayer(ovrpLayerDesc* desc, int* layerId);

OVRP_EXPORT ovrpResult ovrp_EnqueueSetupLayer2(ovrpLayerDesc* desc, int compositionDepth, int* layerId);

OVRP_EXPORT ovrpResult ovrp_EnqueueDestroyLayer(int* layerId);

OVRP_EXPORT ovrpResult ovrp_GetLayerTexturePtr(int layerId, int stage, ovrpEye eyeId, void** texturePtr);

OVRP_EXPORT ovrpResult ovrp_EnqueueSubmitLayer(
    unsigned int flags,
    void* textureLeft,
    void* textureRight,
    int layerId,
    int frameIndex,
    OVRP_CONSTREF(ovrpPosef) pose,
    OVRP_CONSTREF(ovrpVector3f) scale,
    int layerIndex);

OVRP_EXPORT ovrpResult ovrp_EnqueueSubmitLayer2(
  unsigned int flags,
  void* textureLeft,
  void* textureRight,
  int layerId,
  int frameIndex,
  OVRP_CONSTREF(ovrpPosef) pose,
  OVRP_CONSTREF(ovrpVector3f) scale,
  int layerIndex,
  ovrpBool overrideTextureRectMatrix,
  OVRP_CONSTREF(ovrpTextureRectMatrixf) textureRectMatrix,
  ovrpBool overridePerLayerColorScaleAndOffset,
  OVRP_CONSTREF(ovrpVector4f) colorScale,
  OVRP_CONSTREF(ovrpVector4f) colorOffset);





















// Previously deprecated
OVRP_EXPORT ovrpBool ovrp_Initialize(ovrpRenderAPIType apiType, void* platformArgs);
OVRP_EXPORT ovrpBoundaryGeometry ovrp_GetBoundaryGeometry(ovrpBoundaryType boundaryType);
OVRP_EXPORT void* ovrp_GetNativePointer();
OVRP_EXPORT ovrpBool ovrp_DismissHSW();
OVRP_EXPORT void* ovrp_GetAdapterId();
OVRP_EXPORT int ovrp_GetBufferCount();
OVRP_EXPORT ovrpBool ovrp_SetEyeTexture(ovrpEye eyeId, void* texture, void* device);

OVRP_EXPORT ovrpBool ovrp_RecreateEyeTexture(
    ovrpEye eyeId,
    int stage,
    void* device,
    int height,
    int width,
    int samples,
    ovrpBool isSRGB,
    void* result);

OVRP_EXPORT ovrpBool ovrp_ReleaseEyeTexture(ovrpEye eyeId, int stage);
OVRP_EXPORT ovrpPosef ovrp_GetEyePose(ovrpEye eyeId);
OVRP_EXPORT ovrpPosef ovrp_GetEyeVelocity(ovrpEye eyeId);
OVRP_EXPORT ovrpPosef ovrp_GetEyeAcceleration(ovrpEye eyeId);
OVRP_EXPORT ovrpFrustumf ovrp_GetEyeFrustum(ovrpEye eyeId);
OVRP_EXPORT ovrpPosef ovrp_GetTrackerPose(ovrpTracker trackerId);
OVRP_EXPORT ovrpFrustumf ovrp_GetTrackerFrustum(ovrpTracker trackerId);
OVRP_EXPORT ovrpBool ovrp_RecenterPose();
OVRP_EXPORT ovrpInputState ovrp_GetInputState(ovrpController controllerMask);
OVRP_EXPORT ovrpBatteryStatus ovrp_GetBatteryStatus();
OVRP_EXPORT ovrpBool ovrp_ShowUI(ovrpUI ui);
OVRP_EXPORT ovrpCaps ovrp_GetCaps();
OVRP_EXPORT unsigned int ovrp_GetCaps2(unsigned int query);
OVRP_EXPORT ovrpBool ovrp_SetCaps(ovrpCaps caps);
OVRP_EXPORT ovrpStatus ovrp_GetStatus();
OVRP_EXPORT unsigned int ovrp_GetStatus2(unsigned int query);
OVRP_EXPORT float ovrp_GetFloat(ovrpKey key);
OVRP_EXPORT ovrpBool ovrp_SetFloat(ovrpKey key, float value);
OVRP_EXPORT const char* ovrp_GetString(ovrpKey key);

OVRP_EXPORT ovrpBool
ovrp_SetOverlayQuad(ovrpBool onTop, void* texture, void* device, ovrpPosef pose, ovrpVector3f scale);

OVRP_EXPORT ovrpBool ovrp_SetOverlayQuad2(
    ovrpBool onTop,
    ovrpBool headLocked,
    void* texture,
    void* device,
    ovrpPosef pose,
    ovrpVector3f scale);

OVRP_EXPORT ovrpResult ovrp_CalculateEyeLayerDesc(
	ovrpLayout layout,
	float textureScale,
	int mipLevels,
	int sampleCount,
	ovrpTextureFormat format,
	int layerFlags,
	ovrpLayerDesc_EyeFov* layerDesc);

OVRP_EXPORT ovrpBool ovrp_SetAppIgnoreVrFocus(ovrpBool value);
OVRP_EXPORT ovrpBool ovrp_GetHeadphonesPresent();
OVRP_EXPORT ovrpBool ovrp_Update(int frameIndex);
OVRP_EXPORT ovrpPosef ovrp_GetNodePose(ovrpNode nodeId);
OVRP_EXPORT ovrpPosef ovrp_GetNodeVelocity(ovrpNode nodeId);
OVRP_EXPORT ovrpPosef ovrp_GetNodeAcceleration(ovrpNode nodeId);

/// Gets the texture handle for a specific layer stage and eye.
OVRP_EXPORT ovrpResult ovrp_GetLayerTexture(int layerId, int stage, ovrpEye eyeId, ovrpTextureHandle* textureHandle);

OVRP_EXPORT ovrpBool ovrp_SetupEyeTexture(
    ovrpEye eyeId,
    int stage,
    void* device,
    int height,
    int width,
    int samples,
    ovrpBool isSRGB,
    void* result);

OVRP_EXPORT ovrpPosef ovrp_GetNodePose2(ovrpStep step, ovrpNode nodeId);

OVRP_EXPORT ovrpResult ovrp_SetFunctionPointer(ovrpFunctionType funcType, void *funcPtr);

// Return success if updating depth info is finished
OVRP_EXPORT ovrpResult ovrp_SetDepthCompositingInfo(float zNear, float zFar, ovrpBool isReverseZ);

OVRP_EXPORT ovrpResult ovrp_SetOctilinearInfo(ovrpOctilinearLayout OctilinearLayout[ovrpEye_Count]);

/// Gets the current pose, acceleration, and velocity of the given node on the given update cadence.
OVRP_EXPORT ovrpResult ovrp_GetNodePoseState2(ovrpStep step, ovrpNode nodeId, ovrpPoseStatef* nodePoseState);

// Called by Unity render thread after finished each eye rendering
OVRP_EXPORT ovrpResult ovrp_EndEye2(ovrpEye eye, int frameIndex);

// Update depth projection info, this is a replacement of ovrp_SetDepthCompositingInfo for more generic purpose
OVRP_EXPORT ovrpResult ovrp_SetDepthProjInfo(float zNear, float zFar, ovrpBool isReverseZ);

// Deprecated
OVRP_EXPORT ovrpResult ovrp_SetASWEnable(ovrpBool enable);

// Deprecated
OVRP_EXPORT ovrpResult ovrp_GetASWEnable(ovrpBool* enable);

OVRP_EXPORT ovrpResult ovrp_GetSkeleton(ovrpSkeletonType skeletonType, ovrpSkeleton* skeleton);
#ifdef __cplusplus
}
#endif

#endif
