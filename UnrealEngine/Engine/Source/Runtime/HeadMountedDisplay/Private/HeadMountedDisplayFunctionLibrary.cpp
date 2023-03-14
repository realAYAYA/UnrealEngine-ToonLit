// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "ISpectatorScreenController.h"
#include "IXRSystemAssets.h"
#include "Components/PrimitiveComponent.h"
#include "Features/IModularFeatures.h"
#include "XRMotionControllerBase.h" // for GetHandEnumForSourceName()

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeadMountedDisplayFunctionLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogUHeadMountedDisplay, Log, All);

/* UHeadMountedDisplayFunctionLibrary
 *****************************************************************************/

FXRDeviceOnDisconnectDelegate UHeadMountedDisplayFunctionLibrary::OnXRDeviceOnDisconnectDelegate;

TMap<FName, FXRTimedInputActionDelegate> UHeadMountedDisplayFunctionLibrary::OnXRTimedInputActionDelegateMap;

UHeadMountedDisplayFunctionLibrary::UHeadMountedDisplayFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
{
	return GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected()
{
	return GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDConnected();
}

bool UHeadMountedDisplayFunctionLibrary::EnableHMD(bool bEnable)
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		GEngine->XRSystem->GetHMDDevice()->EnableHMD(bEnable);
		if (GEngine->StereoRenderingDevice.IsValid())
		{
			return GEngine->StereoRenderingDevice->EnableStereo(bEnable) || !bEnable; // EnableStereo returns the actual value. When disabling, we always report success.
		}
		else
		{
			return true; // Assume that if we have a valid HMD but no stereo rendering that the operation succeeded.
		}
	}
	return false;
}

FName UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName()
{
	FName DeviceName(NAME_None);

	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		DeviceName = GEngine->XRSystem->GetHMDDevice()->GetHMDName();
	}

	return DeviceName;
}

FString UHeadMountedDisplayFunctionLibrary::GetVersionString()
{
	FString VersionString;
	if (GEngine->XRSystem.IsValid())
	{
		VersionString = GEngine->XRSystem->GetVersionString();
	}
	return VersionString;
}


int32 UHeadMountedDisplayFunctionLibrary::GetXRSystemFlags()
{
	int32 SystemFlags = 0;

	if (GEngine->XRSystem.IsValid())
	{
		SystemFlags = GEngine->XRSystem->GetXRSystemFlags();
		auto HMD = GEngine->XRSystem->GetHMDDevice();
		if (HMD && !HMD->IsHMDConnected())
		{
			// Clear the flags if a HMD device is present but not connected
			// Note that the HMD device is usually the XR system itself
			// and the latter is registered as soon as the corresponding plugin is loaded
			SystemFlags = 0;
		}
	}

	return SystemFlags;
}

EHMDWornState::Type UHeadMountedDisplayFunctionLibrary::GetHMDWornState()
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		return GEngine->XRSystem->GetHMDDevice()->GetHMDWornState();
	}

	return EHMDWornState::Unknown;
}

void UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition)
{
	if(GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;
		FVector Position(0.f);

		GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, OrientationAsQuat, Position);

		DeviceRotation = OrientationAsQuat.Rotator();
		DevicePosition = Position;
	}
	else
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

bool UHeadMountedDisplayFunctionLibrary::HasValidTrackingPosition()
{
	if(GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		return GEngine->XRSystem->HasValidTrackingPosition();
	}

	return false;
}

int32 UHeadMountedDisplayFunctionLibrary::GetNumOfTrackingSensors()
{
	if (GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::TrackingReference);
	}
	return 0;
}

void UHeadMountedDisplayFunctionLibrary::GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float& FarPlane)
{
	bool isActive;
	float LeftFOV;
	float RightFOV;
	float TopFOV;
	float BottomFOV;
	GetTrackingSensorParameters(CameraOrigin, CameraRotation, LeftFOV, RightFOV, TopFOV, BottomFOV, CameraDistance, NearPlane, FarPlane, isActive, 0);
	HFOV = LeftFOV + RightFOV;
	VFOV = TopFOV + BottomFOV;
}

