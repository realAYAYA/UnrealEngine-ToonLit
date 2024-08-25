// Copyright Epic Games, Inc. All Rights Reserved.

#include "Viewport/AvaCameraManager.h"
#include "AvaSequencePlaybackObject.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "CineCameraActor.h"
#include "Engine/Scene.h"
#include "EngineUtils.h"
#include "SceneView.h"

#if WITH_EDITOR
#include "UObject/UObjectGlobals.h"
#endif

FAvaCameraManager::FAvaCameraManager()
{
	ViewTargetWeak = nullptr;
	CameraComponentWeak = nullptr;
	bCameraCut = true; // Start with a camera cut
}

void FAvaCameraManager::Init(IAvaSequencePlaybackObject* InPlaybackObject, bool bInCanvasController)
{
	if (InPlaybackObject)
	{
		InPlaybackObject->GetOnCameraCut().AddSP(this, &FAvaCameraManager::OnUpdateCameraCut);
	}
}

bool FAvaCameraManager::HasViewTarget() const
{
	return ViewTargetWeak.IsValid();
}

AActor* FAvaCameraManager::GetViewTarget() const
{
	return ViewTargetWeak.Get();
}

bool FAvaCameraManager::SetViewTarget(AActor* InNewViewTarget)
{
	AActor* CurrentViewTarget = GetViewTarget();

	if (InNewViewTarget == CurrentViewTarget)
	{
		return false;
	}

	ViewTargetWeak = nullptr;
	CameraComponentWeak = nullptr;
	bCameraCut = true; // set bCameraCut for 2 frames or it doesn't work

	if (IsValid(InNewViewTarget))
	{
		ViewTargetWeak = InNewViewTarget;

		TArray<UCameraComponent*> CameraComponents;
		InNewViewTarget->GetComponents<UCameraComponent>(CameraComponents);

		for (UCameraComponent* CameraComponent : CameraComponents)
		{
			if (!CameraComponent->IsActive())
			{
				continue;
			}

			CameraComponentWeak = CameraComponent;
			break;
		}
	}

	OnViewTargetChanged.Broadcast(this, InNewViewTarget);

	return true;
}

UCameraComponent* FAvaCameraManager::GetCachedCameraComponent() const
{
	return CameraComponentWeak.Get();
}

#if WITH_EDITOR
bool FAvaCameraManager::SetDefaultViewTarget(UWorld* InWorld, FName InDefaultViewTargetName)
{
	if (!IsValid(InWorld))
	{
		UE_LOG(LogViewport, Error, TEXT("Error setting default view target for Motion Design: Null world."));
		return false;
	}

	if (InDefaultViewTargetName == NAME_None)
	{
		return false;
	}

	for (AActor* Actor : TActorRange<AActor>(InWorld))
	{
		if (Actor->GetFName() == InDefaultViewTargetName)
		{
			SetViewTarget(Actor);
			return true;
		}
	}

	UE_LOG(LogViewport, Error, TEXT("Error setting default view target for Motion Design: Startup camera was not found."));
	return false;
}
#endif

FVector FAvaCameraManager::GetViewLocation() const
{
	FMinimalViewInfo ViewInfo;
	SetCameraViewInfo(ViewInfo);

	return ViewInfo.Location;
}

FRotator FAvaCameraManager::GetViewRotation() const
{
	FMinimalViewInfo ViewInfo;
	SetCameraViewInfo(ViewInfo);

	return ViewInfo.Rotation;
}

void FAvaCameraManager::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	FMinimalViewInfo ViewInfo;
	SetCameraViewInfo(ViewInfo);

	OutLocation = ViewInfo.Location;
	OutRotation = ViewInfo.Rotation;
}

void FAvaCameraManager::SetViewPoint(const FVector& InLocation, const FRotator& InRotation)
{
	if (AActor* ViewTarget = ViewTargetWeak.Get())
	{
		ViewTargetWeak->SetActorLocation(InLocation);
		ViewTargetWeak->SetActorRotation(InRotation);
	}
}

void FAvaCameraManager::SetCameraViewInfo(FMinimalViewInfo& InOutViewInfo) const
{
	if (CameraComponentWeak.IsValid())
	{
		UWorld* World = CameraComponentWeak->GetWorld();
		CameraComponentWeak->GetCameraView(IsValid(World) ? World->GetDeltaSeconds() : 0.f, InOutViewInfo);
	}
	else if (ViewTargetWeak.IsValid())
	{
		InOutViewInfo.Location = ViewTargetWeak->GetActorLocation();
		InOutViewInfo.Rotation = ViewTargetWeak->GetActorRotation();
	}
	else
	{
		InOutViewInfo.Location = FVector::ZeroVector;
		InOutViewInfo.Rotation = FRotator::ZeroRotator;
	}

	// We store the originally desired FOV as other classes may adjust to account for ultra-wide aspect ratios
	InOutViewInfo.DesiredFOV = InOutViewInfo.FOV;
}

void FAvaCameraManager::GetExtraPostProcessBlends(TArray<FPostProcessSettings>& OutSettings,
	TArray<float>& OutWeights) const
{
	OutSettings.Empty();
	OutWeights.Empty();

	if (CameraComponentWeak.IsValid())
	{
		CameraComponentWeak->GetExtraPostProcessBlends(OutSettings, OutWeights);
	}
}

void FAvaCameraManager::ApplyExtraPostProcessBlends(FSceneView* InView) const
{
	if (UCameraComponent* CameraComponent = CameraComponentWeak.Get())
	{
		TArray<FPostProcessSettings> OutSettings;
		TArray<float> OutWeights;
		CameraComponent->GetExtraPostProcessBlends(OutSettings, OutWeights);

		for (int32 Idx = 0; Idx < OutSettings.Num(); ++Idx)
		{
			InView->OverridePostProcessSettings(OutSettings[Idx], OutWeights[Idx]);
		}
	}
}

TArray<AActor*> FAvaCameraManager::GetAvailableCameras(UWorld* InWorld)
{
	TArray<AActor*> ActorsWithActiveCameraComponents;

	if (IsValid(InWorld))
	{
		for (AActor* Actor : TActorRange<AActor>(InWorld))
		{
			if (Actor->HasActiveCameraComponent())
			{
				ActorsWithActiveCameraComponents.Add(Actor);
			}
		}
	}

	return ActorsWithActiveCameraComponents;
}

void FAvaCameraManager::OnUpdateCameraCut(UObject* InCameraObject, bool bInJump)
{
	if (AActor* CameraActor = Cast<AActor>(InCameraObject))
	{
		SetViewTarget(CameraActor);
	}
}
