// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRHMD.h"
#include "OpenXRHMD_Layer.h"
#include "OpenXRHMD_RenderBridge.h"
#include "OpenXRHMD_Swapchain.h"
#include "OpenXRCore.h"
#include "IOpenXRExtensionPlugin.h"

#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessHMD.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CString.h"
#include "Misc/CoreDelegates.h"
#include "ClearQuad.h"
#include "XRThreadUtils.h"
#include "RenderUtils.h"
#include "PipelineStateCache.h"
#include "Slate/SceneViewport.h"
#include "Engine/GameEngine.h"
#include "ARSystem.h"
#include "IHandTracker.h"
#include "PixelShaderUtils.h"
#include "GeneralProjectSettings.h"
#include "Epic_openxr.h"
#include "HDRHelper.h"
#include "Shader.h"
#include "ScreenRendering.h"
#include "DefaultStereoLayers.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#define OPENXR_PAUSED_IDLE_FPS 10
static const int64 OPENXR_SWAPCHAIN_WAIT_TIMEOUT = 100000000ll;		// 100ms in nanoseconds.

static TAutoConsoleVariable<int32> CVarOpenXRExitAppOnRuntimeDrivenSessionExit(
	TEXT("xr.OpenXRExitAppOnRuntimeDrivenSessionExit"),
	1,
	TEXT("If true, RequestExitApp will be called after we destroy the session because the state transitioned to XR_SESSION_STATE_EXITING or XR_SESSION_STATE_LOSS_PENDING and this is NOT the result of a call from the App to xrRequestExitSession.\n")
	TEXT("The aniticipated situation is that the runtime is associated with a launcher application or has a runtime UI overlay which can tell openxr to exit vr and that in that context the app should also exit.  But maybe there are cases where it should not?  Set this CVAR to make it not.\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarOpenXREnvironmentBlendMode(
	TEXT("xr.OpenXREnvironmentBlendMode"),
	0,
	TEXT("Override the XrEnvironmentBlendMode used when submitting frames. 1 = Opaque, 2 = Additive, 3 = Alpha Blend\n"),
	ECVF_Default);

namespace {
	static TSet<XrEnvironmentBlendMode> SupportedBlendModes{ XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE, XR_ENVIRONMENT_BLEND_MODE_OPAQUE };
	static TSet<XrViewConfigurationType> SupportedViewConfigurations{ XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO };

	/** Helper function for acquiring the appropriate FSceneViewport */
	FSceneViewport* FindSceneViewport()
	{
		if (!GIsEditor)
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			return GameEngine->SceneViewport.Get();
		}
	#if WITH_EDITOR
		else
		{
			UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
			FSceneViewport* PIEViewport = (FSceneViewport*)EditorEngine->GetPIEViewport();
			if (PIEViewport != nullptr && PIEViewport->IsStereoRenderingAllowed())
			{
				// PIE is setup for stereo rendering
				return PIEViewport;
			}
			else
			{
				// Check to see if the active editor viewport is drawing in stereo mode
				// @todo vreditor: Should work with even non-active viewport!
				FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
				if (EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed())
				{
					return EditorViewport;
				}
			}
		}
	#endif
		return nullptr;
	}
}

//---------------------------------------------------
// OpenXRHMD IHeadMountedDisplay Implementation
//---------------------------------------------------

bool FOpenXRHMD::FVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
#ifdef XR_USE_GRAPHICS_API_VULKAN
	if (Extensions.IsEmpty())
	{
		PFN_xrGetVulkanInstanceExtensionsKHR GetVulkanInstanceExtensionsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(Instance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&GetVulkanInstanceExtensionsKHR));

		uint32 ExtensionCount = 0;
		XR_ENSURE(GetVulkanInstanceExtensionsKHR(Instance, System, 0, &ExtensionCount, nullptr));
		Extensions.SetNum(ExtensionCount);
		XR_ENSURE(GetVulkanInstanceExtensionsKHR(Instance, System, ExtensionCount, &ExtensionCount, Extensions.GetData()));
	}

	ANSICHAR* Context = nullptr;
	for (ANSICHAR* Tok = FCStringAnsi::Strtok(Extensions.GetData(), " ", &Context); Tok != nullptr; Tok = FCStringAnsi::Strtok(nullptr, " ", &Context))
	{
		Out.Add(Tok);
	}
#endif
	return true;
}

bool FOpenXRHMD::FVulkanExtensions::GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
#ifdef XR_USE_GRAPHICS_API_VULKAN
	if (DeviceExtensions.IsEmpty())
	{
		PFN_xrGetVulkanDeviceExtensionsKHR GetVulkanDeviceExtensionsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(Instance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&GetVulkanDeviceExtensionsKHR));

		uint32 ExtensionCount = 0;
		XR_ENSURE(GetVulkanDeviceExtensionsKHR(Instance, System, 0, &ExtensionCount, nullptr));
		DeviceExtensions.SetNum(ExtensionCount);
		XR_ENSURE(GetVulkanDeviceExtensionsKHR(Instance, System, ExtensionCount, &ExtensionCount, DeviceExtensions.GetData()));
	}

	ANSICHAR* Context = nullptr;
	for (ANSICHAR* Tok = FCStringAnsi::Strtok(DeviceExtensions.GetData(), " ", &Context); Tok != nullptr; Tok = FCStringAnsi::Strtok(nullptr, " ", &Context))
	{
		Out.Add(Tok);
	}
#endif
	return true;
}

void FOpenXRHMD::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData.DeviceName = NAME_None;
	MotionControllerData.ApplicationInstanceID = FApp::GetInstanceId();
	MotionControllerData.DeviceVisualType = EXRVisualType::Controller;
	MotionControllerData.TrackingStatus = ETrackingStatus::NotTracked;
	MotionControllerData.HandIndex = Hand;
	MotionControllerData.bValid = false;

	TArray<int32> Devices;
	if (EnumerateTrackedDevices(Devices, EXRTrackedDeviceType::Controller) && Devices.IsValidIndex((int32)Hand))
	{
		FReadScopeLock SessionLock(SessionHandleMutex);
		if (Session)
		{
			XrInteractionProfileState Profile = { XR_TYPE_INTERACTION_PROFILE_STATE };
			if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(Session, GetTrackedDevicePath(Devices[(int32)Hand]), &Profile)) &&
				Profile.interactionProfile != XR_NULL_PATH)
			{
				XR_ENSURE(OpenXRPathToFName(Instance, Profile.interactionProfile, MotionControllerData.DeviceName));
			}
		}
	}

	FName HandTrackerName("OpenXRHandTracking");
	TArray<IHandTracker*> HandTrackers = IModularFeatures::Get().GetModularFeatureImplementations<IHandTracker>(IHandTracker::GetModularFeatureName());
	IHandTracker* HandTracker = nullptr;
	for (auto Itr : HandTrackers)
	{
		if (Itr->GetHandTrackerDeviceTypeName() == HandTrackerName)
		{
			HandTracker = Itr;
			break;
		}
	}

	if ((Hand == EControllerHand::Left) || (Hand == EControllerHand::Right))
	{
		FName MotionControllerName("OpenXR");
		TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
		IMotionController* MotionController = nullptr;
		for (auto Itr : MotionControllers)
		{
			if (Itr->GetMotionControllerDeviceTypeName() == MotionControllerName)
			{
				MotionController = Itr;
				break;
			}
		}

		if (MotionController)
		{
			const float WorldToMeters = GetWorldToMetersScale();

			bool bSuccess = false;
			FVector Position = FVector::ZeroVector;
			FRotator Rotation = FRotator::ZeroRotator;
			FTransform trackingToWorld = GetTrackingToWorldTransform();
			FName AimSource = Hand == EControllerHand::Left ? FName("LeftAim") : FName("RightAim");
			bSuccess = MotionController->GetControllerOrientationAndPosition(0, AimSource, Rotation, Position, WorldToMeters);
			if (bSuccess)
			{
				MotionControllerData.AimPosition = trackingToWorld.TransformPosition(Position);
				MotionControllerData.AimRotation = trackingToWorld.TransformRotation(FQuat(Rotation));
			}
			MotionControllerData.bValid |= bSuccess;

			FName GripSource = Hand == EControllerHand::Left ? FName("LeftGrip") : FName("RightGrip");
			bSuccess = MotionController->GetControllerOrientationAndPosition(0, GripSource, Rotation, Position, WorldToMeters);
			if (bSuccess)
			{
				MotionControllerData.GripPosition = trackingToWorld.TransformPosition(Position);
				MotionControllerData.GripRotation = trackingToWorld.TransformRotation(FQuat(Rotation));
			}
			MotionControllerData.bValid |= bSuccess;

			MotionControllerData.TrackingStatus = MotionController->GetControllerTrackingStatus(0, GripSource);
		}


		if (HandTracker && HandTracker->IsHandTrackingStateValid())
		{
			MotionControllerData.DeviceVisualType = EXRVisualType::Hand;

			MotionControllerData.bValid = HandTracker->GetAllKeypointStates(Hand, MotionControllerData.HandKeyPositions, MotionControllerData.HandKeyRotations, MotionControllerData.HandKeyRadii);
			check(!MotionControllerData.bValid || (MotionControllerData.HandKeyPositions.Num() == EHandKeypointCount && MotionControllerData.HandKeyRotations.Num() == EHandKeypointCount && MotionControllerData.HandKeyRadii.Num() == EHandKeypointCount));
		}	
	}

	//TODO: this is reportedly a wmr specific convenience function for rapid prototyping.  Not sure it is useful for openxr.
	MotionControllerData.bIsGrasped = false;
}

bool FOpenXRHMD::GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile)
{
	int32 DeviceId = -1;
	if (Hand == EControllerHand::HMD)
	{
		DeviceId = IXRTrackingSystem::HMDDeviceId;
	}
	else
	{
		TArray<int32> Devices;
		if (EnumerateTrackedDevices(Devices, EXRTrackedDeviceType::Controller))
		{
			if (Devices.IsValidIndex((int32)Hand))
			{
				DeviceId = Devices[(int32)Hand];
			}
		}
	}

	if (DeviceId == -1)
	{
		UE_LOG(LogHMD, Warning, TEXT("GetCurrentInteractionProfile failed because that EControllerHandValue %i does not map to a device!"), Hand);
		return false;
	}

	FReadScopeLock SessionLock(SessionHandleMutex);
	if (Session)
	{
		XrInteractionProfileState Profile{ XR_TYPE_INTERACTION_PROFILE_STATE };
		XrPath Path = GetTrackedDevicePath(DeviceId);
		XrResult Result = xrGetCurrentInteractionProfile(Session, Path, &Profile);
		if (XR_SUCCEEDED(Result))
		{
			if (Profile.interactionProfile == XR_NULL_PATH)
			{
				InteractionProfile = "";
				return true;
			}
			else
			{				
				XR_ENSURE(OpenXRPathToFString(Instance, Profile.interactionProfile, InteractionProfile));
				return true;
			}
		}
		else
		{
			FString PathStr;
			XrResult PathResult = OpenXRPathToFString(Instance, Path, PathStr);
			if (!XR_SUCCEEDED(PathResult))
			{
				PathStr = FString::Printf(TEXT("xrPathToString returned %s"), OpenXRResultToString(Result));
			}
			UE_LOG(LogHMD, Warning, TEXT("GetCurrentInteractionProfile for %i (%s) failed because xrGetCurrentInteractionProfile failed with result %s."), Hand, *PathStr, OpenXRResultToString(Result));
			return false;
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("GetCurrentInteractionProfile for %i failed because session is null!"), Hand);
		return false;
	}

}

float FOpenXRHMD::GetWorldToMetersScale() const
{
	return IsInActualRenderingThread() ? PipelinedFrameStateRendering.WorldToMetersScale : PipelinedFrameStateGame.WorldToMetersScale;
}

FVector2D FOpenXRHMD::GetPlayAreaBounds(EHMDTrackingOrigin::Type Origin) const
{
	XrReferenceSpaceType Space = XR_REFERENCE_SPACE_TYPE_STAGE;
	switch (Origin)
	{
	case EHMDTrackingOrigin::Eye:
		Space = XR_REFERENCE_SPACE_TYPE_VIEW;
		break;
	case EHMDTrackingOrigin::Floor:
		Space = XR_REFERENCE_SPACE_TYPE_LOCAL;
		break;
	case EHMDTrackingOrigin::Stage:
		Space = XR_REFERENCE_SPACE_TYPE_STAGE;
		break;
	default:
		break;
	}
	XrExtent2Df Bounds;
	const XrResult Result = xrGetReferenceSpaceBoundsRect(Session, Space, &Bounds);
	if (Result != XR_SUCCESS)
	{
		UE_LOG(LogHMD, Warning, TEXT("GetPlayAreaBounds xrGetReferenceSpaceBoundsRect with reference space %s failed with result %s. Returning zero vector."), OpenXRReferenceSpaceTypeToString(Space), OpenXRResultToString(Result));
		return FVector2D::ZeroVector;
	}
	
	Swap(Bounds.width, Bounds.height); // Convert to UE coordinate system
	return ToFVector2D(Bounds, WorldToMetersScale);
}

bool FOpenXRHMD::GetPlayAreaRect(FTransform& OutTransform, FVector2D& OutRect) const
{
	// Get the origin and the extents of the play area rect.
	// The OpenXR Stage Space defines the origin of the playable rectangle.  The origin is at the floor. xrGetReferenceSpaceBoundsRect will give you the horizontal extents.

	const FPipelinedFrameState& PipelinedState = GetPipelinedFrameStateForThread();

	{
		if (StageSpace == XR_NULL_HANDLE)
		{
			return false;
		}

		XrSpaceLocation NewLocation = { XR_TYPE_SPACE_LOCATION };
		const XrResult Result = xrLocateSpace(StageSpace, PipelinedState.TrackingSpace->Handle, PipelinedState.FrameState.predictedDisplayTime, &NewLocation);
		if (Result != XR_SUCCESS)
		{
			return false;
		}

		if (!(NewLocation.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)))
		{
			return false;
		}
		const FQuat Orientation = ToFQuat(NewLocation.pose.orientation);
		const FVector Position = ToFVector(NewLocation.pose.position, PipelinedState.WorldToMetersScale);

		FTransform TrackingToWorld = GetTrackingToWorldTransform();
		OutTransform = FTransform(Orientation, Position) * TrackingToWorld;
	}

	{
		XrExtent2Df Bounds; // width is X height is Z
		const XrResult Result = xrGetReferenceSpaceBoundsRect(Session, XR_REFERENCE_SPACE_TYPE_STAGE, &Bounds);
		if (Result != XR_SUCCESS)
		{
			return false;
		}

		OutRect = ToFVector2D(Bounds, PipelinedState.WorldToMetersScale);
	}

	return true;
}

bool FOpenXRHMD::GetTrackingOriginTransform(TEnumAsByte<EHMDTrackingOrigin::Type> Origin, FTransform& OutTransform)  const
{
	XrSpace Space = XR_NULL_HANDLE;
	switch (Origin)
	{
	case EHMDTrackingOrigin::Eye:
		{
			FReadScopeLock DeviceLock(DeviceMutex);
			if (DeviceSpaces.Num())
			{
				Space = DeviceSpaces[HMDDeviceId].Space;
			}
		}
		break;
	case EHMDTrackingOrigin::Floor:
		Space = LocalSpace;
		break;
	case EHMDTrackingOrigin::Stage:
		Space = StageSpace;
		break;
	//case EHMDTrackingOrigin::???:
		//Space = CustomSpace
		//break;
	default:
		check(false);
		break;
	}

	if (Space == XR_NULL_HANDLE)
	{
		// This space is not supported.
		return false;
	}

	const FPipelinedFrameState& PipelinedState = GetPipelinedFrameStateForThread();

	if (!PipelinedState.TrackingSpace.IsValid())
	{
		// Session is in a state where we can't locate.
		return false;
	}

	XrSpaceLocation NewLocation = { XR_TYPE_SPACE_LOCATION };
	const XrResult Result = xrLocateSpace(Space, PipelinedState.TrackingSpace->Handle, PipelinedState.FrameState.predictedDisplayTime, &NewLocation);
	if (Result != XR_SUCCESS)
	{
		return false;
	}
	if (!(NewLocation.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)))
	{
		return false;
	}
	const FQuat Orientation = ToFQuat(NewLocation.pose.orientation);
	const FVector Position = ToFVector(NewLocation.pose.position, PipelinedState.WorldToMetersScale);

	FTransform TrackingToWorld = GetTrackingToWorldTransform();
	OutTransform =  FTransform(Orientation, Position) * TrackingToWorld;

	return true;
}


FName FOpenXRHMD::GetHMDName() const
{
	return SystemProperties.systemName;
}

