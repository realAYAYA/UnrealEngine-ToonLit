// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusPluginWrapper.h"
#include "OculusHMDModule.h"

#if PLATFORM_ANDROID
#include <dlfcn.h>
#endif

DEFINE_LOG_CATEGORY(LogOculusPluginWrapper);

static void* LoadEntryPoint(void* handle, const char* EntryPointName);

bool InitializeOculusPluginWrapper(OculusPluginWrapper* wrapper)
{
	if (wrapper->Initialized)
	{
		UE_LOG(LogOculusPluginWrapper, Warning, TEXT("wrapper already initialized"));
		return true;
	}

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	void* LibraryHandle = nullptr;

#if PLATFORM_ANDROID
	const bool VersionValid = FAndroidMisc::GetAndroidBuildVersion() > 23;
#else
	const bool VersionValid = true;
#endif

	if (VersionValid)
	{
		LibraryHandle = FOculusHMDModule::GetOVRPluginHandle();
		if (LibraryHandle == nullptr)
		{
			UE_LOG(LogOculusPluginWrapper, Warning, TEXT("GetOVRPluginHandle() returned NULL"));
			return false;
		}
	}
	else
	{
		return false;
	}
#else
	return false;
#endif


	struct OculusEntryPoint
	{
		const char* EntryPointName;
		void** EntryPointPtr;
	};

#define OCULUS_BIND_ENTRY_POINT(Func)	{ "ovrp_"#Func, (void**)&wrapper->Func }

	OculusEntryPoint entryPointArray[] =
	{
		// OVR_Plugin.h

		OCULUS_BIND_ENTRY_POINT(PreInitialize4),
		OCULUS_BIND_ENTRY_POINT(GetInitialized),
		OCULUS_BIND_ENTRY_POINT(Initialize6),
		OCULUS_BIND_ENTRY_POINT(Shutdown2),
		OCULUS_BIND_ENTRY_POINT(GetVersion2),
		OCULUS_BIND_ENTRY_POINT(GetNativeSDKVersion2),
		OCULUS_BIND_ENTRY_POINT(GetNativeSDKPointer2),
		OCULUS_BIND_ENTRY_POINT(GetDisplayAdapterId2),
		OCULUS_BIND_ENTRY_POINT(GetAudioOutId2),
		OCULUS_BIND_ENTRY_POINT(GetAudioOutDeviceId2),
		OCULUS_BIND_ENTRY_POINT(GetAudioInId2),
		OCULUS_BIND_ENTRY_POINT(GetAudioInDeviceId2),
		OCULUS_BIND_ENTRY_POINT(GetInstanceExtensionsVk),
		OCULUS_BIND_ENTRY_POINT(GetDeviceExtensionsVk),
		OCULUS_BIND_ENTRY_POINT(SetupDistortionWindow3),
		OCULUS_BIND_ENTRY_POINT(DestroyDistortionWindow2),
		OCULUS_BIND_ENTRY_POINT(GetDominantHand),
		OCULUS_BIND_ENTRY_POINT(SetRemoteHandedness),
		OCULUS_BIND_ENTRY_POINT(SetColorScaleAndOffset),
		OCULUS_BIND_ENTRY_POINT(SetupLayer),
		OCULUS_BIND_ENTRY_POINT(SetupLayerDepth),
		OCULUS_BIND_ENTRY_POINT(SetEyeFovPremultipliedAlphaMode),
		OCULUS_BIND_ENTRY_POINT(GetEyeFovLayerId),
		OCULUS_BIND_ENTRY_POINT(GetLayerTextureStageCount),
		OCULUS_BIND_ENTRY_POINT(GetLayerTexture2),
		OCULUS_BIND_ENTRY_POINT(GetLayerTextureFoveation),
		OCULUS_BIND_ENTRY_POINT(GetLayerAndroidSurfaceObject),
		OCULUS_BIND_ENTRY_POINT(GetLayerOcclusionMesh),
		OCULUS_BIND_ENTRY_POINT(DestroyLayer),
		OCULUS_BIND_ENTRY_POINT(CalculateLayerDesc),
		OCULUS_BIND_ENTRY_POINT(CalculateEyeLayerDesc2),
		OCULUS_BIND_ENTRY_POINT(CalculateEyeViewportRect),
		OCULUS_BIND_ENTRY_POINT(CalculateEyePreviewRect),
		OCULUS_BIND_ENTRY_POINT(SetupMirrorTexture2),
		OCULUS_BIND_ENTRY_POINT(DestroyMirrorTexture2),
		OCULUS_BIND_ENTRY_POINT(GetAdaptiveGpuPerformanceScale2),
		OCULUS_BIND_ENTRY_POINT(GetAppCpuStartToGpuEndTime2),
		OCULUS_BIND_ENTRY_POINT(GetEyePixelsPerTanAngleAtCenter2),
		OCULUS_BIND_ENTRY_POINT(GetHmdToEyeOffset2),
		OCULUS_BIND_ENTRY_POINT(Update3),
		OCULUS_BIND_ENTRY_POINT(WaitToBeginFrame),
		OCULUS_BIND_ENTRY_POINT(BeginFrame4),
		OCULUS_BIND_ENTRY_POINT(UpdateFoveation),
		OCULUS_BIND_ENTRY_POINT(EndFrame4),
		OCULUS_BIND_ENTRY_POINT(GetTrackingOrientationSupported2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingOrientationEnabled2),
		OCULUS_BIND_ENTRY_POINT(SetTrackingOrientationEnabled2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingPositionSupported2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingPositionEnabled2),
		OCULUS_BIND_ENTRY_POINT(SetTrackingPositionEnabled2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingIPDEnabled2),
		OCULUS_BIND_ENTRY_POINT(SetTrackingIPDEnabled2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingCalibratedOrigin2),
		OCULUS_BIND_ENTRY_POINT(SetTrackingCalibratedOrigin2),
		OCULUS_BIND_ENTRY_POINT(GetTrackingOriginType2),
		OCULUS_BIND_ENTRY_POINT(SetTrackingOriginType2),
		OCULUS_BIND_ENTRY_POINT(RecenterTrackingOrigin2),
		OCULUS_BIND_ENTRY_POINT(GetNodePresent2),
		OCULUS_BIND_ENTRY_POINT(GetNodeOrientationTracked2),
		OCULUS_BIND_ENTRY_POINT(GetNodeOrientationValid),
		OCULUS_BIND_ENTRY_POINT(GetNodePositionTracked2),
		OCULUS_BIND_ENTRY_POINT(GetNodePositionValid),
		OCULUS_BIND_ENTRY_POINT(SetNodePositionTracked2),
		OCULUS_BIND_ENTRY_POINT(GetNodePoseState3),
		OCULUS_BIND_ENTRY_POINT(GetNodePoseStateRaw),
		OCULUS_BIND_ENTRY_POINT(GetNodeFrustum2),
		OCULUS_BIND_ENTRY_POINT(SetHeadPoseModifier),
		OCULUS_BIND_ENTRY_POINT(GetHeadPoseModifier),
		OCULUS_BIND_ENTRY_POINT(GetControllerState4),
		OCULUS_BIND_ENTRY_POINT(GetActiveController2),
		OCULUS_BIND_ENTRY_POINT(GetConnectedControllers2),
		OCULUS_BIND_ENTRY_POINT(SetControllerVibration2),
		OCULUS_BIND_ENTRY_POINT(GetControllerHapticsDesc2),
		OCULUS_BIND_ENTRY_POINT(GetControllerHapticsState2),
		OCULUS_BIND_ENTRY_POINT(SetControllerHaptics2),
		OCULUS_BIND_ENTRY_POINT(GetSystemCpuLevel2),
		OCULUS_BIND_ENTRY_POINT(SetSystemCpuLevel2),
		OCULUS_BIND_ENTRY_POINT(GetAppCPUPriority2),
		OCULUS_BIND_ENTRY_POINT(SetAppCPUPriority2),
		OCULUS_BIND_ENTRY_POINT(GetSystemGpuLevel2),
		OCULUS_BIND_ENTRY_POINT(SetSystemGpuLevel2),
		OCULUS_BIND_ENTRY_POINT(GetSystemPowerSavingMode2),
		OCULUS_BIND_ENTRY_POINT(GetSystemDisplayFrequency2),
		OCULUS_BIND_ENTRY_POINT(GetSystemDisplayAvailableFrequencies),
		OCULUS_BIND_ENTRY_POINT(SetSystemDisplayFrequency),
		OCULUS_BIND_ENTRY_POINT(GetSystemVSyncCount2),
		OCULUS_BIND_ENTRY_POINT(SetSystemVSyncCount2),
		OCULUS_BIND_ENTRY_POINT(GetSystemProductName2),
		OCULUS_BIND_ENTRY_POINT(GetSystemRegion2),
		OCULUS_BIND_ENTRY_POINT(ShowSystemUI2),
		OCULUS_BIND_ENTRY_POINT(GetAppHasVrFocus2),
		OCULUS_BIND_ENTRY_POINT(GetAppHasInputFocus),
		OCULUS_BIND_ENTRY_POINT(GetAppHasSystemOverlayPresent),
		OCULUS_BIND_ENTRY_POINT(GetAppShouldQuit2),
		OCULUS_BIND_ENTRY_POINT(GetAppShouldRecenter2),
		OCULUS_BIND_ENTRY_POINT(GetAppShouldRecreateDistortionWindow2),
		OCULUS_BIND_ENTRY_POINT(GetAppLatencyTimings2),
		OCULUS_BIND_ENTRY_POINT(SetAppEngineInfo2),
		OCULUS_BIND_ENTRY_POINT(GetUserPresent2),
		OCULUS_BIND_ENTRY_POINT(GetUserIPD2),
		OCULUS_BIND_ENTRY_POINT(SetUserIPD2),
		OCULUS_BIND_ENTRY_POINT(GetUserEyeHeight2),
		OCULUS_BIND_ENTRY_POINT(SetUserEyeHeight2),
		OCULUS_BIND_ENTRY_POINT(GetUserNeckEyeDistance2),
		OCULUS_BIND_ENTRY_POINT(SetUserNeckEyeDistance2),
		OCULUS_BIND_ENTRY_POINT(SetupDisplayObjects2),
		OCULUS_BIND_ENTRY_POINT(GetSystemMultiViewSupported2),
		OCULUS_BIND_ENTRY_POINT(GetEyeTextureArraySupported2),
		OCULUS_BIND_ENTRY_POINT(GetBoundaryConfigured2),
		OCULUS_BIND_ENTRY_POINT(GetDepthCompositingSupported),
		OCULUS_BIND_ENTRY_POINT(TestBoundaryNode2),
		OCULUS_BIND_ENTRY_POINT(TestBoundaryPoint2),
		OCULUS_BIND_ENTRY_POINT(GetBoundaryGeometry3),
		OCULUS_BIND_ENTRY_POINT(GetBoundaryDimensions2),
		OCULUS_BIND_ENTRY_POINT(GetBoundaryVisible2),
		OCULUS_BIND_ENTRY_POINT(SetBoundaryVisible2),
		OCULUS_BIND_ENTRY_POINT(GetSystemHeadsetType2),
		OCULUS_BIND_ENTRY_POINT(GetAppPerfStats2),
		OCULUS_BIND_ENTRY_POINT(ResetAppPerfStats2),
		OCULUS_BIND_ENTRY_POINT(GetAppFramerate2),
		OCULUS_BIND_ENTRY_POINT(IsPerfMetricsSupported),
		OCULUS_BIND_ENTRY_POINT(GetPerfMetricsFloat),
		OCULUS_BIND_ENTRY_POINT(GetPerfMetricsInt),
		OCULUS_BIND_ENTRY_POINT(SetHandNodePoseStateLatency),
		OCULUS_BIND_ENTRY_POINT(GetHandNodePoseStateLatency),
		OCULUS_BIND_ENTRY_POINT(GetSystemRecommendedMSAALevel2),
		OCULUS_BIND_ENTRY_POINT(SetInhibitSystemUX2),
		OCULUS_BIND_ENTRY_POINT(GetTiledMultiResSupported),
		OCULUS_BIND_ENTRY_POINT(GetTiledMultiResLevel),
		OCULUS_BIND_ENTRY_POINT(SetTiledMultiResLevel),
		OCULUS_BIND_ENTRY_POINT(GetTiledMultiResDynamic),
		OCULUS_BIND_ENTRY_POINT(SetTiledMultiResDynamic),
		OCULUS_BIND_ENTRY_POINT(GetGPUUtilSupported),
		OCULUS_BIND_ENTRY_POINT(GetGPUUtilLevel),
		OCULUS_BIND_ENTRY_POINT(SetThreadPerformance),
		OCULUS_BIND_ENTRY_POINT(AutoThreadScheduling),
		OCULUS_BIND_ENTRY_POINT(GetGPUFrameTime),
		OCULUS_BIND_ENTRY_POINT(GetViewportStencil),
		OCULUS_BIND_ENTRY_POINT(SendEvent),
		OCULUS_BIND_ENTRY_POINT(SendEvent2),
		OCULUS_BIND_ENTRY_POINT(AddCustomMetadata),
		OCULUS_BIND_ENTRY_POINT(SetDeveloperMode),
		OCULUS_BIND_ENTRY_POINT(SetVrApiPropertyInt),
		OCULUS_BIND_ENTRY_POINT(SetVrApiPropertyFloat),
		OCULUS_BIND_ENTRY_POINT(GetVrApiPropertyInt),
		OCULUS_BIND_ENTRY_POINT(GetCurrentTrackingTransformPose),
		OCULUS_BIND_ENTRY_POINT(GetTrackingTransformRawPose),
		OCULUS_BIND_ENTRY_POINT(GetTrackingTransformRelativePose),
		OCULUS_BIND_ENTRY_POINT(GetTimeInSeconds),
		//OCULUS_BIND_ENTRY_POINT(GetPTWNear),
		OCULUS_BIND_ENTRY_POINT(GetASWVelocityScale),
		OCULUS_BIND_ENTRY_POINT(GetASWDepthScale),
		OCULUS_BIND_ENTRY_POINT(GetASWAdaptiveMode),
		OCULUS_BIND_ENTRY_POINT(SetASWAdaptiveMode),
		OCULUS_BIND_ENTRY_POINT(IsRequestingASWData),
		OCULUS_BIND_ENTRY_POINT(GetPredictedDisplayTime),
		OCULUS_BIND_ENTRY_POINT(GetHandTrackingEnabled),
		OCULUS_BIND_ENTRY_POINT(GetHandState),
		OCULUS_BIND_ENTRY_POINT(GetHandState2),
		OCULUS_BIND_ENTRY_POINT(GetSkeleton2),
		OCULUS_BIND_ENTRY_POINT(GetMesh),
		OCULUS_BIND_ENTRY_POINT(GetLocalTrackingSpaceRecenterCount),
		OCULUS_BIND_ENTRY_POINT(GetSystemHmd3DofModeEnabled),
		OCULUS_BIND_ENTRY_POINT(SetClientColorDesc),
		OCULUS_BIND_ENTRY_POINT(GetHmdColorDesc),
		OCULUS_BIND_ENTRY_POINT(PollEvent),
		OCULUS_BIND_ENTRY_POINT(GetNativeXrApiType),
#ifndef OVRPLUGIN_JNI_LIB_EXCLUDED
		OCULUS_BIND_ENTRY_POINT(GetSystemVolume2),
		OCULUS_BIND_ENTRY_POINT(GetSystemHeadphonesPresent2),
#endif

		// OVR_Plugin_MixedReality.h

		OCULUS_BIND_ENTRY_POINT(InitializeMixedReality),
		OCULUS_BIND_ENTRY_POINT(ShutdownMixedReality),
		OCULUS_BIND_ENTRY_POINT(GetMixedRealityInitialized),
		OCULUS_BIND_ENTRY_POINT(UpdateExternalCamera),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraCount),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraName),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraIntrinsics),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraExtrinsics),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraCalibrationRawPose),
		OCULUS_BIND_ENTRY_POINT(OverrideExternalCameraFov),
		OCULUS_BIND_ENTRY_POINT(GetUseOverriddenExternalCameraFov),
		OCULUS_BIND_ENTRY_POINT(OverrideExternalCameraStaticPose),
		OCULUS_BIND_ENTRY_POINT(GetUseOverriddenExternalCameraStaticPose),
		OCULUS_BIND_ENTRY_POINT(GetExternalCameraPose),
		OCULUS_BIND_ENTRY_POINT(ConvertPoseToCameraSpace),
		OCULUS_BIND_ENTRY_POINT(ResetDefaultExternalCamera),
		OCULUS_BIND_ENTRY_POINT(SetDefaultExternalCamera),
		OCULUS_BIND_ENTRY_POINT(EnumerateAllCameraDevices),
		OCULUS_BIND_ENTRY_POINT(EnumerateAvailableCameraDevices),
		OCULUS_BIND_ENTRY_POINT(UpdateCameraDevices),
		OCULUS_BIND_ENTRY_POINT(IsCameraDeviceAvailable2),
		OCULUS_BIND_ENTRY_POINT(SetCameraDevicePreferredColorFrameSize),
		OCULUS_BIND_ENTRY_POINT(OpenCameraDevice),
		OCULUS_BIND_ENTRY_POINT(CloseCameraDevice),
		OCULUS_BIND_ENTRY_POINT(HasCameraDeviceOpened2),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceIntrinsicsParameters),
		OCULUS_BIND_ENTRY_POINT(IsCameraDeviceColorFrameAvailable2),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceColorFrameSize),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceColorFrameBgraPixels),
		OCULUS_BIND_ENTRY_POINT(DoesCameraDeviceSupportDepth),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceDepthSensingMode),
		OCULUS_BIND_ENTRY_POINT(SetCameraDeviceDepthSensingMode),
		OCULUS_BIND_ENTRY_POINT(GetCameraDevicePreferredDepthQuality),
		OCULUS_BIND_ENTRY_POINT(SetCameraDevicePreferredDepthQuality),
		OCULUS_BIND_ENTRY_POINT(IsCameraDeviceDepthFrameAvailable),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceDepthFrameSize),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceDepthFramePixels),
		OCULUS_BIND_ENTRY_POINT(GetCameraDeviceDepthConfidencePixels),

		// OVR_Plugin_Media.h

		OCULUS_BIND_ENTRY_POINT(Media_Initialize),
		OCULUS_BIND_ENTRY_POINT(Media_Shutdown),
		OCULUS_BIND_ENTRY_POINT(Media_GetInitialized),
		OCULUS_BIND_ENTRY_POINT(Media_Update),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcActivationMode),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcActivationMode),
		OCULUS_BIND_ENTRY_POINT(Media_IsMrcEnabled),
		OCULUS_BIND_ENTRY_POINT(Media_IsMrcActivated),
		OCULUS_BIND_ENTRY_POINT(Media_UseMrcDebugCamera),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcInputVideoBufferType),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcInputVideoBufferType),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcFrameSize),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcFrameSize),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcAudioSampleRate),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcAudioSampleRate),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcFrameImageFlipped),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcFrameImageFlipped),
		OCULUS_BIND_ENTRY_POINT(Media_SetMrcFrameInverseAlpha),
		OCULUS_BIND_ENTRY_POINT(Media_GetMrcFrameInverseAlpha),
		OCULUS_BIND_ENTRY_POINT(Media_SetAvailableQueueIndexVulkan),
		OCULUS_BIND_ENTRY_POINT(Media_EncodeMrcFrame),
		OCULUS_BIND_ENTRY_POINT(Media_EncodeMrcFrameWithDualTextures),
		OCULUS_BIND_ENTRY_POINT(Media_SyncMrcFrame),
		OCULUS_BIND_ENTRY_POINT(Media_EncodeMrcFrameWithPoseTime),
		OCULUS_BIND_ENTRY_POINT(Media_EncodeMrcFrameDualTexturesWithPoseTime),
		OCULUS_BIND_ENTRY_POINT(Media_SetHeadsetControllerPose),
		OCULUS_BIND_ENTRY_POINT(Media_EnumerateCameraAnchorHandles),
		OCULUS_BIND_ENTRY_POINT(Media_GetCurrentCameraAnchorHandle),
		OCULUS_BIND_ENTRY_POINT(Media_GetCameraAnchorName),
		OCULUS_BIND_ENTRY_POINT(Media_GetCameraAnchorHandle),
		OCULUS_BIND_ENTRY_POINT(Media_GetCameraAnchorType),
		OCULUS_BIND_ENTRY_POINT(Media_CreateCustomCameraAnchor),
		OCULUS_BIND_ENTRY_POINT(Media_DestroyCustomCameraAnchor),
		OCULUS_BIND_ENTRY_POINT(Media_GetCustomCameraAnchorPose),
		OCULUS_BIND_ENTRY_POINT(Media_SetCustomCameraAnchorPose),
		OCULUS_BIND_ENTRY_POINT(Media_GetCameraMinMaxDistance),
		OCULUS_BIND_ENTRY_POINT(Media_SetCameraMinMaxDistance),
	};

