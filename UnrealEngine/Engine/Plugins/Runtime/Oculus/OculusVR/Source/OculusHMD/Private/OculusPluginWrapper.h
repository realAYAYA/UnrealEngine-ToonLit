// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <memory.h>

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#pragma warning(push)
#pragma warning(disable:4201)		// nonstandard extension used: nameless struct/union
//#pragma warning(disable:4668)		// 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#define OVRP_EXPORT typedef
#include "OVR_Plugin.h"
#include "OVR_Plugin_MixedReality.h"
#include "OVR_Plugin_Media.h"
#undef OVRP_EXPORT
#pragma warning(pop)

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogOculusPluginWrapper, Log, All);

#define OCULUS_DECLARE_ENTRY_POINT(Func)		ovrp_##Func* Func

struct OculusPluginWrapper
{
	OculusPluginWrapper()
	{
		Reset();
	}

	void Reset()
	{
		memset(this, 0, sizeof(OculusPluginWrapper));
		ovrpHeaderVersion.MajorVersion = OVRP_MAJOR_VERSION;
		ovrpHeaderVersion.MinorVersion = OVRP_MINOR_VERSION;
		ovrpHeaderVersion.PatchVersion = OVRP_PATCH_VERSION;
	}

	ovrpVersion ovrpHeaderVersion;
	bool Initialized;

	// OVR_Plugin.h