FString FOpenXRHMD::GetVersionString() const
{
	return FString::Printf(TEXT("%s (%d.%d.%d)"),
		UTF8_TO_TCHAR(InstanceProperties.runtimeName),
		XR_VERSION_MAJOR(InstanceProperties.runtimeVersion),
		XR_VERSION_MINOR(InstanceProperties.runtimeVersion),
		XR_VERSION_PATCH(InstanceProperties.runtimeVersion));
}

bool FOpenXRHMD::IsHMDEnabled() const
{
	return true;
}

void FOpenXRHMD::EnableHMD(bool enable)
{
}

bool FOpenXRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = UTF8_TO_TCHAR(SystemProperties.systemName);
	MonitorDesc.MonitorId = 0;

	FIntPoint RTSize = GetIdealRenderTargetSize();
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = MonitorDesc.WindowSizeX = RTSize.X;
	MonitorDesc.ResolutionY = MonitorDesc.WindowSizeY = RTSize.Y;
	return true;
}

void FOpenXRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();

	XrFovf UnifiedFov = { 0.0f };
	for (const XrView& View : FrameState.Views)
	{
		UnifiedFov.angleLeft = FMath::Min(UnifiedFov.angleLeft, View.fov.angleLeft);
		UnifiedFov.angleRight = FMath::Max(UnifiedFov.angleRight, View.fov.angleRight);
		UnifiedFov.angleUp = FMath::Max(UnifiedFov.angleUp, View.fov.angleUp);
		UnifiedFov.angleDown = FMath::Min(UnifiedFov.angleDown, View.fov.angleDown);
	}
	OutHFOVInDegrees = FMath::RadiansToDegrees(UnifiedFov.angleRight - UnifiedFov.angleLeft);
	OutVFOVInDegrees = FMath::RadiansToDegrees(UnifiedFov.angleUp - UnifiedFov.angleDown);
}

bool FOpenXRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
	}
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::Controller)
	{
		FReadScopeLock DeviceLock(DeviceMutex);

		// Skip the HMD, we already added it to the list
		for (int32 i = 1; i < DeviceSpaces.Num(); i++)
		{
			OutDevices.Add(i);
		}
	}
	return OutDevices.Num() > 0;
}

void FOpenXRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FOpenXRHMD::GetInterpupillaryDistance() const
{
	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
	if (FrameState.Views.Num() < 2)
	{
		return 0.064f;
	}

	FVector leftPos = ToFVector(FrameState.Views[0].pose.position);
	FVector rightPos = ToFVector(FrameState.Views[1].pose.position);
	return FVector::Dist(leftPos, rightPos);
}	

bool FOpenXRHMD::GetIsTracked(int32 DeviceId)
{
	// This function is called from both the game and rendering thread and each thread maintains separate pose
	// snapshots to prevent inconsistent poses (tearing) on the same frame.
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();

	if (!PipelineState.DeviceLocations.IsValidIndex(DeviceId))
	{
		return false;
	}

	const XrSpaceLocation& Location = PipelineState.DeviceLocations[DeviceId];
	return Location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT &&
		Location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
}

bool FOpenXRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	CurrentOrientation = FQuat::Identity;
	CurrentPosition = FVector::ZeroVector;

	// This function is called from both the game and rendering thread and each thread maintains separate pose
	// snapshots to prevent inconsistent poses (tearing) on the same frame.
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();

	if (!PipelineState.DeviceLocations.IsValidIndex(DeviceId))
	{
		return false;
	}

	const XrSpaceLocation& Location = PipelineState.DeviceLocations[DeviceId];
	if (Location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
	{
		CurrentOrientation = ToFQuat(Location.pose.orientation);
	}
	if (Location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
	{
		CurrentPosition = ToFVector(Location.pose.position, GetWorldToMetersScale());
	}
	return true;
}

bool FOpenXRHMD::GetPoseForTime(int32 DeviceId, FTimespan Timespan, bool& OutTimeWasUsed, FQuat& Orientation, FVector& Position, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityRadPerSec, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration, float InWorldToMetersScale)
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();

	FReadScopeLock DeviceLock(DeviceMutex);
	if (!DeviceSpaces.IsValidIndex(DeviceId))
	{
		return false;
	}

	XrTime TargetTime = ToXrTime(Timespan);

	// If TargetTime is zero just get the latest data (rather than the oldest).
	if (TargetTime == 0)
	{
		OutTimeWasUsed = false;
		TargetTime = GetDisplayTime();
	}
	else
	{
		OutTimeWasUsed = true;
	}

	const FDeviceSpace& DeviceSpace = DeviceSpaces[DeviceId];

	XrSpaceAccelerationEPIC DeviceAcceleration{ (XrStructureType)XR_TYPE_SPACE_ACCELERATION_EPIC };
	void* DeviceAccellerationPtr = bSpaceAccellerationSupported ? &DeviceAcceleration : nullptr;
	XrSpaceVelocity DeviceVelocity { XR_TYPE_SPACE_VELOCITY, DeviceAccellerationPtr };
	XrSpaceLocation DeviceLocation { XR_TYPE_SPACE_LOCATION, &DeviceVelocity };

	XR_ENSURE(xrLocateSpace(DeviceSpace.Space, PipelineState.TrackingSpace->Handle, TargetTime, &DeviceLocation));

	bool ReturnValue = false;

	if (DeviceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT &&
		DeviceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
	{
		Orientation = ToFQuat(DeviceLocation.pose.orientation);
		Position = ToFVector(DeviceLocation.pose.position, InWorldToMetersScale);

		if (DeviceVelocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)
		{
			bProvidedLinearVelocity = true;
			LinearVelocity = ToFVector(DeviceVelocity.linearVelocity, InWorldToMetersScale);
		}
		if (DeviceVelocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)
		{
			bProvidedAngularVelocity = true;
			AngularVelocityRadPerSec = -ToFVector(DeviceVelocity.angularVelocity);
		}

		if (DeviceAcceleration.accelerationFlags & XR_SPACE_ACCELERATION_LINEAR_VALID_BIT_EPIC)
		{
			bProvidedLinearAcceleration = true;
			LinearAcceleration = ToFVector(DeviceAcceleration.linearAcceleration, InWorldToMetersScale);
		}

		ReturnValue = true;
	}

	return ReturnValue;
}

bool FOpenXRHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

void FOpenXRHMD::VRHeadsetRecenterDelegate()
{
	Recenter(EOrientPositionSelector::OrientationAndPosition, 0.f);
}

void FOpenXRHMD::ResetOrientationAndPosition(float Yaw)
{
	Recenter(EOrientPositionSelector::OrientationAndPosition, Yaw);
}

void FOpenXRHMD::ResetOrientation(float Yaw)
{
	Recenter(EOrientPositionSelector::Orientation, Yaw);
}

void FOpenXRHMD::ResetPosition()
{

	Recenter(EOrientPositionSelector::Position);
}

void FOpenXRHMD::Recenter(EOrientPositionSelector::Type Selector, float Yaw)
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	const XrTime TargetTime = PipelineState.FrameState.predictedDisplayTime;
	check(PipelineState.bXrFrameStateUpdated);

	XrSpace DeviceSpace = XR_NULL_HANDLE;
	{
		FReadScopeLock DeviceLock(DeviceMutex);
		const FDeviceSpace& DeviceSpaceStruct = DeviceSpaces[HMDDeviceId];
		DeviceSpace = DeviceSpaceStruct.Space;
	}
	XrSpaceLocation DeviceLocation = { XR_TYPE_SPACE_LOCATION, nullptr };

	XrSpace BaseSpace = TrackingSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? StageSpace : LocalSpace;
	if (bUseCustomReferenceSpace)
	{
		BaseSpace = CustomSpace;
	}
	XR_ENSURE(xrLocateSpace(DeviceSpace, BaseSpace, TargetTime, &DeviceLocation));

	const FQuat CurrentOrientation = ToFQuat(DeviceLocation.pose.orientation);
	const FVector CurrentPosition = ToFVector(DeviceLocation.pose.position, GetWorldToMetersScale());

	if (Selector == EOrientPositionSelector::Position ||
		Selector == EOrientPositionSelector::OrientationAndPosition)
	{
		FVector NewPosition;
		NewPosition.X = CurrentPosition.X;
		NewPosition.Y = CurrentPosition.Y;
		if (TrackingSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)
		{
			NewPosition.Z = CurrentPosition.Z;
		}
		else
		{
			NewPosition.Z = 0.0f;
		}
		SetBasePosition(NewPosition);
	}

	if (Selector == EOrientPositionSelector::Orientation ||
		Selector == EOrientPositionSelector::OrientationAndPosition)
	{
		FRotator NewOrientation(0.0f, CurrentOrientation.Rotator().Yaw - Yaw, 0.0f);
		SetBaseOrientation(NewOrientation.Quaternion());
	}

	bTrackingSpaceInvalid = true;
}

void FOpenXRHMD::SetBaseRotation(const FRotator& InBaseRotation)
{
	SetBaseOrientation(InBaseRotation.Quaternion());
}

FRotator FOpenXRHMD::GetBaseRotation() const
{
	return BaseOrientation.Rotator();
}

void FOpenXRHMD::SetBaseOrientation(const FQuat& InBaseOrientation)
{
	BaseOrientation = InBaseOrientation;
	bTrackingSpaceInvalid = true;
}

FQuat FOpenXRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

void FOpenXRHMD::SetBasePosition(const FVector& InBasePosition)
{
	BasePosition = InBasePosition;
	bTrackingSpaceInvalid = true;
}

FVector FOpenXRHMD::GetBasePosition() const
{
	return BasePosition;
}

bool FOpenXRHMD::IsStereoEnabled() const
{
	return bStereoEnabled;
}

bool FOpenXRHMD::EnableStereo(bool stereo)
{
	if (stereo == bStereoEnabled)
	{
		return true;
	}

	bStereoEnabled = stereo;
	if (stereo)
	{
		GEngine->bForceDisableFrameRateSmoothing = true;
		if (OnStereoStartup())
		{
			if (!GIsEditor)
			{
				GEngine->SetMaxFPS(0);
			}
			StartSession();

			FApp::SetUseVRFocus(true);
			FApp::SetHasVRFocus(true);

#if WITH_EDITOR
			if (GIsEditor)
			{
				if (FSceneViewport* SceneVP = FindSceneViewport())
				{
					TSharedPtr<SWindow> Window = SceneVP->FindWindow();
					if (Window.IsValid())
					{
						uint32 SizeX = 0;
						uint32 SizeY = 0;
						CalculateRenderTargetSize(*SceneVP, SizeX, SizeY);

						// Window continues to be processed when PIE spectator window is minimized
						Window->SetIndependentViewportSize(FVector2D(SizeX, SizeY));
					}
				}
			}

#endif // WITH_EDITOR

			return true;
		}
		return false;
	}
	else
	{
		GEngine->bForceDisableFrameRateSmoothing = false;

		FApp::SetUseVRFocus(false);
		FApp::SetHasVRFocus(false);

#if WITH_EDITOR
		if (GIsEditor)
		{
			if (FSceneViewport* SceneVP = FindSceneViewport())
			{
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				if (Window.IsValid())
				{
					Window->SetViewportSizeDrivenByWindow(true);
				}
			}
		}
#endif // WITH_EDITOR

		return OnStereoTeardown();
	}
}

FIntPoint GeneratePixelDensitySize(const XrViewConfigurationView& Config, const float PixelDensity)
{
	FIntPoint DensityAdjustedSize =
	{
		FMath::CeilToInt(Config.recommendedImageRectWidth * PixelDensity),
		FMath::CeilToInt(Config.recommendedImageRectHeight * PixelDensity)
	};

	// We quantize in order to be consistent with the rest of the engine in creating our buffers.
	// Interestingly, we need to be a bit careful with this quantization during target alloc because
	// some runtime compositors want/expect targets that match the recommended size. Some runtimes
	// might blit from a 'larger' size to the recommended size. This could happen with quantization
	// factors that don't align with the recommended size.
	QuantizeSceneBufferSize(DensityAdjustedSize, DensityAdjustedSize);

	return DensityAdjustedSize;
}

void FOpenXRHMD::AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	const XrViewConfigurationView& Config = PipelineState.ViewConfigs[ViewIndex];
	FIntPoint ViewRectMin(EForceInit::ForceInitToZero);

	// If Mobile Multi-View is active the first two views will share the same position
	// Thus the start index should be the second view if enabled
	for (int32 i = bIsMobileMultiViewEnabled ? 1 : 0; i < ViewIndex; ++i)
	{
		ViewRectMin.X += FMath::CeilToInt(PipelineState.ViewConfigs[i].recommendedImageRectWidth * PipelineState.PixelDensity);
		QuantizeSceneBufferSize(ViewRectMin, ViewRectMin);
	}

	X = ViewRectMin.X;
	Y = ViewRectMin.Y;

	const FIntPoint DensityAdjustedSize = GeneratePixelDensitySize(Config, PipelineState.PixelDensity);

	SizeX = DensityAdjustedSize.X;
	SizeY = DensityAdjustedSize.Y;

}

void FOpenXRHMD::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread() || IsInRenderingThread());

	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	const float PixelDensity = PipelineState.PixelDensity;

	// TODO: Could we just call AdjustViewRect per view, or even for _only_ the last view?
	FIntPoint Size(EForceInit::ForceInitToZero);
	for (int32 ViewIndex = 0; ViewIndex < PipelineState.ViewConfigs.Num(); ViewIndex++)
	{
		const XrViewConfigurationView& Config = PipelineState.ViewConfigs[ViewIndex];

		// If Mobile Multi-View is active the first two views will share the same position
		// TODO: This is weird logic that we should re-investigate. It makes sense for AdjustViewRect, but not
		// for the 'size' of an RT.
		const bool bMMVView = bIsMobileMultiViewEnabled && ViewIndex < 2;

		const FIntPoint DensityAdjustedSize = GeneratePixelDensitySize(Config, PipelineState.PixelDensity);
		Size.X = bMMVView ? FMath::Max(Size.X, DensityAdjustedSize.X) : Size.X + DensityAdjustedSize.X;
		Size.Y = FMath::Max(Size.Y, DensityAdjustedSize.Y);
	}

	InOutSizeX = Size.X;
	InOutSizeY = Size.Y;

	check(InOutSizeX != 0 && InOutSizeY != 0);
}

void FOpenXRHMD::SetFinalViewRect(FRHICommandListImmediate& RHICmdList, const int32 ViewIndex, const FIntRect& FinalViewRect)
{
	check(IsInRenderingThread());

	if (ViewIndex == INDEX_NONE || !PipelinedLayerStateRendering.ColorImages.IsValidIndex(ViewIndex))
	{
		return;
	}

	XrSwapchainSubImage& ColorImage = PipelinedLayerStateRendering.ColorImages[ViewIndex];
	ColorImage.imageArrayIndex = bIsMobileMultiViewEnabled && ViewIndex < 2 ? ViewIndex : 0;
	ColorImage.imageRect = {
		{ FinalViewRect.Min.X, FinalViewRect.Min.Y },
		{ FinalViewRect.Width(), FinalViewRect.Height() }
	};

	XrSwapchainSubImage& DepthImage = PipelinedLayerStateRendering.DepthImages[ViewIndex];
	if (bDepthExtensionSupported)
	{
		DepthImage.imageArrayIndex = bIsMobileMultiViewEnabled && ViewIndex < 2 ? ViewIndex : 0;
		DepthImage.imageRect = ColorImage.imageRect;
	}
}

EStereoscopicPass FOpenXRHMD::GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const
{
	if (!bStereoRequested)
		return EStereoscopicPass::eSSP_FULL;

	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	if (PipelineState.PluginViewInfos.IsValidIndex(ViewIndex) && PipelineState.PluginViewInfos[ViewIndex].bIsPluginManaged)
	{
		return PipelineState.PluginViewInfos[ViewIndex].PassType;
	}

	if (SelectedViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO)
	{
		return ViewIndex % 2 == 0 ? EStereoscopicPass::eSSP_PRIMARY : EStereoscopicPass::eSSP_SECONDARY;
	}
	return ViewIndex == EStereoscopicEye::eSSE_LEFT_EYE ? EStereoscopicPass::eSSP_PRIMARY : EStereoscopicPass::eSSP_SECONDARY;
}

uint32 FOpenXRHMD::GetLODViewIndex() const
{
	if (SelectedViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO)
	{
		return EStereoscopicEye::eSSE_LEFT_EYE_SIDE;
	}
	return IStereoRendering::GetLODViewIndex();
}

int32 FOpenXRHMD::GetDesiredNumberOfViews(bool bStereoRequested) const
{
	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();

	// FIXME: Monoscopic actually needs 2 views for quad vr
	return bStereoRequested ? FrameState.ViewConfigs.Num() : 1;
}