void UHeadMountedDisplayFunctionLibrary::GetTrackingSensorParameters(FVector& Origin, FRotator& Rotation, float& LeftFOV, float& RightFOV, float& TopFOV, float& BottomFOV, float& Distance, float& NearPlane, float& FarPlane, bool& IsActive, int32 Index)
{
	IsActive = false;

	if (Index >= 0 && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed() && GEngine->XRSystem->DoesSupportPositionalTracking())
	{
		TArray<int32> TrackingSensors;
		GEngine->XRSystem->EnumerateTrackedDevices(TrackingSensors, EXRTrackedDeviceType::TrackingReference);

		if (TrackingSensors.Num() > 0)
		{
			FQuat Orientation;
			FXRSensorProperties SensorProperties;
			IsActive = GEngine->XRSystem->GetTrackingSensorProperties(TrackingSensors[Index], Orientation, Origin, SensorProperties);
			Rotation = Orientation.Rotator();
			LeftFOV = SensorProperties.LeftFOV;
			RightFOV = SensorProperties.RightFOV;
			TopFOV = SensorProperties.TopFOV;
			BottomFOV = SensorProperties.BottomFOV;
			Distance = SensorProperties.CameraDistance;
			NearPlane = SensorProperties.NearPlane;
			FarPlane = SensorProperties.FarPlane;
		}
	}
	else
	{
		// No HMD, zero the values
		Origin = FVector::ZeroVector;
		Rotation = FRotator::ZeroRotator;
		LeftFOV = RightFOV = TopFOV = BottomFOV = 0.f;
		NearPlane = 0.f;
		FarPlane = 0.f;
		Distance = 0.f;
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(float Yaw, EOrientPositionSelector::Type Options)
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		switch (Options)
		{
		case EOrientPositionSelector::Orientation:
			GEngine->XRSystem->ResetOrientation(Yaw);
			break;
		case EOrientPositionSelector::Position:
			GEngine->XRSystem->ResetPosition();
			break;
		default:
			GEngine->XRSystem->ResetOrientationAndPosition(Yaw);
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetClippingPlanes(float Near, float Far)
{
	IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;

	if (HMD)
	{
		HMD->SetClippingPlanes(Near, Far);
	}
}

float UHeadMountedDisplayFunctionLibrary::GetPixelDensity()
{
	static const auto PixelDensityTCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("vr.pixeldensity"));
	return PixelDensityTCVar->GetValueOnGameThread();
}

void UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(UObject* WorldContext, float NewScale)
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters = NewScale;
	}
}

float UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(UObject* WorldContext)
{
	return WorldContext ? WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters : 0.f;
}

void UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(TEnumAsByte<EHMDTrackingOrigin::Type> InOrigin)
{
	if (GEngine->XRSystem.IsValid())
	{
		EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;
		switch (InOrigin)
		{
		case EHMDTrackingOrigin::Eye:
			Origin = EHMDTrackingOrigin::Eye;
			break;
		case EHMDTrackingOrigin::Floor:
			Origin = EHMDTrackingOrigin::Floor;
			break;
		case EHMDTrackingOrigin::Stage:
			Origin = EHMDTrackingOrigin::Stage;
			break;
		default:
			break;
		}
		GEngine->XRSystem->SetTrackingOrigin(Origin);
	}
}

TEnumAsByte<EHMDTrackingOrigin::Type> UHeadMountedDisplayFunctionLibrary::GetTrackingOrigin()
{
	EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;

	if (GEngine->XRSystem.IsValid())
	{
		Origin = GEngine->XRSystem->GetTrackingOrigin();
	}

	return Origin;
}

FTransform UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(UObject* WorldContext)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		return TrackingSys->GetTrackingToWorldTransform();
	}
	return FTransform::Identity;
}

void UHeadMountedDisplayFunctionLibrary::CalibrateExternalTrackingToHMD(const FTransform& ExternalTrackingTransform)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->CalibrateExternalTrackingSource(ExternalTrackingTransform);
	}
}

void UHeadMountedDisplayFunctionLibrary::UpdateExternalTrackingHMDPosition(const FTransform& ExternalTrackingTransform)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->UpdateExternalTrackingPosition(ExternalTrackingTransform);
	}
}

void UHeadMountedDisplayFunctionLibrary::GetVRFocusState(bool& bUseFocus, bool& bHasFocus)
{
	IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
	if (HMD)
	{
		bUseFocus = HMD->DoesAppUseVRFocus();
		bHasFocus = HMD->DoesAppHaveVRFocus();
	}
	else
	{
		bUseFocus = bHasFocus = false;
	}
}

namespace HMDFunctionLibraryHelpers
{
	ISpectatorScreenController* GetSpectatorScreenController()
	{
		IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
		if (HMD)
		{
			return HMD->GetSpectatorScreenController();
		}
		return nullptr;
	}
}