	OCULUS_DECLARE_ENTRY_POINT(PreInitialize4);
	OCULUS_DECLARE_ENTRY_POINT(GetInitialized);
	OCULUS_DECLARE_ENTRY_POINT(Initialize6);
	OCULUS_DECLARE_ENTRY_POINT(Shutdown2);
	OCULUS_DECLARE_ENTRY_POINT(GetVersion2);
	OCULUS_DECLARE_ENTRY_POINT(GetNativeSDKVersion2);
	OCULUS_DECLARE_ENTRY_POINT(GetNativeSDKPointer2);
	OCULUS_DECLARE_ENTRY_POINT(GetDisplayAdapterId2);
	OCULUS_DECLARE_ENTRY_POINT(GetAudioOutId2);
	OCULUS_DECLARE_ENTRY_POINT(GetAudioOutDeviceId2);
	OCULUS_DECLARE_ENTRY_POINT(GetAudioInId2);
	OCULUS_DECLARE_ENTRY_POINT(GetAudioInDeviceId2);
	OCULUS_DECLARE_ENTRY_POINT(GetInstanceExtensionsVk);
	OCULUS_DECLARE_ENTRY_POINT(GetDeviceExtensionsVk);
	OCULUS_DECLARE_ENTRY_POINT(SetupDistortionWindow3);
	OCULUS_DECLARE_ENTRY_POINT(DestroyDistortionWindow2);
	OCULUS_DECLARE_ENTRY_POINT(GetDominantHand);
	OCULUS_DECLARE_ENTRY_POINT(SetRemoteHandedness);
	OCULUS_DECLARE_ENTRY_POINT(SetColorScaleAndOffset);
	OCULUS_DECLARE_ENTRY_POINT(SetupLayer);
	OCULUS_DECLARE_ENTRY_POINT(SetupLayerDepth);
	OCULUS_DECLARE_ENTRY_POINT(SetEyeFovPremultipliedAlphaMode);
	OCULUS_DECLARE_ENTRY_POINT(GetEyeFovLayerId);
	OCULUS_DECLARE_ENTRY_POINT(GetLayerTextureStageCount);
	OCULUS_DECLARE_ENTRY_POINT(GetLayerTexture2);
	OCULUS_DECLARE_ENTRY_POINT(GetLayerTextureFoveation);
	OCULUS_DECLARE_ENTRY_POINT(GetLayerAndroidSurfaceObject);
	OCULUS_DECLARE_ENTRY_POINT(GetLayerOcclusionMesh);
	OCULUS_DECLARE_ENTRY_POINT(DestroyLayer);
	OCULUS_DECLARE_ENTRY_POINT(CalculateLayerDesc);
	OCULUS_DECLARE_ENTRY_POINT(CalculateEyeLayerDesc2);
	OCULUS_DECLARE_ENTRY_POINT(CalculateEyeViewportRect);
	OCULUS_DECLARE_ENTRY_POINT(CalculateEyePreviewRect);
	OCULUS_DECLARE_ENTRY_POINT(SetupMirrorTexture2);
	OCULUS_DECLARE_ENTRY_POINT(DestroyMirrorTexture2);
	OCULUS_DECLARE_ENTRY_POINT(GetAdaptiveGpuPerformanceScale2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppCpuStartToGpuEndTime2);
	OCULUS_DECLARE_ENTRY_POINT(GetEyePixelsPerTanAngleAtCenter2);
	OCULUS_DECLARE_ENTRY_POINT(GetHmdToEyeOffset2);
	OCULUS_DECLARE_ENTRY_POINT(Update3);
	OCULUS_DECLARE_ENTRY_POINT(WaitToBeginFrame);
	OCULUS_DECLARE_ENTRY_POINT(BeginFrame4);
	OCULUS_DECLARE_ENTRY_POINT(UpdateFoveation);
	OCULUS_DECLARE_ENTRY_POINT(EndFrame4);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingOrientationSupported2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingOrientationEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(SetTrackingOrientationEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingPositionSupported2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingPositionEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(SetTrackingPositionEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingIPDEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(SetTrackingIPDEnabled2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingCalibratedOrigin2);
	OCULUS_DECLARE_ENTRY_POINT(SetTrackingCalibratedOrigin2);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingOriginType2);
	OCULUS_DECLARE_ENTRY_POINT(SetTrackingOriginType2);
	OCULUS_DECLARE_ENTRY_POINT(RecenterTrackingOrigin2);
	OCULUS_DECLARE_ENTRY_POINT(GetNodePresent2);
	OCULUS_DECLARE_ENTRY_POINT(GetNodeOrientationTracked2);
	OCULUS_DECLARE_ENTRY_POINT(GetNodeOrientationValid);
	OCULUS_DECLARE_ENTRY_POINT(GetNodePositionTracked2);
	OCULUS_DECLARE_ENTRY_POINT(GetNodePositionValid);
	OCULUS_DECLARE_ENTRY_POINT(SetNodePositionTracked2);
	OCULUS_DECLARE_ENTRY_POINT(GetNodePoseState3);
	OCULUS_DECLARE_ENTRY_POINT(GetNodePoseStateRaw);
	OCULUS_DECLARE_ENTRY_POINT(GetNodeFrustum2);
	OCULUS_DECLARE_ENTRY_POINT(SetHeadPoseModifier);
	OCULUS_DECLARE_ENTRY_POINT(GetHeadPoseModifier);
	OCULUS_DECLARE_ENTRY_POINT(GetControllerState4);
	OCULUS_DECLARE_ENTRY_POINT(GetActiveController2);
	OCULUS_DECLARE_ENTRY_POINT(GetConnectedControllers2);
	OCULUS_DECLARE_ENTRY_POINT(SetControllerVibration2);
	OCULUS_DECLARE_ENTRY_POINT(GetControllerHapticsDesc2);
	OCULUS_DECLARE_ENTRY_POINT(GetControllerHapticsState2);
	OCULUS_DECLARE_ENTRY_POINT(SetControllerHaptics2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemCpuLevel2);
	OCULUS_DECLARE_ENTRY_POINT(SetSystemCpuLevel2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppCPUPriority2);
	OCULUS_DECLARE_ENTRY_POINT(SetAppCPUPriority2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemGpuLevel2);
	OCULUS_DECLARE_ENTRY_POINT(SetSystemGpuLevel2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemPowerSavingMode2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemDisplayFrequency2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemDisplayAvailableFrequencies);
	OCULUS_DECLARE_ENTRY_POINT(SetSystemDisplayFrequency);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemVSyncCount2);
	OCULUS_DECLARE_ENTRY_POINT(SetSystemVSyncCount2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemProductName2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemRegion2);
	OCULUS_DECLARE_ENTRY_POINT(ShowSystemUI2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppHasVrFocus2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppHasInputFocus);
	OCULUS_DECLARE_ENTRY_POINT(GetAppHasSystemOverlayPresent);
	OCULUS_DECLARE_ENTRY_POINT(GetAppShouldQuit2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppShouldRecenter2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppShouldRecreateDistortionWindow2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppLatencyTimings2);
	OCULUS_DECLARE_ENTRY_POINT(SetAppEngineInfo2);
	OCULUS_DECLARE_ENTRY_POINT(GetUserPresent2);
	OCULUS_DECLARE_ENTRY_POINT(GetUserIPD2);
	OCULUS_DECLARE_ENTRY_POINT(SetUserIPD2);
	OCULUS_DECLARE_ENTRY_POINT(GetUserEyeHeight2);
	OCULUS_DECLARE_ENTRY_POINT(SetUserEyeHeight2);
	OCULUS_DECLARE_ENTRY_POINT(GetUserNeckEyeDistance2);
	OCULUS_DECLARE_ENTRY_POINT(SetUserNeckEyeDistance2);
	OCULUS_DECLARE_ENTRY_POINT(SetupDisplayObjects2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemMultiViewSupported2);
	OCULUS_DECLARE_ENTRY_POINT(GetEyeTextureArraySupported2);
	OCULUS_DECLARE_ENTRY_POINT(GetBoundaryConfigured2);
	OCULUS_DECLARE_ENTRY_POINT(GetDepthCompositingSupported);
	OCULUS_DECLARE_ENTRY_POINT(TestBoundaryNode2);
	OCULUS_DECLARE_ENTRY_POINT(TestBoundaryPoint2);
	OCULUS_DECLARE_ENTRY_POINT(GetBoundaryGeometry3);
	OCULUS_DECLARE_ENTRY_POINT(GetBoundaryDimensions2);
	OCULUS_DECLARE_ENTRY_POINT(GetBoundaryVisible2);
	OCULUS_DECLARE_ENTRY_POINT(SetBoundaryVisible2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemHeadsetType2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppPerfStats2);
	OCULUS_DECLARE_ENTRY_POINT(ResetAppPerfStats2);
	OCULUS_DECLARE_ENTRY_POINT(GetAppFramerate2);
	OCULUS_DECLARE_ENTRY_POINT(IsPerfMetricsSupported);
	OCULUS_DECLARE_ENTRY_POINT(GetPerfMetricsFloat);
	OCULUS_DECLARE_ENTRY_POINT(GetPerfMetricsInt);
	OCULUS_DECLARE_ENTRY_POINT(SetHandNodePoseStateLatency);
	OCULUS_DECLARE_ENTRY_POINT(GetHandNodePoseStateLatency);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemRecommendedMSAALevel2);
	OCULUS_DECLARE_ENTRY_POINT(SetInhibitSystemUX2);
	OCULUS_DECLARE_ENTRY_POINT(GetTiledMultiResSupported);
	OCULUS_DECLARE_ENTRY_POINT(GetTiledMultiResLevel);
	OCULUS_DECLARE_ENTRY_POINT(SetTiledMultiResLevel);
	OCULUS_DECLARE_ENTRY_POINT(GetTiledMultiResDynamic);
	OCULUS_DECLARE_ENTRY_POINT(SetTiledMultiResDynamic);
	OCULUS_DECLARE_ENTRY_POINT(GetGPUUtilSupported);
	OCULUS_DECLARE_ENTRY_POINT(GetGPUUtilLevel);
	OCULUS_DECLARE_ENTRY_POINT(SetThreadPerformance);
	OCULUS_DECLARE_ENTRY_POINT(AutoThreadScheduling);
	OCULUS_DECLARE_ENTRY_POINT(GetGPUFrameTime);
	OCULUS_DECLARE_ENTRY_POINT(GetViewportStencil);
	OCULUS_DECLARE_ENTRY_POINT(SendEvent);
	OCULUS_DECLARE_ENTRY_POINT(SendEvent2);
	OCULUS_DECLARE_ENTRY_POINT(AddCustomMetadata);
	OCULUS_DECLARE_ENTRY_POINT(SetDeveloperMode);
	OCULUS_DECLARE_ENTRY_POINT(SetVrApiPropertyInt);
	OCULUS_DECLARE_ENTRY_POINT(SetVrApiPropertyFloat);
	OCULUS_DECLARE_ENTRY_POINT(GetVrApiPropertyInt);
	OCULUS_DECLARE_ENTRY_POINT(GetCurrentTrackingTransformPose);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingTransformRawPose);
	OCULUS_DECLARE_ENTRY_POINT(GetTrackingTransformRelativePose);
	OCULUS_DECLARE_ENTRY_POINT(GetTimeInSeconds);
	//OCULUS_DECLARE_ENTRY_POINT(GetPTWNear);
	OCULUS_DECLARE_ENTRY_POINT(GetASWVelocityScale);
	OCULUS_DECLARE_ENTRY_POINT(GetASWDepthScale);
	OCULUS_DECLARE_ENTRY_POINT(GetASWAdaptiveMode);
	OCULUS_DECLARE_ENTRY_POINT(SetASWAdaptiveMode);
	OCULUS_DECLARE_ENTRY_POINT(IsRequestingASWData);
	OCULUS_DECLARE_ENTRY_POINT(GetPredictedDisplayTime);
	OCULUS_DECLARE_ENTRY_POINT(GetHandTrackingEnabled);
	OCULUS_DECLARE_ENTRY_POINT(GetHandState);
	OCULUS_DECLARE_ENTRY_POINT(GetHandState2);
	OCULUS_DECLARE_ENTRY_POINT(GetSkeleton2);
	OCULUS_DECLARE_ENTRY_POINT(GetMesh);
	OCULUS_DECLARE_ENTRY_POINT(GetLocalTrackingSpaceRecenterCount);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemHmd3DofModeEnabled);
	OCULUS_DECLARE_ENTRY_POINT(SetClientColorDesc);
	OCULUS_DECLARE_ENTRY_POINT(GetHmdColorDesc);
	OCULUS_DECLARE_ENTRY_POINT(PollEvent);
	OCULUS_DECLARE_ENTRY_POINT(GetNativeXrApiType);