#undef OCULUS_BIND_ENTRY_POINT

	bool result = true;
	for (int i = 0; i < UE_ARRAY_COUNT(entryPointArray); ++i)
	{
		*(entryPointArray[i].EntryPointPtr) = LoadEntryPoint(LibraryHandle, entryPointArray[i].EntryPointName);

		if (*entryPointArray[i].EntryPointPtr == NULL)
		{
			UE_LOG(LogOculusPluginWrapper, Error, TEXT("OculusPlugin EntryPoint could not be loaded: %s"), entryPointArray[i].EntryPointName);
			result = false;
		}
	}

	if (result)
	{
		UE_LOG(LogOculusPluginWrapper, Log, TEXT("OculusPlugin initialized successfully"));
	}
	else
	{
		DestroyOculusPluginWrapper(wrapper);
	}

	return result;
}

void DestroyOculusPluginWrapper(OculusPluginWrapper* wrapper)
{
	if (!wrapper->Initialized)
		return;

	wrapper->Reset();

	UE_LOG(LogOculusPluginWrapper, Log, TEXT("OculusPlugin destroyed successfully"));
}

static void* LoadEntryPoint(void* Handle, const char* EntryPointName)
{
	if (Handle == nullptr)
		return nullptr;

#if PLATFORM_WINDOWS
	void* ptr = GetProcAddress((HMODULE)Handle, EntryPointName);
	if (ptr == nullptr)
	{
		UE_LOG(LogOculusPluginWrapper, Error, TEXT("Unable to load entry point: %s"), ANSI_TO_TCHAR(EntryPointName));
	}
	return ptr;
#elif PLATFORM_ANDROID
	void* ptr = dlsym(Handle, EntryPointName);
	if (ptr == nullptr)
	{
		UE_LOG(LogOculusPluginWrapper, Error, TEXT("Unable to load entry point: %s, error %s"), ANSI_TO_TCHAR(EntryPointName), ANSI_TO_TCHAR(dlerror()));
	}
	return ptr;
#else
	UE_LOG(LogOculusPluginWrapper, Error, TEXT("LoadEntryPoint: Unsupported platform"));
	return nullptr;
#endif
}