bool FOpenXRHMD::GetRelativeEyePose(int32 InDeviceId, int32 InViewIndex, FQuat& OutOrientation, FVector& OutPosition)
{
	if (InDeviceId != IXRTrackingSystem::HMDDeviceId)
	{
		return false;
	}

	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();

	if (FrameState.ViewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT &&
		FrameState.ViewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
		FrameState.Views.IsValidIndex(InViewIndex))
	{
		OutOrientation = ToFQuat(FrameState.Views[InViewIndex].pose.orientation);
		OutPosition = ToFVector(FrameState.Views[InViewIndex].pose.position, GetWorldToMetersScale());
		return true;
	}

	return false;
}

FMatrix FOpenXRHMD::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();

	XrFovf Fov = {};
	if (ViewIndex == eSSE_MONOSCOPIC)
	{
		// The monoscopic projection matrix uses the combined field-of-view of both eyes
		for (int32 Index = 0; Index < FrameState.Views.Num(); Index++)
		{
			const XrFovf& ViewFov = FrameState.Views[Index].fov;
			Fov.angleUp = FMath::Max(Fov.angleUp, ViewFov.angleUp);
			Fov.angleDown = FMath::Min(Fov.angleDown, ViewFov.angleDown);
			Fov.angleLeft = FMath::Min(Fov.angleLeft, ViewFov.angleLeft);
			Fov.angleRight = FMath::Max(Fov.angleRight, ViewFov.angleRight);
		}
	}
	else
	{
		Fov = (ViewIndex < FrameState.Views.Num()) ? FrameState.Views[ViewIndex].fov
			: XrFovf{ -PI / 4.0f, PI / 4.0f, PI / 4.0f, -PI / 4.0f };
	}

	Fov.angleUp = tan(Fov.angleUp);
	Fov.angleDown = tan(Fov.angleDown);
	Fov.angleLeft = tan(Fov.angleLeft);
	Fov.angleRight = tan(Fov.angleRight);

	float ZNear = GNearClippingPlane;
	float SumRL = (Fov.angleRight + Fov.angleLeft);
	float SumTB = (Fov.angleUp + Fov.angleDown);
	float InvRL = (1.0f / (Fov.angleRight - Fov.angleLeft));
	float InvTB = (1.0f / (Fov.angleUp - Fov.angleDown));

	FMatrix Mat = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * -InvRL), (SumTB * -InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
	);

	return Mat;
}

void FOpenXRHMD::GetEyeRenderParams_RenderThread(const FHeadMountedDisplayPassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
	EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
}


void FOpenXRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();

	const FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
	if (FrameState.Views.Num() > 2)
	{
		InViewFamily.EngineShowFlags.Vignette = 0;
		InViewFamily.EngineShowFlags.Bloom = 0;
	}
}

void FOpenXRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FOpenXRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	uint32 ViewConfigCount = 0;
	XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, SelectedViewConfigurationType, 0, &ViewConfigCount, nullptr));

	PipelinedLayerStateRendering.ProjectionLayers.SetNum(ViewConfigCount);
	PipelinedLayerStateRendering.DepthLayers.SetNum(ViewConfigCount);

	PipelinedLayerStateRendering.ColorImages.SetNum(PipelinedFrameStateRendering.ViewConfigs.Num());
	PipelinedLayerStateRendering.DepthImages.SetNum(PipelinedFrameStateRendering.ViewConfigs.Num());

	if (SpectatorScreenController)
	{
		SpectatorScreenController->BeginRenderViewFamily();
	}
}

void FOpenXRHMD::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	check(IsInRenderingThread());
}

void FOpenXRHMD::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	DrawEmulatedQuadLayers_RenderThread(GraphBuilder, InView);
}

void FOpenXRHMD::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	if (SpectatorScreenController)
	{
		SpectatorScreenController->UpdateSpectatorScreenMode_RenderThread();
	}
}

void FOpenXRHMD::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());

	const float NearZ = GNearClippingPlane / GetWorldToMetersScale();

	for (int32 ViewIndex = 0; ViewIndex < PipelinedLayerStateRendering.ColorImages.Num(); ViewIndex++)
	{
		if (!PipelinedLayerStateRendering.ColorImages.IsValidIndex(ViewIndex))
		{
			continue;
		}

		// Update SubImages with latest swapchain
		XrSwapchainSubImage& ColorImage = PipelinedLayerStateRendering.ColorImages[ViewIndex];
		XrSwapchainSubImage& DepthImage = PipelinedLayerStateRendering.DepthImages[ViewIndex];

		ColorImage.swapchain = PipelinedLayerStateRendering.ColorSwapchain.IsValid() ? static_cast<FOpenXRSwapchain*>(PipelinedLayerStateRendering.ColorSwapchain.Get())->GetHandle() : XR_NULL_HANDLE;
		if (bDepthExtensionSupported)
		{
			DepthImage.swapchain = PipelinedLayerStateRendering.DepthSwapchain.IsValid() ? static_cast<FOpenXRSwapchain*>(PipelinedLayerStateRendering.DepthSwapchain.Get())->GetHandle() : XR_NULL_HANDLE;
		}

		if (IsViewManagedByPlugin(ViewIndex))
		{
			// Plugin owns further usage of the subimage, so we don't use the subimages in our layers
			continue;
		}

		XrCompositionLayerProjectionView& Projection = PipelinedLayerStateRendering.ProjectionLayers[ViewIndex];

		Projection.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		Projection.next = nullptr;
		Projection.subImage = ColorImage;

		if (bDepthExtensionSupported && PipelinedLayerStateRendering.DepthSwapchain.IsValid())
		{
			XrCompositionLayerDepthInfoKHR& DepthLayer = PipelinedLayerStateRendering.DepthLayers[ViewIndex];

			DepthLayer.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
			DepthLayer.next = nullptr;
			DepthLayer.subImage = DepthImage;
			DepthLayer.minDepth = 0.0f;
			DepthLayer.maxDepth = 1.0f;
			DepthLayer.nearZ = FLT_MAX;
			DepthLayer.farZ = NearZ;

			for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
			{
				DepthLayer.next = Module->OnBeginDepthInfo(Session, 0, ViewIndex, DepthLayer.next);
			}

			Projection.next = &DepthLayer;
		}

		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			Projection.next = Module->OnBeginProjectionView(Session, 0, ViewIndex, Projection.next);
		}
	}

	// We use RHICmdList directly, though eventually, we might want to schedule on GraphBuilder
	GraphBuilder.RHICmdList.EnqueueLambda([this, LayerState = PipelinedLayerStateRendering](FRHICommandListImmediate&)
	{
		PipelinedLayerStateRHI = LayerState;
	});
}

bool FOpenXRHMD::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	// Don't activate the SVE if xr is being used for tracking only purposes
	static const bool bXrTrackingOnly = FParse::Param(FCommandLine::Get(), TEXT("xrtrackingonly"));

	return FHMDSceneViewExtension::IsActiveThisFrame_Internal(Context) && !bXrTrackingOnly;
}

bool CheckPlatformDepthExtensionSupport(const XrInstanceProperties& InstanceProps)
{
	if (FCStringAnsi::Strstr(InstanceProps.runtimeName, "SteamVR/OpenXR") && RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
	{
		return false;
	}
	else if (FCStringAnsi::Strstr(InstanceProps.runtimeName, "Oculus") && RHIGetInterfaceType() == ERHIInterfaceType::D3D12)
	{
		// No PF_DepthStencil compatible formats offered yet
		return false;
	}
	return true;
}

FOpenXRHMD::FOpenXRHMD(const FAutoRegister& AutoRegister, XrInstance InInstance, XrSystemId InSystem, TRefCountPtr<FOpenXRRenderBridge>& InRenderBridge, TArray<const char*> InEnabledExtensions, TArray<IOpenXRExtensionPlugin*> InExtensionPlugins, IARSystemSupport* ARSystemSupport)
	: FHeadMountedDisplayBase(ARSystemSupport)
	, FHMDSceneViewExtension(AutoRegister)
	, FOpenXRAssetManager(InInstance, this)
	, bStereoEnabled(false)
	, bIsRunning(false)
	, bIsReady(false)
	, bIsRendering(false)
	, bIsSynchronized(false)
	, bShouldWait(true)
	, bIsExitingSessionByxrRequestExitSession(false)
	, bNeedReAllocatedDepth(false)
	, bNeedReBuildOcclusionMesh(true)
	, bIsMobileMultiViewEnabled(false)
	, bSupportsHandTracking(false)
	, bIsStandaloneStereoOnlyDevice(false)
	, CurrentSessionState(XR_SESSION_STATE_UNKNOWN)
	, EnabledExtensions(std::move(InEnabledExtensions))
	, InputModule(nullptr)
	, ExtensionPlugins(std::move(InExtensionPlugins))
	, Instance(InInstance)
	, System(InSystem)
	, Session(XR_NULL_HANDLE)
	, LocalSpace(XR_NULL_HANDLE)
	, StageSpace(XR_NULL_HANDLE)
	, CustomSpace(XR_NULL_HANDLE)
	, TrackingSpaceType(XR_REFERENCE_SPACE_TYPE_STAGE)
	, SelectedViewConfigurationType(XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM)
	, SelectedEnvironmentBlendMode(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
	, RenderBridge(InRenderBridge)
	, RendererModule(nullptr)
	, LastRequestedColorSwapchainFormat(0)
	, LastActualColorSwapchainFormat(0)
	, LastRequestedDepthSwapchainFormat(PF_DepthStencil)
	, bTrackingSpaceInvalid(true)
	, bUseCustomReferenceSpace(false)
	, BaseOrientation(FQuat::Identity)
	, BasePosition(FVector::ZeroVector)
{
	InstanceProperties = { XR_TYPE_INSTANCE_PROPERTIES, nullptr };
	XR_ENSURE(xrGetInstanceProperties(Instance, &InstanceProperties));
	InstanceProperties.runtimeName[XR_MAX_RUNTIME_NAME_SIZE - 1] = 0; // Ensure the name is null terminated.

	bDepthExtensionSupported = IsExtensionEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) && CheckPlatformDepthExtensionSupport(InstanceProperties);
	bHiddenAreaMaskSupported = IsExtensionEnabled(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME) &&
		!FCStringAnsi::Strstr(InstanceProperties.runtimeName, "Oculus");
	bViewConfigurationFovSupported = IsExtensionEnabled(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);

	// Retrieve system properties and check for hand tracking support
	XrSystemHandTrackingPropertiesEXT HandTrackingSystemProperties = { XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
	SystemProperties = XrSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES, &HandTrackingSystemProperties };
	XR_ENSURE(xrGetSystemProperties(Instance, System, &SystemProperties));
	bSupportsHandTracking = HandTrackingSystemProperties.supportsHandTracking == XR_TRUE;
	SystemProperties.next = nullptr;

	// Some runtimes aren't compliant with their number of layers supported.
	// We support a fallback by emulating non-facelocked layers
	bNativeWorldQuadLayerSupport = SystemProperties.graphicsProperties.maxLayerCount >= XR_MIN_COMPOSITION_LAYERS_SUPPORTED;

	bSpaceAccellerationSupported = IsExtensionEnabled(XR_EPIC_SPACE_ACCELERATION_NAME);

	static const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
	static const auto CVarMobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	const bool bMobileHDR = (CVarMobileHDR && CVarMobileHDR->GetValueOnAnyThread() != 0);
	const bool bMobileMultiView = !bMobileHDR && (CVarMobileMultiView && CVarMobileMultiView->GetValueOnAnyThread() != 0);
#if PLATFORM_HOLOLENS
	bIsMobileMultiViewEnabled = bMobileMultiView && GRHISupportsArrayIndexFromAnyShader;
#else
	bIsMobileMultiViewEnabled = bMobileMultiView && RHISupportsMobileMultiView(GMaxRHIShaderPlatform);
#endif

	static const auto CVarPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
	bProjectionLayerAlphaEnabled = !IsMobilePlatform(GMaxRHIShaderPlatform) && CVarPropagateAlpha->GetValueOnAnyThread() != 0;

	// Enumerate the viewport configurations
	uint32 ConfigurationCount;
	TArray<XrViewConfigurationType> ViewConfigTypes;
	XR_ENSURE(xrEnumerateViewConfigurations(Instance, System, 0, &ConfigurationCount, nullptr));
	ViewConfigTypes.SetNum(ConfigurationCount);
	// Fill the initial array with valid enum types (this will fail in the validation layer otherwise).
	for (auto & TypeIter : ViewConfigTypes)
		TypeIter = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
	XR_ENSURE(xrEnumerateViewConfigurations(Instance, System, ConfigurationCount, &ConfigurationCount, ViewConfigTypes.GetData()));

	// Select the first view configuration returned by the runtime that is supported.
	// This is the view configuration preferred by the runtime.
	for (XrViewConfigurationType ViewConfigType : ViewConfigTypes)
	{
		if (SupportedViewConfigurations.Contains(ViewConfigType))
		{
			SelectedViewConfigurationType = ViewConfigType;
			break;
		}
	}

	// If there is no supported view configuration type, use the first option as a last resort.
	if (!ensure(SelectedViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM))
	{
		UE_LOG(LogHMD, Error, TEXT("No compatible view configuration type found, falling back to runtime preferred type."));
		SelectedViewConfigurationType = ViewConfigTypes[0];
	}

	// Enumerate the views we will be simulating with.
	EnumerateViews(PipelinedFrameStateGame);

	for (const XrViewConfigurationView& Config : PipelinedFrameStateGame.ViewConfigs)
	{
		const float WidthDensityMax = float(Config.maxImageRectWidth) / Config.recommendedImageRectWidth;
		const float HeightDensitymax = float(Config.maxImageRectHeight) / Config.recommendedImageRectHeight;
		const float PerViewPixelDensityMax = FMath::Min(WidthDensityMax, HeightDensitymax);
		RuntimePixelDensityMax = FMath::Min(RuntimePixelDensityMax, PerViewPixelDensityMax);
	}

	// Enumerate environment blend modes and select the best one.
	{
		uint32 BlendModeCount;
		TArray<XrEnvironmentBlendMode> BlendModes;
		XR_ENSURE(xrEnumerateEnvironmentBlendModes(Instance, System, SelectedViewConfigurationType, 0, &BlendModeCount, nullptr));
		// Fill the initial array with valid enum types (this will fail in the validation layer otherwise).
		for (auto& TypeIter : BlendModes)
			TypeIter = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		BlendModes.SetNum(BlendModeCount);
		XR_ENSURE(xrEnumerateEnvironmentBlendModes(Instance, System, SelectedViewConfigurationType, BlendModeCount, &BlendModeCount, BlendModes.GetData()));

		// Select the first blend mode returned by the runtime that is supported.
		// This is the environment blend mode preferred by the runtime.
		for (XrEnvironmentBlendMode BlendMode : BlendModes)
		{
			if (SupportedBlendModes.Contains(BlendMode) &&
				// On mobile platforms the alpha channel can contain depth information, so we can't use alpha blend.
				(BlendMode != XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND || !IsMobilePlatform(GMaxRHIShaderPlatform)))
			{
				SelectedEnvironmentBlendMode = BlendMode;
				break;
			}
		}

		// If there is no supported environment blend mode, use the first option as a last resort.
		if (!ensure(SelectedEnvironmentBlendMode != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM))
		{
			SelectedEnvironmentBlendMode = BlendModes[0];
		}
	}

#if PLATFORM_HOLOLENS || PLATFORM_ANDROID
	bool bStartInVR = false;
	GConfig->GetBool(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("bStartInVR"), bStartInVR, GGameIni);
	bIsStandaloneStereoOnlyDevice = FParse::Param(FCommandLine::Get(), TEXT("vr")) || bStartInVR;
#else
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		if (Module->IsStandaloneStereoOnlyDevice())
		{
			bIsStandaloneStereoOnlyDevice = true;
		}
	}
#endif

	// Add a device space for the HMD without an action handle and ensure it has the correct index
	XrPath UserHead = XR_NULL_PATH;
	XR_ENSURE(xrStringToPath(Instance, "/user/head", &UserHead));
	ensure(DeviceSpaces.Emplace(XR_NULL_HANDLE, UserHead) == HMDDeviceId);

	// Give the all frame states the same initial values.
	PipelinedFrameStateRHI = PipelinedFrameStateRendering = PipelinedFrameStateGame;

	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		Module->BindExtensionPluginDelegates(*this);
	}
}

FOpenXRHMD::~FOpenXRHMD()
{
	DestroySession();
}

const FOpenXRHMD::FPipelinedFrameState& FOpenXRHMD::GetPipelinedFrameStateForThread() const
{
	// Relying on implicit selection of the RHI struct is hazardous since the RHI thread isn't always present
	check(!IsInRHIThread());

	if (IsInActualRenderingThread())
	{
		return PipelinedFrameStateRendering;
	}
	else
	{
		check(IsInGameThread());
		return PipelinedFrameStateGame;
	}
}