bool UHeadMountedDisplayFunctionLibrary::IsSpectatorScreenModeControllable()
{
	return HMDFunctionLibraryHelpers::GetSpectatorScreenController() != nullptr;
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenMode(ESpectatorScreenMode Mode)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		Controller->SetSpectatorScreenMode(Mode);
	}
	else
	{
		static FName PSVRName(TEXT("PSVR"));
		if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetSystemName() == PSVRName)
		{
			UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenMode called while running PSVR, but the SpectatorScreenController was not found.  Perhaps you need to set the plugin project setting bEnableSocialScreenSeparateMode to true to enable it?  Ignoring this call."));
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenTexture(UTexture* InTexture)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		if (!InTexture)
		{
			UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenTexture blueprint function called with null Texture!"));
		}

		Controller->SetSpectatorScreenTexture(InTexture);
	}
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenModeTexturePlusEyeLayout(FVector2D EyeRectMin, FVector2D EyeRectMax, FVector2D TextureRectMin, FVector2D TextureRectMax, bool bDrawEyeFirst /* = true */, bool bClearBlack /* = false */, bool bUseAlpha /* = false */)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		Controller->SetSpectatorScreenModeTexturePlusEyeLayout(FSpectatorScreenModeTexturePlusEyeLayout(EyeRectMin, EyeRectMax, TextureRectMin, TextureRectMax, bDrawEyeFirst, bClearBlack, bUseAlpha));
	}
}

TArray<FXRDeviceId> UHeadMountedDisplayFunctionLibrary::EnumerateTrackedDevices(const FName SystemId, EXRTrackedDeviceType DeviceType)
{
	TArray<FXRDeviceId> DeviceListOut;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (SystemId.IsNone() || TrackingSys->GetSystemName() == SystemId)
		{
			TArray<int32> DeviceIds;
			TrackingSys->EnumerateTrackedDevices(DeviceIds, DeviceType);

			DeviceListOut.Reserve(DeviceListOut.Num() + DeviceIds.Num());
			for (const int32& DeviceId : DeviceIds)
			{
				DeviceListOut.Add(FXRDeviceId(TrackingSys, DeviceId));
			}
		}			
	}

	return DeviceListOut;
}

void UHeadMountedDisplayFunctionLibrary::GetDevicePose(const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position)
{
	bIsTracked = false;
	bHasPositionalTracking = false;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (XRDeviceId.IsOwnedBy(TrackingSys))
		{
			FQuat QuatRotation;
			if (TrackingSys->GetCurrentPose(XRDeviceId.DeviceId, QuatRotation, Position))
			{
				bIsTracked = true;
				bHasPositionalTracking = TrackingSys->HasValidTrackingPosition();

				Orientation = FRotator(QuatRotation);
			}
			else
			{
				Position = FVector::ZeroVector;
				Orientation = FRotator::ZeroRotator;
			}
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::GetDeviceWorldPose(UObject* WorldContext, const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position)
{
	GetDevicePose(XRDeviceId, bIsTracked, Orientation, bHasPositionalTracking, Position);

	const FTransform TrackingToWorld = GetTrackingToWorldTransform(WorldContext);
	Position = TrackingToWorld.TransformPosition(Position);

	FQuat WorldOrientation = TrackingToWorld.TransformRotation(Orientation.Quaternion());
	Orientation = WorldOrientation.Rotator();
}

bool UHeadMountedDisplayFunctionLibrary::IsDeviceTracking(const FXRDeviceId& XRDeviceId)
{
	bool bIsTracked = false;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (XRDeviceId.IsOwnedBy(TrackingSys))
		{
			bIsTracked = TrackingSys->IsTracking(XRDeviceId.DeviceId);
		}
	}

	return bIsTracked;
}


void UHeadMountedDisplayFunctionLibrary::GetHMDData(UObject* WorldContext, FXRHMDData& HMDData)
{
	HMDData.bValid = false;

	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->GetHMDData(WorldContext, HMDData);
	}
}

void UHeadMountedDisplayFunctionLibrary::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData.bValid = false;
	
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->GetMotionControllerData(WorldContext, Hand, MotionControllerData);
	}
}

bool UHeadMountedDisplayFunctionLibrary::ConfigureGestures(const FXRGestureConfig& GestureConfig)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		return TrackingSys->ConfigureGestures(GestureConfig);
	}
	return false;
}

bool UHeadMountedDisplayFunctionLibrary::GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		return TrackingSys->GetCurrentInteractionProfile(Hand, InteractionProfile);
	}

	return false;
}

/** Connect to a remote device for Remote Debugging */
EXRDeviceConnectionResult::Type UHeadMountedDisplayFunctionLibrary::ConnectRemoteXRDevice(const FString& IpAddress, const int32 BitRate)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		return TrackingSys->ConnectRemoteXRDevice(IpAddress, BitRate);
	}
	return EXRDeviceConnectionResult::NoTrackingSystem;
}

