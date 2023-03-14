// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRTrackingSystemBase.h"
#include "DefaultXRCamera.h"
#include "Engine/Engine.h" // for FWorldContext
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "GameFramework/PlayerController.h"
#include "DefaultXRLoadingScreen.h"
#include "UObject/UObjectHash.h"


// Tracking system delegates
FXRTrackingSystemDelegates::FXRTrackingOriginChanged FXRTrackingSystemDelegates::OnXRTrackingOriginChanged;
FXRTrackingSystemDelegates::FXRPlayAreaChanged FXRTrackingSystemDelegates::OnXRPlayAreaChanged;




FXRTrackingSystemBase::FXRTrackingSystemBase(IARSystemSupport* InARImplementation)
	: LoadingScreen(nullptr)
{
	ARCompositionComponent = MakeShared<FARSupportInterface , ESPMode::ThreadSafe>(InARImplementation, this);
}

FXRTrackingSystemBase::~FXRTrackingSystemBase()
{
	if (ARCompositionComponent.IsValid())
	{
		ARCompositionComponent.Reset();
	}

	if (LoadingScreen)
	{
		LoadingScreen->HideLoadingScreen();
		delete LoadingScreen;
		LoadingScreen = nullptr;
	}
}

uint32 FXRTrackingSystemBase::CountTrackedDevices(EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	TArray<int32> DeviceIds;
	if (EnumerateTrackedDevices(DeviceIds, Type))
	{
		return DeviceIds.Num();
	}
	else
	{
		return 0;
	}
}

bool FXRTrackingSystemBase::IsTracking(int32 DeviceId)
{
	FQuat Orientation;
	FVector Position;
	return GetCurrentPose(DeviceId, Orientation, Position);
}

TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > FXRTrackingSystemBase::GetXRCamera(int32 DeviceId)
{
	check(DeviceId == HMDDeviceId);

	if (!XRCamera.IsValid())
	{
		XRCamera = FSceneViewExtensions::NewExtension<FDefaultXRCamera>(this, DeviceId);
	}
	return XRCamera;
}

bool FXRTrackingSystemBase::GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition)
{
	OutOrientation = FQuat::Identity;
	OutPosition = FVector::ZeroVector;
	if (DeviceId == IXRTrackingSystem::HMDDeviceId && (ViewIndex == EStereoscopicEye::eSSE_LEFT_EYE || ViewIndex == EStereoscopicEye::eSSE_RIGHT_EYE))
	{
		OutPosition = FVector(0, (ViewIndex == EStereoscopicEye::eSSE_LEFT_EYE ? -.5 : .5) * 0.064f * GetWorldToMetersScale(), 0);
		return true;
	}
	else
	{
		return false;
	}
}

FTransform FXRTrackingSystemBase::GetTrackingToWorldTransform() const
{
	return CachedTrackingToWorld;
}

void FXRTrackingSystemBase::UpdateTrackingToWorldTransform(const FTransform& TrackingToWorldOverride)
{
	CachedTrackingToWorld = TrackingToWorldOverride;
}

void FXRTrackingSystemBase::CalibrateExternalTrackingSource(const FTransform& ExternalTrackingTransform)
{
	FVector HMDPos;
	FQuat HMDOrientation;
	GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDOrientation, HMDPos);

	FTransform CameraLocalTransform(HMDOrientation, HMDPos);

	CalibratedOffset = ExternalTrackingTransform.GetRelativeTransform(CameraLocalTransform * GetTrackingToWorldTransform());
	UE_LOG(LogHMD, Log, TEXT("XRTracker is now using calibrated offset to external tracking source: [%s]"), *CalibratedOffset.GetTranslation().ToCompactString());
}

void FXRTrackingSystemBase::UpdateExternalTrackingPosition(const FTransform& ExternalTrackingTransform)
{
	FVector HMDPos;
	FQuat HMDOrientation;
	GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDOrientation, HMDPos);

	const FTransform CameraLocalTransform(HMDOrientation, HMDPos);
	const FTransform NewRelativeOffset = ExternalTrackingTransform.GetRelativeTransform(CameraLocalTransform * GetTrackingToWorldTransform());
	const FTransform DeltaOffset = NewRelativeOffset.GetRelativeTransform(CalibratedOffset);

	//UE_LOG(LogHMD, Warning, TEXT("---> Delta from Calibrated: [%s] (%s)"), *DeltaOffset.GetTranslation().ToCompactString(), *DeltaOffset.GetRotation().Rotator().ToCompactString());

	SetBaseOrientation((DeltaOffset.GetRotation().Inverse() * GetBaseOrientation()).GetNormalized());
	SetBasePosition(GetBasePosition() - DeltaOffset.GetTranslation());
}