FOpenXRHMD::FPipelinedFrameState& FOpenXRHMD::GetPipelinedFrameStateForThread()
{
	// Relying on implicit selection of the RHI struct is hazardous since the RHI thread isn't always present
	check(!IsInRHIThread());

	if (IsInActualRenderingThread())
	{
		return PipelinedFrameStateRendering;
	}
	else
	{
		check(IsInGameThread());
		return PipelinedFrameStateGame;
	}
}

void FOpenXRHMD::UpdateDeviceLocations(bool bUpdateOpenXRExtensionPlugins)
{
	SCOPED_NAMED_EVENT(UpdateDeviceLocations, FColor::Red);

	FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();

	// Only update the device locations if the frame state has been predicted, which is dependent on WaitFrame success
	// Also need a valid TrackingSpace
	if (PipelineState.bXrFrameStateUpdated && PipelineState.TrackingSpace.IsValid())
	{
		FReadScopeLock Lock(DeviceMutex);
		PipelineState.DeviceLocations.SetNumZeroed(DeviceSpaces.Num());
		for (int32 DeviceIndex = 0; DeviceIndex < PipelineState.DeviceLocations.Num(); DeviceIndex++)
		{
			const FDeviceSpace& DeviceSpace = DeviceSpaces[DeviceIndex];
			XrSpaceLocation& CachedDeviceLocation = PipelineState.DeviceLocations[DeviceIndex];
			CachedDeviceLocation.type = XR_TYPE_SPACE_LOCATION;

			if (DeviceSpace.Space != XR_NULL_HANDLE)
			{
				XrSpaceLocation NewDeviceLocation = { XR_TYPE_SPACE_LOCATION };
				XrResult Result = xrLocateSpace(DeviceSpace.Space, PipelineState.TrackingSpace->Handle, PipelineState.FrameState.predictedDisplayTime, &NewDeviceLocation);
				if (Result == XR_ERROR_TIME_INVALID)
				{
					// The display time is no longer valid so set the location as invalid as well
					PipelineState.DeviceLocations[DeviceIndex].locationFlags = 0;
				}
				else
				{
					XR_ENSURE(Result);
				}
				
				// Clear the location tracked bits
				CachedDeviceLocation.locationFlags &= ~(XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT);
				if (NewDeviceLocation.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT))
				{
					CachedDeviceLocation.pose.position = NewDeviceLocation.pose.position;
					CachedDeviceLocation.locationFlags |= (NewDeviceLocation.locationFlags & (XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT));
				}
				if (NewDeviceLocation.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
				{
					CachedDeviceLocation.pose.orientation = NewDeviceLocation.pose.orientation;
					CachedDeviceLocation.locationFlags |= (NewDeviceLocation.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT));
				}
			}
			else
			{
				// Ensure the location flags are zeroed out so the pose is detected as invalid
				CachedDeviceLocation.locationFlags = 0;
			}
		}

		if (bUpdateOpenXRExtensionPlugins)
		{
			for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
			{
				Module->UpdateDeviceLocations(Session, PipelineState.FrameState.predictedDisplayTime, PipelineState.TrackingSpace->Handle);
			}
		}
	}
}

void FOpenXRHMD::EnumerateViews(FPipelinedFrameState& PipelineState)
{
	SCOPED_NAMED_EVENT(EnumerateViews, FColor::Red);

	// Enumerate the viewport configuration views
	uint32 ViewConfigCount = 0;
	TArray<XrViewConfigurationViewFovEPIC> ViewFov;
	XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, SelectedViewConfigurationType, 0, &ViewConfigCount, nullptr));
	ViewFov.SetNum(ViewConfigCount);
	PipelineState.ViewConfigs.Empty(ViewConfigCount);
	PipelineState.PluginViewInfos.Empty(ViewConfigCount);
	for (uint32 ViewIndex = 0; ViewIndex < ViewConfigCount; ViewIndex++)
	{
		XrViewConfigurationView View;
		View.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;

		ViewFov[ViewIndex].type = XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC;
		ViewFov[ViewIndex].next = nullptr;
		View.next = bViewConfigurationFovSupported ? &ViewFov[ViewIndex] : nullptr;

		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			View.next = Module->OnEnumerateViewConfigurationViews(Instance, System, SelectedViewConfigurationType, ViewIndex, View.next);
		}

		// These are core views that don't have an associated plugin
		PipelineState.PluginViewInfos.AddDefaulted(1);
		PipelineState.ViewConfigs.Add(View);
	}
	XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, SelectedViewConfigurationType, ViewConfigCount, &ViewConfigCount, PipelineState.ViewConfigs.GetData()));

	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		TArray<XrViewConfigurationView> ViewConfigs;
		Module->GetViewConfigurations(System, ViewConfigs);

		const EStereoscopicPass PluginPassType = ViewConfigs.Num() > 1 ? EStereoscopicPass::eSSP_PRIMARY : EStereoscopicPass::eSSP_FULL;
		for (int32 i = 0; i < ViewConfigs.Num(); i++)
		{
			PipelineState.PluginViewInfos.Add({ Module, PluginPassType, true });
		}
		PipelineState.ViewConfigs.Append(ViewConfigs);
	}
	
	if (Session)
	{
		LocateViews(PipelineState, true);

		check(PipelineState.bXrFrameStateUpdated);

		FReadScopeLock DeviceLock(DeviceMutex);
		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			auto Predicate = [Module](const FPluginViewInfo& Info) -> bool
			{
				return Info.Plugin == Module;
			};

			if (PipelineState.PluginViewInfos.ContainsByPredicate(Predicate))
			{
				TArray<XrView> Views;
				Module->GetViewLocations(Session, PipelineState.FrameState.predictedDisplayTime, DeviceSpaces[HMDDeviceId].Space, Views);
				check(Views.Num() > 0);
				PipelineState.Views.Append(Views);
			}
		}
	}
	else if (bViewConfigurationFovSupported)
	{
		// We can't locate the views yet, but we can already retrieve their field-of-views
		PipelineState.Views.SetNum(PipelineState.ViewConfigs.Num());
		for (int ViewIndex = 0; ViewIndex < PipelineState.Views.Num(); ViewIndex++)
		{
			XrView& View = PipelineState.Views[ViewIndex];
			View.type = XR_TYPE_VIEW;
			View.next = nullptr;
			// FIXME: should be recommendedFov
			View.fov = ViewFov[ViewIndex].recommendedFov;
			View.pose = ToXrPose(FTransform::Identity);
		}
	}
	else
	{
		// Ensure the views have sane values before we locate them
		PipelineState.Views.SetNum(PipelineState.ViewConfigs.Num());
		for (XrView& View : PipelineState.Views)
		{
			View.type = XR_TYPE_VIEW;
			View.next = nullptr;
			View.fov = XrFovf{ -PI / 4.0f, PI / 4.0f, PI / 4.0f, -PI / 4.0f };
			View.pose = ToXrPose(FTransform::Identity);
		}
	}
}

bool FOpenXRHMD::IsViewManagedByPlugin(int32 ViewIndex) const
{
	check(IsInRenderingThread());
	if (!PipelinedFrameStateRendering.PluginViewInfos.IsValidIndex(ViewIndex))
	{
		// Was formerly associated with plugin, but plugin is no longer providing this view
		// Core views always have an entry in the PluginViewInfos array
		return true;
	}
	return PipelinedFrameStateRendering.PluginViewInfos[ViewIndex].bIsPluginManaged;
}

#if !PLATFORM_HOLOLENS
void FOpenXRHMD::BuildOcclusionMeshes()
{
	SCOPED_NAMED_EVENT(BuildOcclusionMeshes, FColor::Red);

	uint32_t ViewCount = 0;
	XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, SelectedViewConfigurationType, 0, &ViewCount, nullptr));
	HiddenAreaMeshes.SetNum(ViewCount);
	VisibleAreaMeshes.SetNum(ViewCount);

	bool bAnyViewSucceeded = false;

	for (uint32_t View = 0; View < ViewCount; ++View)
	{
		if (BuildOcclusionMesh(XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR, View, VisibleAreaMeshes[View]) &&
			BuildOcclusionMesh(XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, View, HiddenAreaMeshes[View]))
		{
			bAnyViewSucceeded = true;
		}
	}

	if (!bAnyViewSucceeded)
	{
		UE_LOG(LogHMD, Error, TEXT("Failed to create all visibility mask meshes for device/views. Abandoning visibility mask."));

		HiddenAreaMeshes.Empty();
		VisibleAreaMeshes.Empty();
	}

	bNeedReBuildOcclusionMesh = false;
}

bool FOpenXRHMD::BuildOcclusionMesh(XrVisibilityMaskTypeKHR Type, int View, FHMDViewMesh& Mesh)
{
	FReadScopeLock Lock(SessionHandleMutex);
	if (!Session)
	{
		return false;
	}

	PFN_xrGetVisibilityMaskKHR GetVisibilityMaskKHR;
	XR_ENSURE(xrGetInstanceProcAddr(Instance, "xrGetVisibilityMaskKHR", (PFN_xrVoidFunction*)&GetVisibilityMaskKHR));

	XrVisibilityMaskKHR VisibilityMask = { XR_TYPE_VISIBILITY_MASK_KHR };
	XR_ENSURE(GetVisibilityMaskKHR(Session, SelectedViewConfigurationType, View, Type, &VisibilityMask));

	if (VisibilityMask.indexCountOutput == 0)
	{
		// Runtime doesn't have a valid mask for this view
		return false;
	}
	if (!VisibilityMask.indexCountOutput || (VisibilityMask.indexCountOutput % 3) != 0 || VisibilityMask.vertexCountOutput == 0)
	{
		UE_LOG(LogHMD, Error, TEXT("Visibility Mask Mesh returned from runtime is invalid."));
		return false;
	}

	FRHIResourceCreateInfo CreateInfo(TEXT("FOpenXRHMD"));
	Mesh.VertexBufferRHI = RHICreateVertexBuffer(sizeof(FFilterVertex) * VisibilityMask.vertexCountOutput, BUF_Static, CreateInfo);
	void* VertexBufferPtr = RHILockBuffer(Mesh.VertexBufferRHI, 0, sizeof(FFilterVertex) * VisibilityMask.vertexCountOutput, RLM_WriteOnly);
	FFilterVertex* Vertices = reinterpret_cast<FFilterVertex*>(VertexBufferPtr);

	Mesh.IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32), sizeof(uint32) * VisibilityMask.indexCountOutput, BUF_Static, CreateInfo);
	void* IndexBufferPtr = RHILockBuffer(Mesh.IndexBufferRHI, 0, sizeof(uint32) * VisibilityMask.indexCountOutput, RLM_WriteOnly);

	uint32* OutIndices = reinterpret_cast<uint32*>(IndexBufferPtr);
	TUniquePtr<XrVector2f[]> const OutVertices = MakeUnique<XrVector2f[]>(VisibilityMask.vertexCountOutput);

	VisibilityMask.vertexCapacityInput = VisibilityMask.vertexCountOutput;
	VisibilityMask.indexCapacityInput = VisibilityMask.indexCountOutput;
	VisibilityMask.indices = OutIndices;
	VisibilityMask.vertices = OutVertices.Get();

	GetVisibilityMaskKHR(Session, SelectedViewConfigurationType, View, Type, &VisibilityMask);

	// We need to apply the eye's projection matrix to each vertex
	FMatrix Projection = GetStereoProjectionMatrix(View);

	ensure(VisibilityMask.vertexCapacityInput == VisibilityMask.vertexCountOutput);
	ensure(VisibilityMask.indexCapacityInput == VisibilityMask.indexCountOutput);

	for (uint32 VertexIndex = 0; VertexIndex < VisibilityMask.vertexCountOutput; ++VertexIndex)
	{
		FFilterVertex& Vertex = Vertices[VertexIndex];
		FVector Position(OutVertices[VertexIndex].x, OutVertices[VertexIndex].y, 1.0f);

		Vertex.Position = (FVector4f)Projection.TransformPosition(Position); // LWC_TODO: precision loss

		if (Type == XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR)
		{
			// For the visible-area mesh, this will be consumed by the post-process pipeline, so set up coordinates in the space they expect
			// (x and y range from 0-1, origin bottom-left, z at the far plane).
			Vertex.Position.X = Vertex.Position.X / 2.0f + .5f;
			Vertex.Position.Y = Vertex.Position.Y / -2.0f + .5f;
			Vertex.Position.Z = 0.0f;
			Vertex.Position.W = 1.0f;
		}

		Vertex.UV.X = Vertex.Position.X;
		Vertex.UV.Y = Vertex.Position.Y;
	}

	Mesh.NumIndices = VisibilityMask.indexCountOutput;
	Mesh.NumVertices = VisibilityMask.vertexCountOutput;
	Mesh.NumTriangles = Mesh.NumIndices / 3;

	RHIUnlockBuffer(Mesh.VertexBufferRHI);
	RHIUnlockBuffer(Mesh.IndexBufferRHI);

	return true;
}
#endif

bool FOpenXRHMD::OnStereoStartup()
{
	FWriteScopeLock Lock(SessionHandleMutex);

	check(Session == XR_NULL_HANDLE);
	bIsExitingSessionByxrRequestExitSession = false;  // clear in case we requested exit for a previous session, but it ended in some other way before that happened.

	XrSessionCreateInfo SessionInfo;
	SessionInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	SessionInfo.next = RenderBridge.IsValid() ? RenderBridge->GetGraphicsBinding() : nullptr;
	SessionInfo.createFlags = 0;
	SessionInfo.systemId = System;
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		SessionInfo.next = Module->OnCreateSession(Instance, System, SessionInfo.next);
	}

	if (!XR_ENSURE(xrCreateSession(Instance, &SessionInfo, &Session)))
	{
		return false;
	}

	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		Module->PostCreateSession(Session);
	}

	uint32_t ReferenceSpacesCount;
	XR_ENSURE(xrEnumerateReferenceSpaces(Session, 0, &ReferenceSpacesCount, nullptr));

	TArray<XrReferenceSpaceType> ReferenceSpaces;
	ReferenceSpaces.SetNum(ReferenceSpacesCount);
	// Initialize spaces array with valid enum values (avoid triggering validation error).
	for (auto& SpaceIter : ReferenceSpaces)
		SpaceIter = XR_REFERENCE_SPACE_TYPE_VIEW;
	XR_ENSURE(xrEnumerateReferenceSpaces(Session, (uint32_t)ReferenceSpaces.Num(), &ReferenceSpacesCount, ReferenceSpaces.GetData()));
	ensure(ReferenceSpacesCount == ReferenceSpaces.Num());

	XrSpace HmdSpace = XR_NULL_HANDLE;
	XrReferenceSpaceCreateInfo SpaceInfo;
	ensure(ReferenceSpaces.Contains(XR_REFERENCE_SPACE_TYPE_VIEW));
	SpaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	SpaceInfo.next = nullptr;
	SpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	SpaceInfo.poseInReferenceSpace = ToXrPose(FTransform::Identity);
	XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &HmdSpace));
	{
		FWriteScopeLock DeviceLock(DeviceMutex);
		DeviceSpaces[HMDDeviceId].Space = HmdSpace;
	}

	ensure(ReferenceSpaces.Contains(XR_REFERENCE_SPACE_TYPE_LOCAL));
	SpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &LocalSpace));

	bUseCustomReferenceSpace = false;
	XrReferenceSpaceType CustomReferenceSpaceType;
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		if (Module->UseCustomReferenceSpaceType(CustomReferenceSpaceType))
		{
			bUseCustomReferenceSpace = true;
			break;
		}
	}

	// If a custom reference space is desired, try to use that.
	// Otherwise use the currently selected reference space.
	if (bUseCustomReferenceSpace && ReferenceSpaces.Contains(CustomReferenceSpaceType))
	{
		TrackingSpaceType = CustomReferenceSpaceType;
		SpaceInfo.referenceSpaceType = TrackingSpaceType;
		XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &CustomSpace));
	}
	else if (ReferenceSpaces.Contains(XR_REFERENCE_SPACE_TYPE_STAGE))
	{
		TrackingSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		SpaceInfo.referenceSpaceType = TrackingSpaceType;
		XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &StageSpace));
	}
	else
	{
		ensure(ReferenceSpaces.Contains(XR_REFERENCE_SPACE_TYPE_LOCAL));
		TrackingSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	}

	// Create initial tracking space
	BaseOrientation = FQuat::Identity;
	BasePosition = FVector::ZeroVector;
	PipelinedFrameStateGame.TrackingSpace = MakeShared<FTrackingSpace>(TrackingSpaceType);
	PipelinedFrameStateGame.TrackingSpace->CreateSpace(Session);

	// Create action spaces for all devices
	{
		FWriteScopeLock DeviceLock(DeviceMutex);
		for (FDeviceSpace& DeviceSpace : DeviceSpaces)
		{
			DeviceSpace.CreateSpace(Session);
		}
	}

	if (RenderBridge.IsValid())
	{
		RenderBridge->SetOpenXRHMD(this);
	}

	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	bool bUseExtensionSpectatorScreenController = false;
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		bUseExtensionSpectatorScreenController = Module->GetSpectatorScreenController(this, SpectatorScreenController);
		if (bUseExtensionSpectatorScreenController)
		{
			break;
		}
	}