void UHeadMountedDisplayFunctionLibrary::DisconnectRemoteXRDevice()
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		TrackingSys->DisconnectRemoteXRDevice();
	}
}

void UHeadMountedDisplayFunctionLibrary::SetXRDisconnectDelegate(const FXRDeviceOnDisconnectDelegate& InDisconnectedDelegate)
{
	OnXRDeviceOnDisconnectDelegate = InDisconnectedDelegate;
}

void UHeadMountedDisplayFunctionLibrary::SetXRTimedInputActionDelegate(const FName& ActionName, const FXRTimedInputActionDelegate& InDelegate)
{
	OnXRTimedInputActionDelegateMap.Add(ActionName, InDelegate);
}

void UHeadMountedDisplayFunctionLibrary::ClearXRTimedInputActionDelegate(const FName& ActionName)
{
	OnXRTimedInputActionDelegateMap.Remove(ActionName);
}

bool UHeadMountedDisplayFunctionLibrary::GetControllerTransformForTime(UObject* WorldContext, const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& bTimeWasUsed, FRotator& Orientation, FVector& Position, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityRadPerSec, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration)
{
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (auto MotionController : MotionControllers)
	{
		if (MotionController == nullptr)
		{
			continue;
		}

		const float WorldToMetersScale = WorldContext ? WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;

		const bool bGotTransform = MotionController->GetControllerOrientationAndPositionForTime(ControllerIndex, MotionSource, Time, bTimeWasUsed, Orientation, Position, bProvidedLinearVelocity, LinearVelocity, bProvidedAngularVelocity, AngularVelocityRadPerSec, bProvidedLinearAcceleration, LinearAcceleration, WorldToMetersScale);
		
		if (bGotTransform)
		{
			// transform to world space
			const FTransform TrackingToWorld = GetTrackingToWorldTransform(WorldContext);

			Position = TrackingToWorld.TransformPosition(Position);
			Orientation = TrackingToWorld.TransformRotation(FQuat(Orientation)).Rotator();

			if (bProvidedLinearVelocity)
			{
				LinearVelocity = TrackingToWorld.TransformVector(LinearVelocity);
			}
			
			if (bProvidedAngularVelocity)
			{
				AngularVelocityRadPerSec = TrackingToWorld.TransformVector(AngularVelocityRadPerSec);
			}

			return true;
		}
	}
	return false;
}

FVector2D UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(TEnumAsByte<EHMDTrackingOrigin::Type> InOrigin)
{
	if (GEngine->XRSystem.IsValid())
	{
		EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Stage;
		switch (InOrigin)
		{
		case EHMDTrackingOrigin::Eye:
			Origin = EHMDTrackingOrigin::Eye;
			break;
		case EHMDTrackingOrigin::Floor:
			Origin = EHMDTrackingOrigin::Floor;
			break;
		case EHMDTrackingOrigin::Stage:
			Origin = EHMDTrackingOrigin::Stage;
			break;
		default:
			break;
		}
		return GEngine->XRSystem->GetPlayAreaBounds(Origin);
	}
	return FVector2D::ZeroVector;
}

bool UHeadMountedDisplayFunctionLibrary::GetTrackingOriginTransform(TEnumAsByte<EHMDTrackingOrigin::Type> Origin, FTransform& OutTransform)
{
	if (GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetTrackingOriginTransform(Origin, OutTransform);
	}
	return false;
}

bool UHeadMountedDisplayFunctionLibrary::GetPlayAreaRect(FTransform& OutTransform, FVector2D& OutRect)
{
	if (GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetPlayAreaRect(OutTransform, OutRect);
	}
	return false;
}


void UHeadMountedDisplayFunctionLibrary::BreakKey(FKey InKey, FString& InteractionProfile, EControllerHand& Hand, FName& MotionSource, FString& Indentifier, FString& Component)
{
	TArray<FString> Tokens;
	if (InKey.ToString().ParseIntoArray(Tokens, TEXT("_")) == EKeys::NUM_XR_KEY_TOKENS)
	{
		InteractionProfile = Tokens[0];
		Hand = Tokens[1] == TEXT("Right") ? EControllerHand::Right : EControllerHand::Left;
		MotionSource = FName(Tokens[1]);
		Indentifier = Tokens[2];
		Component = Tokens[3];
	}
	else
	{
		InteractionProfile.Reset();
		Hand = EControllerHand::AnyHand;
		MotionSource = NAME_None;
		Indentifier.Reset();
		Component.Reset();
	}
}