void FXRTrackingSystemBase::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData.DeviceName = GetSystemName();
	MotionControllerData.ApplicationInstanceID = FApp::GetInstanceId();

	//get all motion controllers and find the one for this hand for this local player
	TArray<UObject*> MotionControllers;
	GetObjectsOfClass(UMotionControllerComponent::StaticClass(), MotionControllers);

	MotionControllerData.bValid = false;
	MotionControllerData.HandIndex = Hand;

	for (int32 MotionControllerIndex = 0; MotionControllerIndex < MotionControllers.Num(); ++MotionControllerIndex)
	{
		UMotionControllerComponent* MotionController = Cast<UMotionControllerComponent>(MotionControllers[MotionControllerIndex]);
		check(MotionController);

		AActor* Owner = MotionController->GetOwner();
		if ((Owner!= nullptr) && (MotionController->GetTrackingSource() == Hand) && (Owner->GetLocalRole() == ROLE_Authority))
		{
			MotionControllerData.bValid = true;
			MotionControllerData.DeviceVisualType = EXRVisualType::Controller;
			MotionControllerData.TrackingStatus = MotionController->CurrentTrackingStatus;

			FTransform WorldTransform = MotionController->GetComponentTransform();
			MotionControllerData.GripPosition = WorldTransform.GetLocation();
			MotionControllerData.GripRotation = WorldTransform.GetRotation();

			MotionControllerData.AimPosition = MotionControllerData.GripPosition;
			MotionControllerData.AimRotation = MotionControllerData.GripRotation;

			break;
		}
	}

	if (!MotionControllerData.bValid)
	{
		UE_LOG(LogHMD, Warning, TEXT("GetMotionControllerData could not find matching UMotionControllerComponent, Hand: %d"), Hand);
	}
}

bool FXRTrackingSystemBase::GetCurrentInteractionProfile(const EControllerHand Hand, FString& InteractionProfile)
{
	UE_LOG(LogHMD, Warning, TEXT("GetCurrentInteractionProfile for %i failed because the current VR tracking system does not support it!"), Hand);

	return false;
}


class IXRLoadingScreen* FXRTrackingSystemBase::GetLoadingScreen()
{
	if (!LoadingScreen)
	{
		LoadingScreen = CreateLoadingScreen();
	}
	return LoadingScreen;
}

class IXRLoadingScreen* FXRTrackingSystemBase::CreateLoadingScreen()
{
	if (GetStereoRenderingDevice().IsValid() && GetStereoRenderingDevice()->GetStereoLayers() && GetStereoRenderingDevice()->GetStereoLayers()->SupportsLayerState())
	{
		return new FDefaultXRLoadingScreen(this);
	}
	return nullptr;
}


TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> FXRTrackingSystemBase::GetARCompositionComponent()
{
	return ARCompositionComponent;
}

const TSharedPtr<const FARSupportInterface , ESPMode::ThreadSafe> FXRTrackingSystemBase::GetARCompositionComponent() const
{
	return ARCompositionComponent;
}

FTransform FXRTrackingSystemBase::RefreshTrackingToWorldTransform(FWorldContext& WorldContext)
{
	CachedTrackingToWorld = ComputeTrackingToWorldTransform(WorldContext);
	return CachedTrackingToWorld;
}