#if !PLATFORM_HOLOLENS
	if (!bUseExtensionSpectatorScreenController && !bIsStandaloneStereoOnlyDevice)
	{
		SpectatorScreenController = MakeUnique<FDefaultSpectatorScreenController>(this);
		UE_LOG(LogHMD, Verbose, TEXT("OpenXR using base spectator screen."));
	}
	else
#endif
	{
		if (SpectatorScreenController == nullptr)
		{
			UE_LOG(LogHMD, Verbose, TEXT("OpenXR disabling spectator screen."));
		}
		else
		{
			UE_LOG(LogHMD, Verbose, TEXT("OpenXR using extension spectator screen."));
		}
	}

	FCoreDelegates::VRHeadsetRecenter.AddRaw(this, &FOpenXRHMD::VRHeadsetRecenterDelegate);

	return true;
}

bool FOpenXRHMD::OnStereoTeardown()
{
	XrResult Result = XR_ERROR_SESSION_NOT_RUNNING;
	{
		FReadScopeLock Lock(SessionHandleMutex);
		if (Session != XR_NULL_HANDLE)
		{
			UE_LOG(LogHMD, Verbose, TEXT("FOpenXRHMD::OnStereoTeardown() calling xrRequestExitSession"));
			bIsExitingSessionByxrRequestExitSession = true;
			Result = xrRequestExitSession(Session);
		}
	}

	if (Result == XR_ERROR_SESSION_NOT_RUNNING)
	{
		// Session was never running - most likely PIE without putting the headset on.
		DestroySession();
	}
	else
	{
		XR_ENSURE(Result);
	}

	FCoreDelegates::VRHeadsetRecenter.RemoveAll(this);

	return true;
}

void FOpenXRHMD::DestroySession()
{
	// FlushRenderingCommands must be called outside of SessionLock since some rendering threads will also lock this mutex.
	FlushRenderingCommands();

	// Clear all the tracked devices
	ResetActionDevices();

	FWriteScopeLock SessionLock(SessionHandleMutex);

	if (Session != XR_NULL_HANDLE)
	{
		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			Module->OnDestroySession(Session);
		}

		InputModule->OnDestroySession();

		// We need to reset all swapchain references to ensure there are no attempts
		// to destroy swapchain handles after the session is already destroyed.
		ForEachLayer([&](uint32 /* unused */, FOpenXRLayer& Layer)
		{
			Layer.RightEye.Swapchain.Reset();
			Layer.LeftEye.Swapchain.Reset();
		});

		NativeQuadLayers.Reset();

		PipelinedLayerStateRendering.ColorSwapchain.Reset();
		PipelinedLayerStateRendering.DepthSwapchain.Reset();
		PipelinedLayerStateRendering.QuadSwapchains.Reset();

		// TODO: Once we handle OnFinishRendering_RHIThread + StopSession interactions
		// properly, we can release these shared pointers in that function, and use
		// `ensure` here to make sure these are released.
		PipelinedLayerStateRHI.ColorSwapchain.Reset();
		PipelinedLayerStateRHI.DepthSwapchain.Reset();
		PipelinedLayerStateRHI.QuadSwapchains.Reset();

		PipelinedFrameStateGame.TrackingSpace.Reset();
		PipelinedFrameStateRendering.TrackingSpace.Reset();
		PipelinedFrameStateRHI.TrackingSpace.Reset();
		bTrackingSpaceInvalid = true;

		// Reset the frame state.
		PipelinedFrameStateGame.bXrFrameStateUpdated = false;
		PipelinedFrameStateGame.FrameState = XrFrameState{ XR_TYPE_FRAME_STATE };
		PipelinedFrameStateRendering.bXrFrameStateUpdated = false;
		PipelinedFrameStateRendering.FrameState = XrFrameState{ XR_TYPE_FRAME_STATE };
		PipelinedFrameStateRHI.bXrFrameStateUpdated = false;
		PipelinedFrameStateRHI.FrameState = XrFrameState{ XR_TYPE_FRAME_STATE };

		// VRFocus must be reset so FWindowsApplication::PollGameDeviceState does not incorrectly short-circuit.
		FApp::SetUseVRFocus(false);
		FApp::SetHasVRFocus(false);

		// Destroy device and reference spaces, they will be recreated
		// when the session is created again.
		{
			FReadScopeLock DeviceLock(DeviceMutex);
			for (FDeviceSpace& Device : DeviceSpaces)
			{
				Device.DestroySpace();
			}
		}

		// Close the session now we're allowed to.
		XR_ENSURE(xrDestroySession(Session));
		Session = XR_NULL_HANDLE;
		CurrentSessionState = XR_SESSION_STATE_UNKNOWN;
		UE_LOG(LogHMD, Verbose, TEXT("Session state switched to XR_SESSION_STATE_UNKNOWN by DestroySession()"), OpenXRSessionStateToString(CurrentSessionState));
		bStereoEnabled = false;
		bIsReady = false;
		bIsRunning = false;
		bIsRendering = false;
		bIsSynchronized = false;
		bNeedReAllocatedDepth = true;
		bNeedReBuildOcclusionMesh = true;
	}
}

int32 FOpenXRHMD::AddActionDevice(XrAction Action, XrPath Path)
{
	FWriteScopeLock DeviceLock(DeviceMutex);

	// Ensure the HMD device is already emplaced
	ensure(DeviceSpaces.Num() > 0);

	int32 DeviceId = DeviceSpaces.Emplace(Action, Path);

	//FReadScopeLock Lock(SessionHandleMutex); // This is called from StartSession(), which already has this lock.
	if (Session)
	{
		DeviceSpaces[DeviceId].CreateSpace(Session);
	}

	return DeviceId;
}

void FOpenXRHMD::ResetActionDevices()
{
	FWriteScopeLock DeviceLock(DeviceMutex);

	// Index 0 is HMDDeviceId and is preserved. The remaining are action devices.
	if (DeviceSpaces.Num() > 0)
	{
		DeviceSpaces.RemoveAt(HMDDeviceId + 1, DeviceSpaces.Num() - 1);
	}
}

XrPath FOpenXRHMD::GetTrackedDevicePath(const int32 DeviceId)
{
	FReadScopeLock DeviceLock(DeviceMutex);
	if (DeviceSpaces.IsValidIndex(DeviceId))
	{
		return DeviceSpaces[DeviceId].Path;
	}
	return XR_NULL_PATH;
}

XrSpace FOpenXRHMD::GetTrackedDeviceSpace(const int32 DeviceId)
{
	FReadScopeLock DeviceLock(DeviceMutex);
	if (DeviceSpaces.IsValidIndex(DeviceId))
	{
		return DeviceSpaces[DeviceId].Space;
	}
	return XR_NULL_HANDLE;
}

XrTime FOpenXRHMD::GetDisplayTime() const
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	return PipelineState.bXrFrameStateUpdated ? PipelineState.FrameState.predictedDisplayTime : 0;
}

XrSpace FOpenXRHMD::GetTrackingSpace() const
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	if (PipelineState.TrackingSpace.IsValid())
	{
		return PipelineState.TrackingSpace->Handle;
	}
	else
	{
		return XR_NULL_HANDLE;
	}
}

bool FOpenXRHMD::IsInitialized() const
{
	return Instance != XR_NULL_HANDLE;
}

bool FOpenXRHMD::IsRunning() const
{
	return bIsRunning;
}

bool FOpenXRHMD::IsFocused() const
{
	return CurrentSessionState == XR_SESSION_STATE_FOCUSED;
}

bool FOpenXRHMD::StartSession()
{
	// If the session is not yet ready, we'll call into this function again when it is
	FWriteScopeLock Lock(SessionHandleMutex);
	if (!bIsReady || bIsRunning)
	{
		return false;
	}

	check(InputModule);
	InputModule->OnBeginSession();

	XrSessionBeginInfo Begin = { XR_TYPE_SESSION_BEGIN_INFO, nullptr, SelectedViewConfigurationType };
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		Begin.next = Module->OnBeginSession(Session, Begin.next);
	}

	bIsRunning = XR_ENSURE(xrBeginSession(Session, &Begin));
	return bIsRunning;
}

bool FOpenXRHMD::StopSession()
{
	FWriteScopeLock Lock(SessionHandleMutex);
	if (!bIsRunning)
	{
		return false;
	}

	bIsRunning = !XR_ENSURE(xrEndSession(Session));
	return !bIsRunning;
}

void FOpenXRHMD::OnBeginPlay(FWorldContext& InWorldContext)
{
}

void FOpenXRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
}

IStereoRenderTargetManager* FOpenXRHMD::GetRenderTargetManager()
{
	return this;
}

bool FOpenXRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(IsInRenderingThread());

	FReadScopeLock Lock(SessionHandleMutex);
	if (!Session)
	{
		return false;
	}

	// We're only creating a 1x target here, but we don't know whether it'll be the targeted texture
	// or the resolve texture. Because of this, we unify the input flags.
	ETextureCreateFlags UnifiedCreateFlags = Flags | TargetableTextureFlags;

	// This is not a static swapchain
	UnifiedCreateFlags |= TexCreate_Dynamic;

	// We need to ensure we can sample from the texture in CopyTexture
	UnifiedCreateFlags |= TexCreate_ShaderResource;

	// We assume this could be used as a resolve target
	UnifiedCreateFlags |= TexCreate_ResolveTargetable;

	// Some render APIs require us to present in RT layouts/configs,
	// so even if app won't use this texture as RT, we need the flag.
	UnifiedCreateFlags |= TexCreate_RenderTargetable;

	// On mobile without HDR all render targets need to be marked sRGB
	bool MobileHWsRGB = IsMobileColorsRGB() && IsMobilePlatform(GMaxRHIShaderPlatform);
	if (MobileHWsRGB)
	{
		UnifiedCreateFlags |= TexCreate_SRGB;
	}


	// Temporary workaround to swapchain formats - OpenXR doesn't support 10-bit sRGB swapchains, so prefer 8-bit sRGB instead.
	if (Format == PF_A2B10G10R10 && !RenderBridge->Support10BitSwapchain())
	{
		UE_LOG(LogHMD, Warning, TEXT("Requesting 10 bit swapchain, but not supported: fall back to 8bpc"));
		Format = PF_R8G8B8A8;
	}

	FClearValueBinding ClearColor = FClearValueBinding::Transparent;

	uint8 ActualFormat = Format;
	FXRSwapChainPtr& Swapchain = PipelinedLayerStateRendering.ColorSwapchain;
	const FRHITexture2D* const SwapchainTexture = Swapchain == nullptr ? nullptr : Swapchain->GetTexture2DArray() ? Swapchain->GetTexture2DArray() : Swapchain->GetTexture2D();
	if (Swapchain == nullptr || SwapchainTexture == nullptr || Format != LastRequestedColorSwapchainFormat || SwapchainTexture->GetSizeX() != SizeX || SwapchainTexture->GetSizeY() != SizeY)
	{
		ensureMsgf(NumSamples == 1, TEXT("OpenXR supports MSAA swapchains, but engine logic expects the swapchain target to be 1x."));

		Swapchain = RenderBridge->CreateSwapchain(Session, Format, ActualFormat, SizeX, SizeY, bIsMobileMultiViewEnabled ? 2 : 1, NumMips, NumSamples, UnifiedCreateFlags, ClearColor);
		if (!Swapchain)
		{
			return false;
		}
	}

	// Grab the presentation texture out of the swapchain.
	OutTargetableTexture = OutShaderResourceTexture = (FTexture2DRHIRef&)Swapchain->GetTextureRef();
	LastRequestedColorSwapchainFormat = Format;
	LastActualColorSwapchainFormat = ActualFormat;

	// TODO: Pass in known depth parameters (format + flags)? Do we know that at viewport setup time?
	AllocateDepthTextureInternal(Index, SizeX, SizeY, NumSamples);

	return true;
}

void FOpenXRHMD::AllocateDepthTextureInternal(uint32 Index, uint32 SizeX, uint32 SizeY, uint32 NumSamples)
{
	// TODO: Allocate depth texture by checking bNeedReAllocatedDepth

	check(IsInRenderingThread());

	FReadScopeLock Lock(SessionHandleMutex);
	if (!Session || !bDepthExtensionSupported)
	{
		return;
	}

	check(LastRequestedDepthSwapchainFormat == PF_DepthStencil);

	FXRSwapChainPtr& DepthSwapchain = PipelinedLayerStateRendering.DepthSwapchain;
	const FRHITexture2D* const DepthSwapchainTexture = DepthSwapchain == nullptr ? nullptr : DepthSwapchain->GetTexture2DArray() ? DepthSwapchain->GetTexture2DArray() : DepthSwapchain->GetTexture2D();
	if (DepthSwapchain == nullptr || DepthSwapchainTexture == nullptr || 
		DepthSwapchainTexture->GetSizeX() != SizeX || DepthSwapchainTexture->GetSizeY() != SizeY)
	{
		// We're only creating a 1x target here, but we don't know whether it'll be the targeted texture
		// or the resolve texture. Because of this, we unify the input flags.
		ETextureCreateFlags UnifiedCreateFlags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource | TexCreate_InputAttachmentRead;

		// This is not a static swapchain
		UnifiedCreateFlags |= TexCreate_Dynamic;

		// We assume this could be used as a resolve target
		UnifiedCreateFlags |= TexCreate_DepthStencilResolveTarget;

		ensureMsgf(NumSamples == 1, TEXT("OpenXR supports MSAA swapchains, but engine logic expects the swapchain target to be 1x."));
		constexpr uint32 NumSamplesExpected = 1;
		constexpr uint32 NumMipsExpected = 1;

		uint8 UnusedActualFormat = 0;
		DepthSwapchain = RenderBridge->CreateSwapchain(Session, PF_DepthStencil, UnusedActualFormat, SizeX, SizeY, bIsMobileMultiViewEnabled ? 2 : 1, NumMipsExpected, NumSamplesExpected, UnifiedCreateFlags, FClearValueBinding::DepthFar);
		if (!DepthSwapchain)
		{
			return;
		}

		// image will be acquired next time we begin the rendering
	}

	bNeedReAllocatedDepth = false;
}

// TODO: in the future, we can rename the interface to GetDepthTexture because allocate could happen in AllocateRenderTargetTexture
bool FOpenXRHMD::AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(IsInRenderingThread());

	// FIXME: UE constantly calls this function even when there is no reason to reallocate the depth texture (see NeedReAllocateDepthTexture)
	FReadScopeLock Lock(SessionHandleMutex);
	if (!Session || !bDepthExtensionSupported)
	{
		return false;
	}

	const FXRSwapChainPtr& DepthSwapchain = PipelinedLayerStateRendering.DepthSwapchain;
	if (DepthSwapchain == nullptr)
	{
		return false;
	}

	const ETextureCreateFlags UnifiedCreateFlags = Flags | TargetableTextureFlags;
	ensure(EnumHasAllFlags(UnifiedCreateFlags, TexCreate_DepthStencilTargetable)); // We can't use the depth swapchain w/o this flag

	const FRHITexture2D* const DepthSwapchainTexture = DepthSwapchain->GetTexture2DArray() ? DepthSwapchain->GetTexture2DArray() : DepthSwapchain->GetTexture2D();
	const FRHITextureDesc& DepthSwapchainDesc = DepthSwapchainTexture->GetDesc();

	// Sample count, mip count and size should be known at AllocateRenderTargetTexture time
	// Format _could_ change, but we should know it (and can check for it in AllocateDepthTextureInternal)
	// Flags might also change. We expect TexCreate_DepthStencilTargetable | TexCreate_ShaderResource | TexCreate_InputAttachmentRead from SceneTextures
	check(EnumHasAllFlags(DepthSwapchainDesc.Flags, UnifiedCreateFlags));
	check(DepthSwapchainDesc.Format == Format);
	check(DepthSwapchainDesc.NumMips == FMath::Max(NumMips, 1u));
	check(DepthSwapchainDesc.NumSamples == NumSamples);

	LastRequestedDepthSwapchainFormat = Format;

	OutTargetableTexture = OutShaderResourceTexture = (FTexture2DRHIRef&)PipelinedLayerStateRendering.DepthSwapchain->GetTextureRef();

	return true;
}

