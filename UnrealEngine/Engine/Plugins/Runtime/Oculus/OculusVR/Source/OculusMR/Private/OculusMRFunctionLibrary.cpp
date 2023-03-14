// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusMRFunctionLibrary.h"
#include "OculusMRPrivate.h"
#include "OculusMRModule.h"
#include "OculusMR_CastingCameraActor.h"
#include "OculusMR_State.h"
#include "OculusHMD.h"
#include "OculusHMDPrivate.h"
#include "IHeadMountedDisplay.h"

#include "GameFramework/PlayerController.h"

//-------------------------------------------------------------------------------------------------
// UOculusFunctionLibrary
//-------------------------------------------------------------------------------------------------

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_UOculusMRFunctionLibrary::UDEPRECATED_UOculusMRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDEPRECATED_UOculusMRFunctionLibrary::GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly)
{
	TrackedCameras.Empty();

	if (!FOculusMRModule::IsAvailable() || !FOculusMRModule::Get().IsInitialized() )
	{
		UE_LOG(LogMR, Error, TEXT("OculusMR not available"));
		return;
	}

	if (FOculusHMDModule::GetPluginWrapper().GetInitialized() == ovrpBool_False)
	{
		UE_LOG(LogMR, Error, TEXT("OVRPlugin not initialized"));
		return;
	}

	if (OVRP_FAILURE(FOculusHMDModule::GetPluginWrapper().UpdateExternalCamera()))
	{
		UE_LOG(LogMR, Error, TEXT("FOculusHMDModule::GetPluginWrapper().UpdateExternalCamera failure"));
		return;
	}

	int cameraCount = 0;
	if (OVRP_FAILURE(FOculusHMDModule::GetPluginWrapper().GetExternalCameraCount(&cameraCount)))
	{
		UE_LOG(LogMR, Log, TEXT("FOculusHMDModule::GetPluginWrapper().GetExternalCameraCount failure"));
		return;
	}

	for (int i = 0; i < cameraCount; ++i)
	{
		char cameraName[OVRP_EXTERNAL_CAMERA_NAME_SIZE];
		ovrpCameraIntrinsics cameraIntrinsics;
		ovrpCameraExtrinsics cameraExtrinsics;
		FOculusHMDModule::GetPluginWrapper().GetExternalCameraName(i, cameraName);
		FOculusHMDModule::GetPluginWrapper().GetExternalCameraIntrinsics(i, &cameraIntrinsics);
		FOculusHMDModule::GetPluginWrapper().GetExternalCameraExtrinsics(i, &cameraExtrinsics);
		if ((bCalibratedOnly == false || cameraExtrinsics.CameraStatus == ovrpCameraStatus_Calibrated) && cameraIntrinsics.IsValid && cameraExtrinsics.IsValid)
		{
			FTrackedCamera camera;
			camera.Index = i;
			camera.Name = cameraName;
			camera.FieldOfView = FMath::RadiansToDegrees(FMath::Atan(cameraIntrinsics.FOVPort.LeftTan) + FMath::Atan(cameraIntrinsics.FOVPort.RightTan));
			camera.SizeX = cameraIntrinsics.ImageSensorPixelResolution.w;
			camera.SizeY = cameraIntrinsics.ImageSensorPixelResolution.h;
			camera.AttachedTrackedDevice = OculusHMD::ToETrackedDeviceType(cameraExtrinsics.AttachedToNode);
			OculusHMD::FPose Pose;
			GetOculusHMD()->ConvertPose(cameraExtrinsics.RelativePose, Pose);
			camera.CalibratedRotation = Pose.Orientation.Rotator();
			camera.CalibratedOffset = Pose.Position;
			camera.UserRotation = FRotator::ZeroRotator;
			camera.UserOffset = FVector::ZeroVector;
			TrackedCameras.Add(camera);
		}
	}
}

OculusHMD::FOculusHMD* UDEPRECATED_UOculusMRFunctionLibrary::GetOculusHMD()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		static const FName OculusSystemName(TEXT("OculusHMD"));
		if (GEngine->XRSystem->GetSystemName() == OculusSystemName)
		{
			return static_cast<OculusHMD::FOculusHMD*>(GEngine->XRSystem.Get());
		}
	}
#endif
	return nullptr;
}

bool UDEPRECATED_UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation)
{
	if (!TrackingReferenceComponent)
	{
		APlayerController* PlayerController = GWorld->GetFirstPlayerController();
		if (!PlayerController)
		{
			return false;
		}
		APawn* Pawn = PlayerController->GetPawn();
		if (!Pawn)
		{
			return false;
		}
		TRLocation = Pawn->GetActorLocation();
		TRRotation = Pawn->GetActorRotation();
		return true;
	}
	else
	{
		TRLocation = TrackingReferenceComponent->GetComponentLocation();
		TRRotation = TrackingReferenceComponent->GetComponentRotation();
		return true;
	}
}

UDEPRECATED_UOculusMR_Settings* UDEPRECATED_UOculusMRFunctionLibrary::GetOculusMRSettings()
{
	UDEPRECATED_UOculusMR_Settings* Settings = nullptr;
	if (FOculusMRModule::IsAvailable())
	{
		Settings = FOculusMRModule::Get().GetMRSettings();
	}
	return Settings;
}

USceneComponent* UDEPRECATED_UOculusMRFunctionLibrary::GetTrackingReferenceComponent()
{
	USceneComponent * TrackingRef = nullptr;
	if (FOculusMRModule::IsAvailable())
	{
		TrackingRef = FOculusMRModule::Get().GetMRState()->TrackingReferenceComponent;
	}
	return TrackingRef;
}

bool UDEPRECATED_UOculusMRFunctionLibrary::SetTrackingReferenceComponent(USceneComponent* Component)
{
	if (FOculusMRModule::IsAvailable())
	{
		FOculusMRModule::Get().GetMRState()->TrackingReferenceComponent = Component;
		return true;
	}
	return false;
}

float UDEPRECATED_UOculusMRFunctionLibrary::GetMrcScalingFactor()
{
	if (FOculusMRModule::IsAvailable())
	{
		return FOculusMRModule::Get().GetMRState()->ScalingFactor;
	}
	return 0.0;
}

bool UDEPRECATED_UOculusMRFunctionLibrary::SetMrcScalingFactor(float ScalingFactor)
{
	if (FOculusMRModule::IsAvailable() && ScalingFactor > 0.0f)
	{
		FOculusMRModule::Get().GetMRState()->ScalingFactor = ScalingFactor;
		return true;
	}
	return false;
}

bool UDEPRECATED_UOculusMRFunctionLibrary::IsMrcEnabled()
{
	return FOculusMRModule::IsAvailable() && FOculusMRModule::Get().IsInitialized();
}

bool UDEPRECATED_UOculusMRFunctionLibrary::IsMrcActive()
{
	return FOculusMRModule::IsAvailable() && FOculusMRModule::Get().IsActive();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