FTransform FXRTrackingSystemBase::ComputeTrackingToWorldTransform(FWorldContext& WorldContext) const
{
	FTransform TrackingToWorld = FTransform::Identity;

	UWorld* World = WorldContext.World();
	if (World)
	{
		// Get the primary player for this world context
		const ULocalPlayer* XRPlayer = GEngine->GetFirstGamePlayer(World);

		if (XRPlayer)
		{
			const APlayerController* PlayerController = XRPlayer->GetPlayerController(World);
			if (PlayerController)
			{
				const AActor* PlayerViewTarget = PlayerController->GetViewTarget();
				// follows ULocalPlayer::GetProjectionData()'s logic - where there's two different  
				// modes we use for determining the HMD's view: implicit vs. explicit...
				// IMPLICIT: the player has a camera component which represents the HMD, meaning 
				//           that component's parent is the tracking origin
				// EXPLICIT: there is no object representing the HMD, so we assume the ViewTaget
				//           is the tracking origin and offset the HMD from that
				const bool bUsesImplicitHMDPositioning = XRCamera.IsValid() ? XRCamera->GetUseImplicitHMDPosition() : 
					PlayerViewTarget && PlayerViewTarget->HasActiveCameraComponent();

				if (bUsesImplicitHMDPositioning)
				{
					TArray<UCameraComponent*> CameraComponents;
					PlayerViewTarget->GetComponents(CameraComponents);

					UCameraComponent* PlayerCamera = nullptr;
					for (UCameraComponent* Cam : CameraComponents)
					{
						// emulates AActor::CalcCamera(); the PlayerCameraManager uses ViewTarget->CalcCamera() for 
						// the HMD view's basis (regardless of whether bLockToHmd is set), CalcCamera() just finds 
						// the first active cam component and chooses that
						if (Cam->IsActive())
						{
							PlayerCamera = Cam;
							break;
						}
					}
					// @TODO: PlayerCamera can be null if we're, say, using debug camera functionality. We should think about supporting these scenarios.
					if (PlayerCamera)
					{
						USceneComponent* ViewParent = PlayerCamera->GetAttachParent();
						if (ViewParent)
						{
							TrackingToWorld = ViewParent->GetComponentTransform();

							if (PlayerCamera->IsUsingAbsoluteLocation())
							{
								TrackingToWorld.SetLocation(FVector::ZeroVector);
							}

							if (PlayerCamera->IsUsingAbsoluteRotation())
							{
								TrackingToWorld.SetRotation(FQuat::Identity);
							}

							if (PlayerCamera->IsUsingAbsoluteScale())
							{
								TrackingToWorld.SetScale3D(FVector::OneVector);
							}
						}
						// else, if the camera is the root component (not attached to an origin point)
						// then it is directly relative to the world - the tracking origin is the world's origin (i.e. the identity)
					}
				}
				// if we don't have a camera component, then the HMD is relative to the player's  
				// ViewPoint (see FDefaultXRCamera::CalculateStereoCameraOffset), which means that the
				// ViewPoint is treated as the tracking origin
				else
				{
					FVector  ViewPos;
					// NOTE: the player's view point will have the HMD's rotation folded into it (see 
					//       APlayerController::UpdateRotation => FDefaultXRCamera::ApplyHMDRotation)
					//       so the ViewPoint's rotation doesn't wholly represent the tracking origin's orientation
					FRotator ViewRot;
					PlayerController->GetPlayerViewPoint(ViewPos, ViewRot);

					// in FDefaultXRCamera::ApplyHMDRotation(), we clear the player's ViewRotation, and 
					// replace it with: the frame's yaw delta + hmd orientation; this has two implications...
					//     1) The HMD is initially relative to the world (not the player's rotation)
					//     2) The tracking origin can be directly rotated with player input, and isn't static
					if (GEngine->XRSystem.IsValid())
					{
						FVector HMDPos;
						FQuat   HMDRot;
						if (GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDRot, HMDPos))
						{
							// @TODO: This is assuming users are using the PlayerController's default implementation for
							//        UpdateRotation(). If they're not calling ApplyHMDRotation() then this is wrong,
							//        and ViewRot should be left unmodified.
							ViewRot = FRotator(ViewRot.Quaternion() * HMDRot.Inverse());
						}
					}

					TrackingToWorld = FTransform(ViewRot, ViewPos);
				}
			}
		}

		// don't need to incorporate the world scale here, as its expected that the XR system
		// backend reports tracking space poses with that already incorporated
		// 
		// @TODO: we should probably move world scale up to this level, instead of having every 
		//        system handle it individually
		//TrackingToWorld.SetScale3D(TrackingToWorld->GetScale3D() * GetWorldToMetersScale(WorldContext));
	}

	return TrackingToWorld;
}

const int32 IXRTrackingSystem::HMDDeviceId;