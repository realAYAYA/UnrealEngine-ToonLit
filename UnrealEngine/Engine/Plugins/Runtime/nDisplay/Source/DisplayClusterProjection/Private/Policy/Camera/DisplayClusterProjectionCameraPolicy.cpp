// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Camera/DisplayClusterProjectionCameraPolicy.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Game/IDisplayClusterGameManager.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "ComposurePostMoves.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"


FDisplayClusterProjectionCameraPolicy::FDisplayClusterProjectionCameraPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
const FString& FDisplayClusterProjectionCameraPolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::Camera);
	return Type;
}

bool FDisplayClusterProjectionCameraPolicy::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	return true;
}

void FDisplayClusterProjectionCameraPolicy::HandleEndScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	CameraRef.ResetSceneComponent();
}

APlayerCameraManager* const GetCurPlayerCameraManager(IDisplayClusterViewport* InViewport)
{
	if (InViewport)
	{
		UWorld* World = InViewport->GetOwner().GetCurrentWorld();
		if (World)
		{
			APlayerController* const CurPlayerController = World->GetFirstPlayerController();
			if (CurPlayerController)
			{
				return CurPlayerController->PlayerCameraManager;
			}
		}
	}

	return nullptr;
}

UCameraComponent* FDisplayClusterProjectionCameraPolicy::GetCameraComponent()
{
	USceneComponent* SceneComponent = CameraRef.GetOrFindSceneComponent();
	if (SceneComponent)
	{
		UCameraComponent* CameraComponent = Cast<UCameraComponent>(SceneComponent);
		if (CameraComponent)
		{
			return CameraComponent;
		}
	}
	
	return  nullptr;
}

bool FDisplayClusterProjectionCameraPolicy::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
{
	check(IsInGameThread());

	InOutViewLocation = FVector::ZeroVector;
	InOutViewRotation = FRotator::ZeroRotator;
	
	// Save Z values
	ZNear = NCP;
	ZFar  = FCP;

	// Use assigned camera
	if (UCameraComponent* CameraComponent = GetCameraComponent())
	{
		if (UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(CameraComponent))
		{
			if (CineCameraComponent->bOverride_CustomNearClippingPlane)
			{
				ZNear = CineCameraComponent->CustomNearClippingPlane;
				ZFar = ZNear;
			}
		}

		InOutViewLocation = CameraComponent->GetComponentLocation();
		InOutViewRotation = CameraComponent->GetComponentRotation();
	}
	// Otherwise default UE camera is used
	else
	{
		APlayerCameraManager* const CurPlayerCameraManager = GetCurPlayerCameraManager(InViewport);
		if (CurPlayerCameraManager)
		{
			InOutViewLocation = CurPlayerCameraManager->GetCameraLocation();
			InOutViewRotation = CurPlayerCameraManager->GetCameraRotation();
		}
	}

	// Fix camera lens deffects (prototype)
	InOutViewLocation += CameraSettings.FrustumOffset;
	InOutViewRotation += CameraSettings.FrustumRotation;

	return true;
}

bool FDisplayClusterProjectionCameraPolicy::ImplGetProjectionMatrix(const float InCameraFOV, const float InCameraAspectRatio, IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	// The horizontal field of view (in degrees)
	const float ScaledCameraFOV = InCameraFOV * CameraSettings.FOVMultiplier;

	// Clamp camera fov to valid range [1.f, 178.f]
	const float ClampedCameraFOV = FMath::Clamp(ScaledCameraFOV, 1.f, 178.f);
	if (ClampedCameraFOV != ScaledCameraFOV && !IsEditorOperationMode(InViewport))
	{
		UE_LOG(LogDisplayClusterProjectionCamera, Warning, TEXT("CameraFOV clamped: '%d' -> '%d'. (FieldOfView='%d', FOVMultiplier='%d'"), ScaledCameraFOV, ClampedCameraFOV, InCameraFOV, CameraSettings.FOVMultiplier);
	}

	if (InViewport)
	{
		// Support inner camera custom frustum
		const float HalfFOVH = 0.5 * ClampedCameraFOV;
		const float HalfFOVV = HalfFOVH / InCameraAspectRatio;

		InViewport->CalculateProjectionMatrix(InContextNum, -HalfFOVH, HalfFOVH, HalfFOVV, -HalfFOVV, ZNear, ZFar, true);
		OutPrjMatrix = InViewport->GetContexts()[InContextNum].ProjectionMatrix;
	}
	else
	{
		FComposurePostMoveSettings ComposureSettings;
		OutPrjMatrix = ComposureSettings.GetProjectionMatrix(ClampedCameraFOV, InCameraAspectRatio);
	}

	return true;
}

bool FDisplayClusterProjectionCameraPolicy::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());

	if (UCameraComponent* CameraComponent = GetCameraComponent())
	{
		return ImplGetProjectionMatrix(CameraComponent->FieldOfView, CameraComponent->AspectRatio, InViewport, InContextNum, OutPrjMatrix);
	}
	else
	{
		APlayerCameraManager* const CurPlayerCameraManager = GetCurPlayerCameraManager(InViewport);
		if (CurPlayerCameraManager)
		{
			return ImplGetProjectionMatrix(CurPlayerCameraManager->GetFOVAngle(), CurPlayerCameraManager->DefaultAspectRatio, InViewport, InContextNum, OutPrjMatrix);
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionCameraPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterProjectionCameraPolicy::SetCamera(UCameraComponent* NewCamera, const FDisplayClusterProjectionCameraPolicySettings& InCameraSettings)
{
	if(CameraSettings.bCameraOverrideDefaults && InCameraSettings.bCameraOverrideDefaults == false)
	{
		// Ignore default camera updates (UE-137222)
		return;
	}

	CameraSettings = InCameraSettings;

	if (NewCamera)
	{
		UE_LOG(LogDisplayClusterProjectionCamera, Verbose, TEXT("New camera set: %s"), *NewCamera->GetFullName());

		CameraRef.SetSceneComponent(NewCamera);
	}
	else
	{
		if (InCameraSettings.bCameraOverrideDefaults == false)
		{
			if (!IsEditorOperationMode(nullptr))
			{
				UE_LOG(LogDisplayClusterProjectionCamera, Warning, TEXT("Trying to set nullptr camera pointer"));
			}
		}

		CameraRef.ResetSceneComponent();

		// After ref reset, allow to use default cameras again
		CameraSettings.bCameraOverrideDefaults = false;
	}
}