void CreateNativeLayerSwapchain(FOpenXRLayer& Layer, TRefCountPtr<FOpenXRRenderBridge>& RenderBridge, XrSession Session)
{
	auto CreateSwapchain = [&](FRHITexture2D* Texture, ETextureCreateFlags Flags)
	{
		uint8 UnusedActualFormat = 0;
		return RenderBridge->CreateSwapchain(Session,
			IStereoRenderTargetManager::GetStereoLayerPixelFormat(),
			UnusedActualFormat,
			Texture->GetSizeX(),
			Texture->GetSizeY(),
			1,
			Texture->GetNumMips(),
			Texture->GetNumSamples(),
			Texture->GetFlags() | Flags | TexCreate_RenderTargetable,
			Texture->GetClearBinding());
	};

	const ETextureCreateFlags Flags = Layer.Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE ?
		TexCreate_Dynamic | TexCreate_SRGB : TexCreate_SRGB;

	if (Layer.NeedReallocateRightTexture())
	{
		FRHITexture2D* Texture = Layer.Desc.Texture->GetTexture2D();
		Layer.RightEye.SetSwapchain(CreateSwapchain(Texture, Flags), Texture->GetSizeXY());
	}

	if (Layer.NeedReallocateLeftTexture())
	{
		FRHITexture2D* Texture = Layer.Desc.LeftTexture->GetTexture2D();
		Layer.LeftEye.SetSwapchain(CreateSwapchain(Texture, Flags), Texture->GetSizeXY());
	}
}

void FOpenXRHMD::SetupFrameQuadLayers_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	ensure(IsInRenderingThread());

	if (GetStereoLayersDirty())
	{
		// Go over the dirtied layers to bin them into either native or emulated
		EmulatedSceneLayers.Reset();
		NativeQuadLayers.Reset();

		ForEachLayer([&](uint32 /* unused */, FOpenXRLayer& Layer)
		{
			// We use native layer for facelocked and when world-locked has proper support
			if ((Layer.Desc.PositionType == ELayerType::FaceLocked) || bNativeWorldQuadLayerSupport)
			{
				if (Layer.Desc.IsVisible() && Layer.Desc.HasShape<FQuadLayer>())
				{
					CreateNativeLayerSwapchain(Layer, RenderBridge, Session);
					NativeQuadLayers.Add(Layer);
				}
				else
				{
					// We retain references in FPipelinedLayerState to avoid premature destruction
					Layer.RightEye.Swapchain.Reset();
					Layer.LeftEye.Swapchain.Reset();
				}
			}
			else if (Layer.Desc.IsVisible())
			{
				EmulatedSceneLayers.Add(Layer.Desc);
			}
		});

		auto LayerCompare = [](const auto& A, const auto& B)
		{
			FLayerDesc DescA, DescB;
			if (GetLayerDescMember(A, DescA) && GetLayerDescMember(B, DescB))
			{
				if (DescA.Priority < DescB.Priority)
				{
					return true;
				}
				if (DescA.Priority > DescB.Priority)
				{
					return false;
				}
				return DescA.Id < DescB.Id;
			}
			return false;
		};

		EmulatedSceneLayers.Sort(LayerCompare);
		NativeQuadLayers.Sort(LayerCompare);
	}

	const FTransform InvTrackingToWorld = GetTrackingToWorldTransform().Inverse();
	const float WorldToMeters = GetWorldToMetersScale();

	// Set up our OpenXR info per native quad layer. Emulated layers have everything in FLayerDesc
	PipelinedLayerStateRendering.QuadLayers.Reset(NativeQuadLayers.Num());
	PipelinedLayerStateRendering.QuadSwapchains.Reset(NativeQuadLayers.Num());
	{
		FReadScopeLock DeviceLock(DeviceMutex);

		for (const FOpenXRLayer& Layer : NativeQuadLayers)
		{
			const bool bNoAlpha = Layer.Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL;
			const bool bIsStereo = Layer.Desc.LeftTexture.IsValid();
			const FTransform PositionTransform = Layer.Desc.PositionType == ELayerType::WorldLocked ?
				InvTrackingToWorld : FTransform::Identity;

			XrCompositionLayerQuad Quad = { XR_TYPE_COMPOSITION_LAYER_QUAD, nullptr };
			Quad.layerFlags = bNoAlpha ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
				XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
			Quad.space = Layer.Desc.PositionType == ELayerType::FaceLocked ?
				DeviceSpaces[HMDDeviceId].Space : PipelinedFrameStateRendering.TrackingSpace->Handle;
			Quad.subImage.imageArrayIndex = 0;
			Quad.pose = ToXrPose(Layer.Desc.Transform * PositionTransform, WorldToMeters);

			// We need to copy each layer into an OpenXR swapchain so they can be displayed by the compositor
			if (Layer.RightEye.Swapchain.IsValid() && Layer.Desc.Texture.IsValid())
			{
				if (Layer.RightEye.bUpdateTexture && bIsRunning)
				{
					FRHITexture2D* SrcTexture = Layer.Desc.Texture->GetTexture2D();
					const FIntRect DstRect(FIntPoint(0, 0), Layer.RightEye.SwapchainSize.IntPoint());
					CopyTexture_RenderThread(RHICmdList, SrcTexture, FIntRect(), Layer.RightEye.Swapchain, DstRect, false, bNoAlpha);
				}

				Quad.eyeVisibility = bIsStereo ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH;

				Quad.subImage.imageRect = ToXrRect(Layer.GetRightViewportSize());
				Quad.subImage.swapchain = static_cast<FOpenXRSwapchain*>(Layer.RightEye.Swapchain.Get())->GetHandle();
				Quad.size = ToXrExtent2D(Layer.GetRightQuadSize(), WorldToMeters);
				PipelinedLayerStateRendering.QuadLayers.Add(Quad);
				PipelinedLayerStateRendering.QuadSwapchains.Add(Layer.RightEye.Swapchain);
			}

			if (Layer.LeftEye.Swapchain.IsValid() && Layer.Desc.LeftTexture.IsValid())
			{
				if (Layer.LeftEye.bUpdateTexture && bIsRunning)
				{
					FRHITexture2D* SrcTexture = Layer.Desc.LeftTexture->GetTexture2D();
					const FIntRect DstRect(FIntPoint(0, 0), Layer.LeftEye.SwapchainSize.IntPoint());
					CopyTexture_RenderThread(RHICmdList, SrcTexture, FIntRect(), Layer.LeftEye.Swapchain, DstRect, false, bNoAlpha);
				}

				Quad.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
				Quad.subImage.imageRect = ToXrRect(Layer.GetLeftViewportSize());
				Quad.subImage.swapchain = static_cast<FOpenXRSwapchain*>(Layer.LeftEye.Swapchain.Get())->GetHandle();
				Quad.size = ToXrExtent2D(Layer.GetLeftQuadSize(), WorldToMeters);
				PipelinedLayerStateRendering.QuadLayers.Add(Quad);
				PipelinedLayerStateRendering.QuadSwapchains.Add(Layer.LeftEye.Swapchain);
			}
		}
	}
}

void FOpenXRHMD::DrawEmulatedQuadLayers_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView)
{
	check(IsInRenderingThread());

	if (bNativeWorldQuadLayerSupport || !IStereoRendering::IsStereoEyeView(InView))
	{
		return;
	}

	// Partially borrowed from FDefaultStereoLayers
	AddPass(GraphBuilder, RDG_EVENT_NAME("OpenXREmulatedLayerRender"), [this, &InView](FRHICommandListImmediate& RHICmdList)
	{
		FViewMatrices ModifiedViewMatrices = InView.ViewMatrices;
		ModifiedViewMatrices.HackRemoveTemporalAAProjectionJitter();
		const FMatrix& ProjectionMatrix = ModifiedViewMatrices.GetProjectionMatrix();
		const FMatrix& ViewProjectionMatrix = ModifiedViewMatrices.GetViewProjectionMatrix();

		// Calculate a view matrix that only adjusts for eye position, ignoring head position, orientation and world position.
		FVector EyeShift;
		FQuat EyeOrientation;
		GetRelativeEyePose(IXRTrackingSystem::HMDDeviceId, InView.StereoViewIndex, EyeOrientation, EyeShift);

		FMatrix EyeMatrix = FTranslationMatrix(-EyeShift) * FInverseRotationMatrix(EyeOrientation.Rotator()) * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		FQuat HmdOrientation = FQuat::Identity;
		FVector HmdLocation = FVector::ZeroVector;
		GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HmdOrientation, HmdLocation);

		FMatrix TrackerMatrix = FTranslationMatrix(-HmdLocation) * FInverseRotationMatrix(HmdOrientation.Rotator()) * EyeMatrix;

		FDefaultStereoLayers::FLayerRenderParams RenderParams{
			InView.UnscaledViewRect, // Viewport
			{
				ViewProjectionMatrix,				// WorldLocked,
				TrackerMatrix * ProjectionMatrix,	// TrackerLocked,
				EyeMatrix * ProjectionMatrix		// FaceLocked
			}
		};

		TArray<FRHITransitionInfo, TInlineAllocator<16>> Infos;
		for (const FLayerDesc& Layer : EmulatedSceneLayers)
		{
			Infos.Add(FRHITransitionInfo(Layer.Texture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
		}
		if (Infos.Num())
		{
			RHICmdList.Transition(Infos);
		}

		FTexture2DRHIRef RenderTarget = InView.Family->RenderTarget->GetRenderTargetTexture();

		FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("EmulatedStereoLayerRender"));
		RHICmdList.SetViewport((float)RenderParams.Viewport.Min.X, (float)RenderParams.Viewport.Min.Y, 0.0f, (float)RenderParams.Viewport.Max.X, (float)RenderParams.Viewport.Max.Y, 1.0f);

		if (bSplashIsShown || !PipelinedLayerStateRendering.bBackgroundLayerVisible)
		{
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}

		FDefaultStereoLayers::StereoLayerRender(RHICmdList, EmulatedSceneLayers, RenderParams);

		RHICmdList.EndRenderPass();
	});
}

void FOpenXRHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	ensure(IsInRenderingThread());
	if (!RenderBridge)
	{
		// Frame submission is not necessary in a headless session.
		return;
	}

	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		Module->OnBeginRendering_RenderThread(Session);
	}

	const float WorldToMeters = GetWorldToMetersScale();

	for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ViewIndex++)
	{
		if (ViewFamily.Views[ViewIndex]->StereoPass == EStereoscopicPass::eSSP_FULL)
		{
			continue;
		}

		const XrView& View = PipelinedFrameStateRendering.Views[ViewIndex];
		FTransform EyePose = ToFTransform(View.pose, WorldToMeters);

		// Apply the base HMD pose to each eye pose, we will late update this pose for late update in another callback
		FTransform BasePose(ViewFamily.Views[ViewIndex]->BaseHmdOrientation, ViewFamily.Views[ViewIndex]->BaseHmdLocation);
		XrCompositionLayerProjectionView& Projection = PipelinedLayerStateRendering.ProjectionLayers[ViewIndex];
		FTransform BasePoseTransform = EyePose * BasePose;
		BasePoseTransform.NormalizeRotation();
		Projection.pose = ToXrPose(BasePoseTransform, WorldToMeters);
		Projection.fov = View.fov;
	}

	SetupFrameQuadLayers_RenderThread(RHICmdList);

#if !PLATFORM_HOLOLENS
	if (bHiddenAreaMaskSupported && bNeedReBuildOcclusionMesh)
	{
		BuildOcclusionMeshes();
	}
#endif

	// Guard prediction-dependent calls from being invoked (LocateViews, BeginFrame, etc)
	if (bIsRunning && PipelinedFrameStateRendering.bXrFrameStateUpdated)
	{
		// Locate the views we will actually be rendering for.
		// This is required to support late-updating the field-of-view.
		LocateViews(PipelinedFrameStateRendering, false);

		SCOPED_NAMED_EVENT(EnqueueFrame, FColor::Red);

		// Reset the update flag on native quad layers
		for (FOpenXRLayer& NativeQuadLayer : NativeQuadLayers)
		{
			const bool bUpdateTexture = NativeQuadLayer.Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
			NativeQuadLayer.RightEye.bUpdateTexture = bUpdateTexture;
			NativeQuadLayer.LeftEye.bUpdateTexture = bUpdateTexture;
		}

		FXRSwapChainPtr ColorSwapchain = PipelinedLayerStateRendering.ColorSwapchain;
		FXRSwapChainPtr DepthSwapchain = PipelinedLayerStateRendering.DepthSwapchain;
		RHICmdList.EnqueueLambda([this, FrameState = PipelinedFrameStateRendering, ColorSwapchain, DepthSwapchain](FRHICommandListImmediate& InRHICmdList)
		{
			OnBeginRendering_RHIThread(FrameState, ColorSwapchain, DepthSwapchain);
		});

		// We need to sync with the RHI thread to ensure we've acquired the next swapchain image.
		// TODO: The acquire needs to be moved to the Render thread as soon as it's allowed by the spec.
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}

	// Snapshot new poses for late update.
	UpdateDeviceLocations(false);
}

void FOpenXRHMD::LocateViews(FPipelinedFrameState& PipelineState, bool ResizeViewsArray)
{
	check(PipelineState.bXrFrameStateUpdated);
	FReadScopeLock DeviceLock(DeviceMutex);

	uint32_t ViewCount = 0;
	XrViewLocateInfo ViewInfo;
	ViewInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	ViewInfo.next = nullptr;
	ViewInfo.viewConfigurationType = SelectedViewConfigurationType;
	ViewInfo.space = DeviceSpaces[HMDDeviceId].Space;
	ViewInfo.displayTime = PipelineState.FrameState.predictedDisplayTime;
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		ViewInfo.next = Module->OnLocateViews(Session, ViewInfo.displayTime, ViewInfo.next);
	}

	XR_ENSURE(xrLocateViews(Session, &ViewInfo, &PipelineState.ViewState, 0, &ViewCount, nullptr));
	if (ResizeViewsArray)
	{
		PipelineState.Views.SetNum(ViewCount, false);
	}
	else
	{
		// PipelineState.Views.Num() can be greater than ViewCount if there is an IOpenXRExtensionPlugin
		// which appends more views with the GetViewLocations callback.
		ensure(PipelineState.Views.Num() >= (int32)ViewCount);
	}
	
	XR_ENSURE(xrLocateViews(Session, &ViewInfo, &PipelineState.ViewState, PipelineState.Views.Num(), &ViewCount, PipelineState.Views.GetData()));
}

void FOpenXRHMD::OnLateUpdateApplied_RenderThread(FRHICommandListImmediate& RHICmdList, const FTransform& NewRelativeTransform)
{
	FHeadMountedDisplayBase::OnLateUpdateApplied_RenderThread(RHICmdList, NewRelativeTransform);

	ensure(IsInRenderingThread());

	for (int32 ViewIndex = 0; ViewIndex < PipelinedLayerStateRendering.ProjectionLayers.Num(); ViewIndex++)
	{
		const XrView& View = PipelinedFrameStateRendering.Views[ViewIndex];
		XrCompositionLayerProjectionView& Projection = PipelinedLayerStateRendering.ProjectionLayers[ViewIndex];

		// Apply the new HMD orientation to each eye pose for the final pose
		FTransform EyePose = ToFTransform(View.pose, GetWorldToMetersScale());
		FTransform NewRelativePoseTransform = EyePose * NewRelativeTransform;
		NewRelativePoseTransform.NormalizeRotation();
		Projection.pose = ToXrPose(NewRelativePoseTransform, GetWorldToMetersScale());

		// Update the field-of-view to match the final projection matrix
		Projection.fov = View.fov;
	}

	RHICmdList.EnqueueLambda([this, ProjectionLayers = PipelinedLayerStateRendering.ProjectionLayers](FRHICommandListImmediate& InRHICmdList)
	{
		PipelinedLayerStateRHI.ProjectionLayers = ProjectionLayers;
	});
}

void FOpenXRHMD::OnBeginRendering_GameThread()
{
	// We need to make sure we keep the Wait/Begin/End triplet in sync, so here we signal that we
	// can wait for the next frame in the next tick. Without this signal it's possible that two ticks
	// happen before the next frame is actually rendered.
	bShouldWait = true;

	ENQUEUE_RENDER_COMMAND(TransferFrameStateToRenderingThread)(
		[this, GameFrameState = PipelinedFrameStateGame, bBackgroundLayerVisible = IsBackgroundLayerVisible()](FRHICommandListImmediate& RHICmdList) mutable
		{
			UE_CLOG(PipelinedFrameStateRendering.FrameState.predictedDisplayTime >= GameFrameState.FrameState.predictedDisplayTime,
				LogHMD, VeryVerbose, TEXT("Predicted display time went backwards from %lld to %lld"), PipelinedFrameStateRendering.FrameState.predictedDisplayTime, GameFrameState.FrameState.predictedDisplayTime);

			PipelinedFrameStateRendering = GameFrameState;

			// If we are emulating layers, we still need to submit background layer since we composite into it
			PipelinedLayerStateRendering.bBackgroundLayerVisible = bBackgroundLayerVisible;
			PipelinedLayerStateRendering.bSubmitBackgroundLayer = bBackgroundLayerVisible || !bNativeWorldQuadLayerSupport;
		});
}