#ifndef OVRPLUGIN_JNI_LIB_EXCLUDED
	OCULUS_DECLARE_ENTRY_POINT(GetSystemVolume2);
	OCULUS_DECLARE_ENTRY_POINT(GetSystemHeadphonesPresent2);
#endif

	//OVR_Plugin_MixedReality.h

	OCULUS_DECLARE_ENTRY_POINT(InitializeMixedReality);
	OCULUS_DECLARE_ENTRY_POINT(ShutdownMixedReality);
	OCULUS_DECLARE_ENTRY_POINT(GetMixedRealityInitialized);
	OCULUS_DECLARE_ENTRY_POINT(UpdateExternalCamera);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraCount);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraName);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraIntrinsics);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraExtrinsics);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraCalibrationRawPose);
	OCULUS_DECLARE_ENTRY_POINT(OverrideExternalCameraFov);
	OCULUS_DECLARE_ENTRY_POINT(GetUseOverriddenExternalCameraFov);
	OCULUS_DECLARE_ENTRY_POINT(OverrideExternalCameraStaticPose);
	OCULUS_DECLARE_ENTRY_POINT(GetUseOverriddenExternalCameraStaticPose);
	OCULUS_DECLARE_ENTRY_POINT(GetExternalCameraPose);
	OCULUS_DECLARE_ENTRY_POINT(ConvertPoseToCameraSpace);
	OCULUS_DECLARE_ENTRY_POINT(ResetDefaultExternalCamera);
	OCULUS_DECLARE_ENTRY_POINT(SetDefaultExternalCamera);
	OCULUS_DECLARE_ENTRY_POINT(EnumerateAllCameraDevices);
	OCULUS_DECLARE_ENTRY_POINT(EnumerateAvailableCameraDevices);
	OCULUS_DECLARE_ENTRY_POINT(UpdateCameraDevices);
	OCULUS_DECLARE_ENTRY_POINT(IsCameraDeviceAvailable2);
	OCULUS_DECLARE_ENTRY_POINT(SetCameraDevicePreferredColorFrameSize);
	OCULUS_DECLARE_ENTRY_POINT(OpenCameraDevice);
	OCULUS_DECLARE_ENTRY_POINT(CloseCameraDevice);
	OCULUS_DECLARE_ENTRY_POINT(HasCameraDeviceOpened2);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceIntrinsicsParameters);
	OCULUS_DECLARE_ENTRY_POINT(IsCameraDeviceColorFrameAvailable2);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceColorFrameSize);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceColorFrameBgraPixels);
	OCULUS_DECLARE_ENTRY_POINT(DoesCameraDeviceSupportDepth);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceDepthSensingMode);
	OCULUS_DECLARE_ENTRY_POINT(SetCameraDeviceDepthSensingMode);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDevicePreferredDepthQuality);
	OCULUS_DECLARE_ENTRY_POINT(SetCameraDevicePreferredDepthQuality);
	OCULUS_DECLARE_ENTRY_POINT(IsCameraDeviceDepthFrameAvailable);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceDepthFrameSize);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceDepthFramePixels);
	OCULUS_DECLARE_ENTRY_POINT(GetCameraDeviceDepthConfidencePixels);

	// OVR_Plugin_Media.h

	OCULUS_DECLARE_ENTRY_POINT(Media_Initialize);
	OCULUS_DECLARE_ENTRY_POINT(Media_Shutdown);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetInitialized);
	OCULUS_DECLARE_ENTRY_POINT(Media_Update);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcActivationMode);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcActivationMode);
	OCULUS_DECLARE_ENTRY_POINT(Media_IsMrcEnabled);
	OCULUS_DECLARE_ENTRY_POINT(Media_IsMrcActivated);
	OCULUS_DECLARE_ENTRY_POINT(Media_UseMrcDebugCamera);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcInputVideoBufferType);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcInputVideoBufferType);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcFrameSize);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcFrameSize);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcAudioSampleRate);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcAudioSampleRate);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcFrameImageFlipped);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcFrameImageFlipped);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetMrcFrameInverseAlpha);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetMrcFrameInverseAlpha);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetAvailableQueueIndexVulkan);
	OCULUS_DECLARE_ENTRY_POINT(Media_EncodeMrcFrame);
	OCULUS_DECLARE_ENTRY_POINT(Media_EncodeMrcFrameWithDualTextures);
	OCULUS_DECLARE_ENTRY_POINT(Media_SyncMrcFrame);
	OCULUS_DECLARE_ENTRY_POINT(Media_EncodeMrcFrameWithPoseTime);
	OCULUS_DECLARE_ENTRY_POINT(Media_EncodeMrcFrameDualTexturesWithPoseTime);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetHeadsetControllerPose);
	OCULUS_DECLARE_ENTRY_POINT(Media_EnumerateCameraAnchorHandles);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCurrentCameraAnchorHandle);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCameraAnchorName);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCameraAnchorHandle);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCameraAnchorType);
	OCULUS_DECLARE_ENTRY_POINT(Media_CreateCustomCameraAnchor);
	OCULUS_DECLARE_ENTRY_POINT(Media_DestroyCustomCameraAnchor);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCustomCameraAnchorPose);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetCustomCameraAnchorPose);
	OCULUS_DECLARE_ENTRY_POINT(Media_GetCameraMinMaxDistance);
	OCULUS_DECLARE_ENTRY_POINT(Media_SetCameraMinMaxDistance);
};

#undef OCULUS_DECLARE_ENTRY_POINT

bool InitializeOculusPluginWrapper(OculusPluginWrapper* wrapper);
void DestroyOculusPluginWrapper(OculusPluginWrapper* wrapper);