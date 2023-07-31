#ifndef OVR_Plugin_Media_h
#define OVR_Plugin_Media_h

#include "OVR_Plugin_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

OVRP_EXPORT ovrpResult ovrp_Media_Initialize();
OVRP_EXPORT ovrpResult ovrp_Media_Shutdown();
OVRP_EXPORT ovrpResult ovrp_Media_GetInitialized(ovrpBool* initialized);
OVRP_EXPORT ovrpResult ovrp_Media_Update();

OVRP_EXPORT ovrpResult ovrp_Media_GetMrcActivationMode(ovrpMediaMrcActivationMode* activationMode);
OVRP_EXPORT ovrpResult ovrp_Media_SetMrcActivationMode(ovrpMediaMrcActivationMode activationMode);
OVRP_EXPORT ovrpResult ovrp_Media_SetPlatformInitialized();
OVRP_EXPORT ovrpResult ovrp_Media_GetPlatformCameraMode(ovrpPlatformCameraMode* platformCameraMode);
OVRP_EXPORT ovrpResult ovrp_Media_SetPlatformCameraMode(ovrpPlatformCameraMode platformCameraMode);
OVRP_EXPORT ovrpResult ovrp_Media_IsMrcEnabled(ovrpBool* mrcEnabled);
OVRP_EXPORT ovrpResult ovrp_Media_IsMrcActivated(ovrpBool* mrcActivated);
OVRP_EXPORT ovrpResult ovrp_Media_UseMrcDebugCamera(ovrpBool* useMrcDebugCamera);

OVRP_EXPORT ovrpResult ovrp_Media_SetMrcInputVideoBufferType(ovrpMediaInputVideoBufferType inputVideoBufferType);
OVRP_EXPORT ovrpResult ovrp_Media_GetMrcInputVideoBufferType(ovrpMediaInputVideoBufferType* inputVideoBufferType);
OVRP_EXPORT ovrpResult ovrp_Media_SetMrcFrameSize(int frameWidth, int frameHeight);
OVRP_EXPORT ovrpResult ovrp_Media_GetMrcFrameSize(int* frameWidth, int* frameHeight);
OVRP_EXPORT ovrpResult ovrp_Media_SetMrcAudioSampleRate(int sampleRate);
OVRP_EXPORT ovrpResult ovrp_Media_GetMrcAudioSampleRate(int* sampleRate);
OVRP_EXPORT ovrpResult ovrp_Media_SetMrcFrameImageFlipped(ovrpBool flipped);
OVRP_EXPORT ovrpResult ovrp_Media_GetMrcFrameImageFlipped(ovrpBool* flipped);
OVRP_EXPORT ovrpResult ovrp_Media_SetMrcFrameInverseAlpha(ovrpBool inverseAlpha);
OVRP_EXPORT ovrpResult ovrp_Media_GetMrcFrameInverseAlpha(ovrpBool* inverseAlpha);
OVRP_EXPORT ovrpResult ovrp_Media_SetAvailableQueueIndexVulkan(unsigned int queueIndexVk);
OVRP_EXPORT ovrpResult ovrp_Media_EncodeMrcFrame(void* videoData, float* audioData, int audioDataLen, int audioChannels, double timestamp, int* outSyncId);
OVRP_EXPORT ovrpResult ovrp_Media_EncodeMrcFrameWithDualTextures(void* backgroundTextureHandle, void* foregroundTextureHandle, float* audioData, int audioDataLen, int audioChannels, double timestamp, int* outSyncId);
OVRP_EXPORT ovrpResult ovrp_Media_SyncMrcFrame(int syncId);
OVRP_EXPORT ovrpResult ovrp_Media_EncodeMrcFrameWithPoseTime(void* videoData, float* audioData, int audioDataLen, int audioChannels, double timestamp, double poseTime, int* outSyncId);
OVRP_EXPORT ovrpResult ovrp_Media_EncodeMrcFrameDualTexturesWithPoseTime(void* backgroundTextureHandle, void* foregroundTextureHandle, float* audioData, int audioDataLen, int audioChannels, double timestamp, double poseTime, int* outSyncId);
OVRP_EXPORT ovrpResult ovrp_Media_SetHeadsetControllerPose(ovrpPosef headsetPose, ovrpPosef leftControllerPose, ovrpPosef rightControllerPose);
OVRP_EXPORT ovrpResult ovrp_Media_EnumerateCameraAnchorHandles(int* inoutAnchorCount, ovrpCameraAnchorHandle* outHandleArray);
OVRP_EXPORT ovrpResult ovrp_Media_GetCurrentCameraAnchorHandle(ovrpCameraAnchorHandle* outHandle);
OVRP_EXPORT ovrpResult ovrp_Media_GetCameraAnchorName(ovrpCameraAnchorHandle anchorHandle, char outAnchorName[OVRP_ANCHOR_NAME_SIZE]);
OVRP_EXPORT ovrpResult ovrp_Media_GetCameraAnchorHandle(const char* anchorName, ovrpCameraAnchorHandle* outHandle);
OVRP_EXPORT ovrpResult ovrp_Media_GetCameraAnchorType(ovrpCameraAnchorHandle anchorHandle, ovrpCameraAnchorType* outAnchorType);
OVRP_EXPORT ovrpResult ovrp_Media_CreateCustomCameraAnchor(const char* anchorName, ovrpCameraAnchorHandle* outHandle);
OVRP_EXPORT ovrpResult ovrp_Media_DestroyCustomCameraAnchor(ovrpCameraAnchorHandle anchorHandle);
OVRP_EXPORT ovrpResult ovrp_Media_GetCustomCameraAnchorPose(ovrpCameraAnchorHandle anchorHandle, ovrpPosef* outPose);
OVRP_EXPORT ovrpResult ovrp_Media_SetCustomCameraAnchorPose(ovrpCameraAnchorHandle anchorHandle, ovrpPosef pose);
OVRP_EXPORT ovrpResult ovrp_Media_GetCameraMinMaxDistance(ovrpCameraAnchorHandle anchorHandle, double* outMinDistance, double* outMaxDistance);
OVRP_EXPORT ovrpResult ovrp_Media_SetCameraMinMaxDistance(ovrpCameraAnchorHandle anchorHandle, double minDistance, double maxDistance);

#ifdef __cplusplus
}
#endif

#endif