void FOpenXRHMD::OnBeginSimulation_GameThread()
{
	FReadScopeLock Lock(SessionHandleMutex);

	if (!bShouldWait || !RenderBridge)
	{
		return;
	}

	FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	PipelineState.bXrFrameStateUpdated = false;
	PipelineState.FrameState = { XR_TYPE_FRAME_STATE };

	if (!bIsReady || !bIsRunning)
	{
		return;
	}

	ensure(IsInGameThread());

	SCOPED_NAMED_EVENT(WaitFrame, FColor::Red);

	XrFrameWaitInfo WaitInfo;
	WaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	WaitInfo.next = nullptr;

	XrFrameState FrameState{XR_TYPE_FRAME_STATE};
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		FrameState.next = Module->OnWaitFrame(Session, FrameState.next);
	}
	XR_ENSURE(xrWaitFrame(Session, &WaitInfo, &FrameState));

	// The pipeline state on the game thread can only be safely modified after xrWaitFrame which will be unblocked by
	// the runtime when xrBeginFrame is called. The rendering thread will clone the game pipeline state before calling
	// xrBeginFrame so the game pipeline state can safely be modified after xrWaitFrame returns.

	PipelineState.bXrFrameStateUpdated = true;
	PipelineState.FrameState = FrameState;
	PipelineState.WorldToMetersScale = WorldToMetersScale;

	if (bTrackingSpaceInvalid || !ensure(PipelineState.TrackingSpace.IsValid()))
	{
		// Create the tracking space we'll use until the next recenter.
		FTransform BaseTransform(BaseOrientation, BasePosition);
		PipelineState.TrackingSpace = MakeShared<FTrackingSpace>(TrackingSpaceType, ToXrPose(BaseTransform, WorldToMetersScale));
		PipelineState.TrackingSpace->CreateSpace(Session);
		bTrackingSpaceInvalid = false;
	}

	bShouldWait = false;

	EnumerateViews(PipelineState);
}

bool FOpenXRHMD::ReadNextEvent(XrEventDataBuffer* buffer)
{
	// It is sufficient to clear just the XrEventDataBuffer header to XR_TYPE_EVENT_DATA_BUFFER
	XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(buffer);
	*baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
	const XrResult xr = xrPollEvent(Instance, buffer);
	XR_ENSURE(xr);
	if (xr == XR_SUCCESS)
	{
		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			Module->OnEvent(Session, baseHeader);
		}
		return true;
	}
	return false;
}

bool FOpenXRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	const AWorldSettings* const WorldSettings = WorldContext.World() ? WorldContext.World()->GetWorldSettings() : nullptr;
	if (WorldSettings)
	{
		WorldToMetersScale = WorldSettings->WorldToMeters;
	}

	// Only refresh this based on the game world.  When remoting there is also an editor world, which we do not want to have affect the transform.
	if (WorldContext.World()->IsGameWorld())
	{
		RefreshTrackingToWorldTransform(WorldContext);
	}

	// Process all pending messages.
	XrEventDataBuffer event;
	while (ReadNextEvent(&event))
	{
		switch (event.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged& SessionState =
				reinterpret_cast<XrEventDataSessionStateChanged&>(event);

			CurrentSessionState = SessionState.state;

			UE_LOG(LogHMD, Verbose, TEXT("Session state switching to %s"), OpenXRSessionStateToString(CurrentSessionState));

			if (SessionState.state == XR_SESSION_STATE_READY)
			{
				if (!GIsEditor)
				{
					GEngine->SetMaxFPS(0);
				}
				bIsReady = true;
				StartSession();
			}
			else if (SessionState.state == XR_SESSION_STATE_SYNCHRONIZED)
			{
				bIsSynchronized = true;
			}
			else if (SessionState.state == XR_SESSION_STATE_IDLE)
			{
				bIsSynchronized = false;
			}
			else if (SessionState.state == XR_SESSION_STATE_STOPPING)
			{
				if (!GIsEditor)
				{
					GEngine->SetMaxFPS(OPENXR_PAUSED_IDLE_FPS);
				}
				bIsReady = false;
				StopSession();
			}
			else if (SessionState.state == XR_SESSION_STATE_EXITING || SessionState.state == XR_SESSION_STATE_LOSS_PENDING)
			{
				// We need to make sure we unlock the frame rate again when exiting stereo while idle
				if (!GIsEditor)
				{
					GEngine->SetMaxFPS(0);
				}
				
				FApp::SetHasVRFocus(false);

				DestroySession();

				// Do we want to RequestExitApp the app after destoying the session?
				// Yes if the app (ie ue4) did NOT requested the exit.
				bool bExitApp = !bIsExitingSessionByxrRequestExitSession;
				bIsExitingSessionByxrRequestExitSession = false;

				// But only if this CVar is set to true.
				bExitApp = bExitApp && (CVarOpenXRExitAppOnRuntimeDrivenSessionExit.GetValueOnAnyThread() != 0);
	
				if (bExitApp)
				{
					RequestExitApp();
				}
				break;
			}

			FApp::SetHasVRFocus(SessionState.state == XR_SESSION_STATE_FOCUSED);
			
			break;
		}
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		{
			DestroySession();
			
			// Instance loss is intended to support things like updating the active openxr runtime.  Currently we just require an app restart.
			RequestExitApp();

			break;
		}
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		{
			const XrEventDataReferenceSpaceChangePending& SpaceChange =
				reinterpret_cast<XrEventDataReferenceSpaceChangePending&>(event);
			if (SpaceChange.referenceSpaceType == TrackingSpaceType)
			{
				OnTrackingOriginChanged();

				// Reset base orientation and position
				// TODO: If poseValid is true we can use poseInPreviousSpace to make the old base transform valid in the new space
				BaseOrientation = FQuat::Identity;
				BasePosition = FVector::ZeroVector;
				bTrackingSpaceInvalid = true;
			}

			if (SpaceChange.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE)
			{
				OnPlayAreaChanged();
			}

			break;
		}
		case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
		{
			bHiddenAreaMaskSupported = ensure(IsExtensionEnabled(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME));  // Ensure fail indicates a non-conformant openxr implementation.
			bNeedReBuildOcclusionMesh = true;
		}
		}
	}

	GetARCompositionComponent()->StartARGameFrame(WorldContext);

	// TODO: We could do this earlier in the pipeline and allow simulation to run one frame ahead of the render thread.
	// That would allow us to take more advantage of Late Update and give projects more headroom for simulation.
	// However currently blocking in earlier callbacks can result in a pipeline stall, so we do it here instead.
	OnBeginSimulation_GameThread();

	// Snapshot new poses for game simulation.
	UpdateDeviceLocations(true);

	return true;
}

void FOpenXRHMD::RequestExitApp()
{
	UE_LOG(LogHMD, Log, TEXT("FOpenXRHMD is requesting app exit.  CurrentSessionState: %s"), OpenXRSessionStateToString(CurrentSessionState));

#if WITH_EDITOR
	if (GIsEditor)
	{
		FSceneViewport* SceneVP = FindSceneViewport();
		if (SceneVP && SceneVP->IsStereoRenderingAllowed())
		{
			TSharedPtr<SWindow> Window = SceneVP->FindWindow();
			Window->RequestDestroyWindow();
		}
	}
	else
#endif//WITH_EDITOR
	{
		// ApplicationWillTerminateDelegate will fire from inside of the RequestExit
		FPlatformMisc::RequestExit(false);
	}
}

void FOpenXRHMD::OnBeginRendering_RHIThread(const FPipelinedFrameState& InFrameState, FXRSwapChainPtr ColorSwapchain, FXRSwapChainPtr DepthSwapchain)
{
	ensure(IsInRenderingThread() || IsInRHIThread());

	// TODO: Add a hook to resolve discarded frames before we start a new frame.
	UE_CLOG(bIsRendering, LogHMD, Verbose, TEXT("Discarded previous frame and started rendering a new frame."));

	SCOPED_NAMED_EVENT(BeginFrame, FColor::Red);

	FReadScopeLock Lock(SessionHandleMutex);
	if (!bIsRunning || !RenderBridge)
	{
		return;
	}

	// The layer state will be copied after SetFinalViewRect
	PipelinedFrameStateRHI = InFrameState;

	XrFrameBeginInfo BeginInfo;
	BeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
	BeginInfo.next = nullptr;
	XrTime DisplayTime = InFrameState.FrameState.predictedDisplayTime;
	for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
	{
		BeginInfo.next = Module->OnBeginFrame(Session, DisplayTime, BeginInfo.next);
	}

	XrResult Result = xrBeginFrame(Session, &BeginInfo);
	if (XR_SUCCEEDED(Result))
	{
		// Only the swapchains are valid to pull out of PipelinedLayerStateRendering
		// Full population is deferred until SetFinalViewRect.
		// TODO Possibly move these Waits to SetFinalViewRect??
		PipelinedLayerStateRHI.ColorSwapchain = ColorSwapchain;
		PipelinedLayerStateRHI.DepthSwapchain = DepthSwapchain;

		// We need a new swapchain image unless we've already acquired one for rendering
		if (!bIsRendering && ColorSwapchain)
		{
			ColorSwapchain->IncrementSwapChainIndex_RHIThread();
			ColorSwapchain->WaitCurrentImage_RHIThread(OPENXR_SWAPCHAIN_WAIT_TIMEOUT);
			if (bDepthExtensionSupported && DepthSwapchain)
			{
				DepthSwapchain->IncrementSwapChainIndex_RHIThread();
				DepthSwapchain->WaitCurrentImage_RHIThread(OPENXR_SWAPCHAIN_WAIT_TIMEOUT);
			}
		}

		bIsRendering = true;

		UE_LOG(LogHMD, VeryVerbose, TEXT("Rendering frame predicted to be displayed at %lld"), InFrameState.FrameState.predictedDisplayTime);
	}
	else
	{
		static bool bLoggedBeginFrameFailure = false;
		if (!bLoggedBeginFrameFailure)
		{
			UE_LOG(LogHMD, Error, TEXT("Unexpected error on xrBeginFrame. Error code was %s."), OpenXRResultToString(Result));
			bLoggedBeginFrameFailure = true;
		}
	}
}

void FOpenXRHMD::OnFinishRendering_RHIThread()
{
	ensure(IsInRenderingThread() || IsInRHIThread());

	SCOPED_NAMED_EVENT(EndFrame, FColor::Red);

	if (!bIsRendering || !RenderBridge)
	{
		return;
	}

	// We need to ensure we release the swap chain images even if the session is not running.
	if (PipelinedLayerStateRHI.ColorSwapchain)
	{
		PipelinedLayerStateRHI.ColorSwapchain->ReleaseCurrentImage_RHIThread();

		if (bDepthExtensionSupported && PipelinedLayerStateRHI.DepthSwapchain)
		{
			PipelinedLayerStateRHI.DepthSwapchain->ReleaseCurrentImage_RHIThread();
		}
	}

	FReadScopeLock Lock(SessionHandleMutex);
	if (bIsRunning)
	{
		TArray<const XrCompositionLayerBaseHeader*> Headers;
		XrCompositionLayerProjection Layer = {};
		if (PipelinedLayerStateRHI.bSubmitBackgroundLayer)
		{
			Layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
			Layer.next = nullptr;
			Layer.layerFlags = bProjectionLayerAlphaEnabled ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
			Layer.space = PipelinedFrameStateRHI.TrackingSpace->Handle;
			Layer.viewCount = PipelinedLayerStateRHI.ProjectionLayers.Num();
			Layer.views = PipelinedLayerStateRHI.ProjectionLayers.GetData();
			Headers.Add(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&Layer));

			for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
			{
				Layer.next = Module->OnEndProjectionLayer(Session, 0, Layer.next, Layer.layerFlags);
			}

#if PLATFORM_ANDROID
			// @todo: temporary workaround for Quest compositor issue, see UE-145546
			Layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
#endif
		}

		for (const XrCompositionLayerQuad& Quad : PipelinedLayerStateRHI.QuadLayers)
		{
			Headers.Add(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&Quad));
		}

		int32 BlendModeOverride = CVarOpenXREnvironmentBlendMode.GetValueOnRenderThread();

		XrFrameEndInfo EndInfo;
		EndInfo.type = XR_TYPE_FRAME_END_INFO;
		EndInfo.next = nullptr;
		EndInfo.displayTime = PipelinedFrameStateRHI.FrameState.predictedDisplayTime;
		EndInfo.environmentBlendMode = BlendModeOverride ? (XrEnvironmentBlendMode)BlendModeOverride : SelectedEnvironmentBlendMode;

		EndInfo.layerCount = PipelinedFrameStateRHI.FrameState.shouldRender ? Headers.Num() : 0;
		EndInfo.layers = PipelinedFrameStateRHI.FrameState.shouldRender ? Headers.GetData() : nullptr;

		// Make callback to plugin including any extra view subimages they've requested
		for (IOpenXRExtensionPlugin* Module : ExtensionPlugins)
		{
			TArray<XrSwapchainSubImage> ColorImages;
			TArray<XrSwapchainSubImage> DepthImages;
			for (int32 i = 0; i < PipelinedFrameStateRHI.PluginViewInfos.Num(); i++)
			{
				if (PipelinedFrameStateRHI.PluginViewInfos[i].Plugin == Module && PipelinedLayerStateRHI.ColorImages.IsValidIndex(i))
				{
					ColorImages.Add(PipelinedLayerStateRHI.ColorImages[i]);
					if (bDepthExtensionSupported)
					{
						DepthImages.Add(PipelinedLayerStateRHI.DepthImages[i]);
					}
				}
			}
			EndInfo.next = Module->OnEndFrame(Session, EndInfo.displayTime, ColorImages, DepthImages, EndInfo.next);
		}

		UE_LOG(LogHMD, VeryVerbose, TEXT("Presenting frame predicted to be displayed at %lld"), PipelinedFrameStateRHI.FrameState.predictedDisplayTime);

		XR_ENSURE(xrEndFrame(Session, &EndInfo));
	}

	bIsRendering = false;
}

FXRRenderBridge* FOpenXRHMD::GetActiveRenderBridge_GameThread(bool /* bUseSeparateRenderTarget */)
{
	return RenderBridge;
}

bool FOpenXRHMD::HDRGetMetaDataForStereo(EDisplayOutputFormat& OutDisplayOutputFormat, EDisplayColorGamut& OutDisplayColorGamut, bool& OutbHDRSupported)
{
	if (RenderBridge == nullptr)
	{
		return false;
	}

	return RenderBridge->HDRGetMetaDataForStereo(OutDisplayOutputFormat, OutDisplayColorGamut, OutbHDRSupported);
}

float FOpenXRHMD::GetPixelDenity() const
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();
	return PipelineState.PixelDensity;
}

void FOpenXRHMD::SetPixelDensity(const float NewDensity)
{
	check(IsInGameThread());
	PipelinedFrameStateGame.PixelDensity = FMath::Min(NewDensity, RuntimePixelDensityMax);

	// We have to update the RT state because the new swapchain will be allocated (FSceneViewport::InitDynamicRHI + AllocateRenderTargetTexture)
	// before we call OnBeginRendering_GameThread.
	ENQUEUE_RENDER_COMMAND(UpdatePixelDensity)(
		[this, PixelDensity = PipelinedFrameStateGame.PixelDensity](FRHICommandListImmediate&)
		{
			PipelinedFrameStateRendering.PixelDensity = PixelDensity;
		});
}

FIntPoint FOpenXRHMD::GetIdealRenderTargetSize() const
{
	const FPipelinedFrameState& PipelineState = GetPipelinedFrameStateForThread();

	FIntPoint Size(EForceInit::ForceInitToZero);
	for (int32 ViewIndex = 0; ViewIndex < PipelineState.ViewConfigs.Num(); ViewIndex++)
	{
		const XrViewConfigurationView& Config = PipelineState.ViewConfigs[ViewIndex];

		// If Mobile Multi-View is active the first two views will share the same position
		Size.X = bIsMobileMultiViewEnabled && ViewIndex < 2 ? FMath::Max(Size.X, (int)Config.recommendedImageRectWidth)
			: Size.X + (int)Config.recommendedImageRectWidth;
		Size.Y = FMath::Max(Size.Y, (int)Config.recommendedImageRectHeight);

		// Make sure we quantize in order to be consistent with the rest of the engine in creating our buffers.
		QuantizeSceneBufferSize(Size, Size);
	}

	return Size;
}

FIntRect FOpenXRHMD::GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const
{
	FVector2D SrcNormRectMin(0.05f, 0.2f);
	FVector2D SrcNormRectMax(0.45f, 0.8f);
	if (GetDesiredNumberOfViews(bStereoEnabled) > 2)
	{
		SrcNormRectMin.X /= 2;
		SrcNormRectMax.X /= 2;
	}

	return FIntRect(EyeTexture->GetSizeX() * SrcNormRectMin.X, EyeTexture->GetSizeY() * SrcNormRectMin.Y, EyeTexture->GetSizeX() * SrcNormRectMax.X, EyeTexture->GetSizeY() * SrcNormRectMax.Y);
}

class FDisplayMappingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDisplayMappingPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DISPLAY_MAPPING_PS"), 1);
	}


	FDisplayMappingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(Initializer.ParameterMap, TEXT("OutputGamut"));
		SceneTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		SceneSampler.Bind(Initializer.ParameterMap, TEXT("SceneSampler"));
		TextureToOutputGamutMatrix.Bind(Initializer.ParameterMap, TEXT("TextureToOutputGamutMatrix"));
	}
	FDisplayMappingPS() = default;

	static FMatrix44f GamutToXYZMatrix(EDisplayColorGamut ColorGamut)
	{
		static const FMatrix44f sRGB_2_XYZ_MAT(
			FVector3f(	0.4124564, 0.3575761, 0.1804375),
			FVector3f(	0.2126729, 0.7151522, 0.0721750),
			FVector3f(	0.0193339, 0.1191920, 0.9503041),
			FVector3f(	0        ,         0,         0)
		);

		static const FMatrix44f Rec2020_2_XYZ_MAT(
			FVector3f(	0.6369736, 0.1446172, 0.1688585),
			FVector3f(	0.2627066, 0.6779996, 0.0592938),
			FVector3f(	0.0000000, 0.0280728, 1.0608437),
			FVector3f(	0        ,         0,         0)
		);

		static const FMatrix44f P3D65_2_XYZ_MAT(
			FVector3f(	0.4865906, 0.2656683, 0.1981905),
			FVector3f(	0.2289838, 0.6917402, 0.0792762),
			FVector3f(	0.0000000, 0.0451135, 1.0438031),
			FVector3f(	0        ,         0,         0)
		);
		switch (ColorGamut)
		{
		case EDisplayColorGamut::sRGB_D65: return sRGB_2_XYZ_MAT;
		case EDisplayColorGamut::Rec2020_D65: return Rec2020_2_XYZ_MAT;
		case EDisplayColorGamut::DCIP3_D65: return P3D65_2_XYZ_MAT;
		default:
			checkNoEntry();
			return FMatrix44f::Identity;
		}

	}

	static FMatrix44f XYZToGamutMatrix(EDisplayColorGamut ColorGamut)
	{
		static const FMatrix44f XYZ_2_sRGB_MAT(
			FVector3f(	 3.2409699419, -1.5373831776, -0.4986107603),
			FVector3f(	-0.9692436363,  1.8759675015,  0.0415550574),
			FVector3f(	 0.0556300797, -0.2039769589,  1.0569715142),
			FVector3f(	 0           ,             0,             0)
		);

		static const FMatrix44f XYZ_2_Rec2020_MAT(
			FVector3f(1.7166084, -0.3556621, -0.2533601),
			FVector3f(-0.6666829, 1.6164776, 0.0157685),
			FVector3f(0.0176422, -0.0427763, 0.94222867),
			FVector3f(0, 0, 0)
		);

		static const FMatrix44f XYZ_2_P3D65_MAT(
			FVector3f(	 2.4933963, -0.9313459, -0.4026945),
			FVector3f(	-0.8294868,  1.7626597,  0.0236246),
			FVector3f(	 0.0358507, -0.0761827,  0.9570140),
			FVector3f(	 0        ,         0,         0 )
		);

		switch (ColorGamut)
		{
		case EDisplayColorGamut::sRGB_D65: return XYZ_2_sRGB_MAT;
		case EDisplayColorGamut::Rec2020_D65: return XYZ_2_Rec2020_MAT;
		case EDisplayColorGamut::DCIP3_D65: return XYZ_2_P3D65_MAT;
		default:
			checkNoEntry();
			return FMatrix44f::Identity;
		}

	}

	void SetParameters(FRHICommandList& RHICmdList, EDisplayOutputFormat DisplayOutputFormat, EDisplayColorGamut DisplayColorGamut, EDisplayColorGamut TextureColorGamut, FRHITexture* SceneTextureRHI, bool bSameSize)
	{
		int32 OutputDeviceValue = (int32)DisplayOutputFormat;
		int32 OutputGamutValue = (int32)DisplayColorGamut;

		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), OutputDevice, OutputDeviceValue);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), OutputGamut, OutputGamutValue);

		const FMatrix44f TextureGamutMatrixToXYZ = GamutToXYZMatrix(TextureColorGamut);
		const FMatrix44f XYZToDisplayMatrix = XYZToGamutMatrix(DisplayColorGamut);
		// note: we use mul(m,v) instead of mul(v,m) in the shaders for color conversions which is why matrix multiplication is reversed compared to what we usually do
		const FMatrix44f CombinedMatrix = XYZToDisplayMatrix * TextureGamutMatrixToXYZ;

		SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureToOutputGamutMatrix, CombinedMatrix);

		if (bSameSize)
		{
			SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), SceneTexture, SceneSampler, TStaticSamplerState<SF_Point>::GetRHI(), SceneTextureRHI);
		}
		else
		{
			SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), SceneTexture, SceneSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), SceneTextureRHI);
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/CompositeUIPixelShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("DisplayMappingPS");
	}

private:
	LAYOUT_FIELD(FShaderParameter, OutputDevice);
	LAYOUT_FIELD(FShaderParameter, OutputGamut);
	LAYOUT_FIELD(FShaderParameter, TextureToOutputGamutMatrix);
	LAYOUT_FIELD(FShaderResourceParameter, SceneTexture);
	LAYOUT_FIELD(FShaderResourceParameter, SceneSampler);
};

IMPLEMENT_SHADER_TYPE(, FDisplayMappingPS, TEXT("/Engine/Private/CompositeUIPixelShader.usf"), TEXT("DisplayMappingPS"), SF_Pixel);

void FOpenXRHMD::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, 
											bool bClearBlack, bool bNoAlpha, ERenderTargetActions RTAction, ERHIAccess FinalDstAccess) const
{
	check(IsInRenderingThread());

	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

	const float SrcTextureWidth = SrcTexture->GetSizeX();
	const float SrcTextureHeight = SrcTexture->GetSizeY();
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (SrcRect.IsEmpty())
	{
		SrcRect.Min.X = 0;
		SrcRect.Min.Y = 0;
		SrcRect.Max.X = SrcTextureWidth;
		SrcRect.Max.Y = SrcTextureHeight;
	}
	else
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHITexture * ColorRT = DstTexture->GetTexture2DArray() ? DstTexture->GetTexture2DArray() : DstTexture->GetTexture2D();
	FRHIRenderPassInfo RenderPassInfo(ColorRT, RTAction);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("OpenXRHMD_CopyTexture"));
	{
		if (bClearBlack)
		{
			const FIntRect ClearRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
			RHICmdList.SetViewport(ClearRect.Min.X, ClearRect.Min.Y, 0, ClearRect.Max.X, ClearRect.Max.Y, 1.0f);
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}

		RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = bNoAlpha ? TStaticBlendState<>::GetRHI() : TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

		TShaderRef<FGlobalShader> PixelShader;
		FDisplayMappingPS* DisplayMappingPS = nullptr;
		FScreenPS* ScreenPS = nullptr;

		bool bNeedsDisplayMapping = false;
		EDisplayOutputFormat TVDisplayOutputFormat = EDisplayOutputFormat::SDR_sRGB;
		EDisplayColorGamut HMDColorGamut = EDisplayColorGamut::sRGB_D65;
		EDisplayColorGamut TVColorGamut = EDisplayColorGamut::sRGB_D65;
		if (FinalDstAccess == ERHIAccess::Present && RenderBridge.IsValid())
		{
			EDisplayOutputFormat HMDDisplayFormat;
			bool bHMDSupportHDR;
			if (RenderBridge->HDRGetMetaDataForStereo(HMDDisplayFormat, HMDColorGamut, bHMDSupportHDR))
			{
				bool bTVSupportHDR;
				HDRGetMetaData(TVDisplayOutputFormat, TVColorGamut, bTVSupportHDR, FVector2D(0, 0), FVector2D(0, 0), nullptr);
				if (TVDisplayOutputFormat != HMDDisplayFormat || HMDColorGamut != TVColorGamut || bTVSupportHDR != bHMDSupportHDR)
				{
					// shader assumes G 2.2 for input / ST2084/sRGB for output right now
					ensure(HMDDisplayFormat == EDisplayOutputFormat::SDR_ExplicitGammaMapping);
					ensure(TVDisplayOutputFormat == EDisplayOutputFormat::SDR_sRGB || TVDisplayOutputFormat == EDisplayOutputFormat::HDR_ACES_1000nit_ST2084 || TVDisplayOutputFormat == EDisplayOutputFormat::HDR_ACES_2000nit_ST2084);
					bNeedsDisplayMapping = true;
				}
			}
		}

		bNeedsDisplayMapping &= (GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1);

		if (bNeedsDisplayMapping)
		{
			TShaderMapRef<FDisplayMappingPS> DisplayMappingPSRef(ShaderMap);
			DisplayMappingPS = DisplayMappingPSRef.GetShader();
			PixelShader = DisplayMappingPSRef;
		}
		else
		{
			TShaderMapRef<FScreenPS> ScreenPSRef(ShaderMap);
			ScreenPS = ScreenPSRef.GetShader();
			PixelShader = ScreenPSRef;
		}

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		RHICmdList.Transition(FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));

		const bool bSameSize = DstRect.Size() == SrcRect.Size();
		if (ScreenPS)
		{
			if (bSameSize)
			{
				ScreenPS->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SrcTexture);
			}
			else
			{
				ScreenPS->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);
			}
		}
		else if (DisplayMappingPS)
		{
			DisplayMappingPS->SetParameters(RHICmdList, TVDisplayOutputFormat, TVColorGamut, HMDColorGamut, SrcTexture, bSameSize);
		}

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			ViewportWidth, ViewportHeight,
			U, V,
			USize, VSize,
			TargetSize,
			FIntPoint(1, 1),
			VertexShader,
			EDRF_Default);

	}
	RHICmdList.EndRenderPass();
	
	RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::RTV, FinalDstAccess));
}

void FOpenXRHMD::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, const FXRSwapChainPtr& DstSwapChain, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const
{
	RHICmdList.EnqueueLambda([DstSwapChain](FRHICommandListImmediate& InRHICmdList)
	{
		DstSwapChain->IncrementSwapChainIndex_RHIThread();
		DstSwapChain->WaitCurrentImage_RHIThread(OPENXR_SWAPCHAIN_WAIT_TIMEOUT);
	});

	// Now that we've enqueued the swapchain wait we can add the commands to do the actual texture copy
	FRHITexture2D* const DstTexture = DstSwapChain->GetTexture2DArray() ? DstSwapChain->GetTexture2DArray() : DstSwapChain->GetTexture2D();
	CopyTexture_RenderThread(RHICmdList, SrcTexture, SrcRect, DstTexture, DstRect, bClearBlack, bNoAlpha, ERenderTargetActions::Clear_Store, ERHIAccess::SRVMask);

	// Enqueue a command to release the image after the copy is done
	RHICmdList.EnqueueLambda([DstSwapChain](FRHICommandListImmediate& InRHICmdList)
	{
		DstSwapChain->ReleaseCurrentImage_RHIThread();
	});
}

void FOpenXRHMD::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* SrcTexture, FIntRect SrcRect, FRHITexture2D* DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const
{
	// FIXME: This should probably use the Load_Store action since the spectator controller does multiple overlaying copies.
	CopyTexture_RenderThread(RHICmdList, SrcTexture, SrcRect, DstTexture, DstRect, bClearBlack, bNoAlpha, ERenderTargetActions::DontLoad_Store, ERHIAccess::Present);
}

void FOpenXRHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture* BackBuffer, class FRHITexture* SrcTexture, FVector2D WindowSize) const
{
	if (SpectatorScreenController)
	{
		SpectatorScreenController->RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
	}
}

bool FOpenXRHMD::HasHiddenAreaMesh() const
{
	return HiddenAreaMeshes.Num() > 0;
}

bool FOpenXRHMD::HasVisibleAreaMesh() const
{
	return VisibleAreaMeshes.Num() > 0;
}

void FOpenXRHMD::DrawHiddenAreaMesh(class FRHICommandList& RHICmdList, int32 ViewIndex) const
{
	check(ViewIndex != INDEX_NONE);

	if (ViewIndex < HiddenAreaMeshes.Num())
	{
		const FHMDViewMesh& Mesh = HiddenAreaMeshes[ViewIndex];

		if (Mesh.IsValid())
		{
			RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
			RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
		}
	}
}

void FOpenXRHMD::DrawVisibleAreaMesh(class FRHICommandList& RHICmdList, int32 ViewIndex) const
{
	check(ViewIndex != INDEX_NONE);
	check(ViewIndex < VisibleAreaMeshes.Num());

	if (ViewIndex < VisibleAreaMeshes.Num() && VisibleAreaMeshes[ViewIndex].IsValid())
	{
		const FHMDViewMesh& Mesh = VisibleAreaMeshes[ViewIndex];

		RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
	}
	else
	{
		// Invalid mesh means that entire area is visible, draw a fullscreen quad to simulate
		FPixelShaderUtils::DrawFullscreenQuad(RHICmdList, 1);
	}
}

void FOpenXRHMD::UpdateLayer(FOpenXRLayer& ManagerLayer, uint32 LayerId, bool bIsValid)
{
	for (FOpenXRLayer& NativeLayer : NativeQuadLayers)
	{
		if (NativeLayer.GetLayerId() == LayerId)
		{
			const bool bStaticSwapchain = !(ManagerLayer.Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE);
			NativeLayer.RightEye.bUpdateTexture = ManagerLayer.RightEye.bUpdateTexture;
			NativeLayer.LeftEye.bUpdateTexture = ManagerLayer.LeftEye.bUpdateTexture;
			if (bStaticSwapchain)
			{
				if (NativeLayer.RightEye.bUpdateTexture) 
				{
					NativeLayer.RightEye.Swapchain.Reset();
				}
				if (NativeLayer.LeftEye.bUpdateTexture)
				{
					NativeLayer.LeftEye.Swapchain.Reset();
				}
			}
		}
	}
}

//---------------------------------------------------
// OpenXR Action Space Implementation
//---------------------------------------------------

FOpenXRHMD::FDeviceSpace::FDeviceSpace(XrAction InAction, XrPath InPath)
	: Action(InAction)
	, Space(XR_NULL_HANDLE)
	, Path(InPath)
{
}

FOpenXRHMD::FDeviceSpace::~FDeviceSpace()
{
	DestroySpace();
}

bool FOpenXRHMD::FDeviceSpace::CreateSpace(XrSession InSession)
{
	if (Action == XR_NULL_HANDLE || Space != XR_NULL_HANDLE)
	{
		return false;
	}

	XrActionSpaceCreateInfo ActionSpaceInfo;
	ActionSpaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	ActionSpaceInfo.next = nullptr;
	ActionSpaceInfo.subactionPath = XR_NULL_PATH;
	ActionSpaceInfo.poseInActionSpace = ToXrPose(FTransform::Identity);
	ActionSpaceInfo.action = Action;
	return XR_ENSURE(xrCreateActionSpace(InSession, &ActionSpaceInfo, &Space));
}

void FOpenXRHMD::FDeviceSpace::DestroySpace()
{
	if (Space)
	{
		XR_ENSURE(xrDestroySpace(Space));
	}
	Space = XR_NULL_HANDLE;
}

//---------------------------------------------------
// OpenXR Tracking Space Implementation
//---------------------------------------------------

FOpenXRHMD::FTrackingSpace::FTrackingSpace(XrReferenceSpaceType InType)
	: FTrackingSpace(InType, ToXrPose(FTransform::Identity))
{
}

FOpenXRHMD::FTrackingSpace::FTrackingSpace(XrReferenceSpaceType InType, XrPosef InBasePose)
	: Type(InType)
	, Handle(XR_NULL_HANDLE)
	, BasePose(InBasePose)
{
}

FOpenXRHMD::FTrackingSpace::~FTrackingSpace()
{
	DestroySpace();
}

bool FOpenXRHMD::FTrackingSpace::CreateSpace(XrSession InSession)
{
	DestroySpace();

	XrReferenceSpaceCreateInfo SpaceInfo;
	SpaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	SpaceInfo.next = nullptr;
	SpaceInfo.referenceSpaceType = Type;
	SpaceInfo.poseInReferenceSpace = BasePose;
	return XR_ENSURE(xrCreateReferenceSpace(InSession, &SpaceInfo, &Handle));
}

void FOpenXRHMD::FTrackingSpace::DestroySpace()
{
	if (Handle)
	{
		XR_ENSURE(xrDestroySpace(Handle));
	}
	Handle = XR_NULL_HANDLE;
}
