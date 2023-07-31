// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraCineCameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealEngine.h"

UVirtualCameraCineCameraComponent::UVirtualCameraCineCameraComponent(const FObjectInitializer& ObjectInitializer)
{
	CurrentFilmbackOptionsKey.Empty();

	// Manual focus is the default focus methods
	SetFocusMethod(EVirtualCameraFocusMethod::Manual);
	// Default focus distance
	SetFocusDistance(1000.f);
	// Default to smooth focus changing
	FocusSettings.bSmoothFocusChanges = true;
	// Default smoothing speed
	FocusSettings.FocusSmoothingInterpSpeed = 100.f;
	
	// Constrained aspect ratio is disabled by default, matte is handled by UI
	bConstrainAspectRatio = false;
	// Default view size ratio
	ViewSizeRatio = FVector2D(1.f, 1.f);

	MatteOpacity = .7f;
	
	// By default allow camera view updates
	bAllowCameraViewUpdates = true;
}

void UVirtualCameraCineCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Get a default FilmbackKey name
	TArray<FString> FilmbackKeys;
	if (GetFilmbackPresetOptions(FilmbackKeys))
	{
		CurrentFilmbackOptionsKey = FilmbackKeys[0];
	}

	if (FilmbackOptions.Contains(CurrentFilmbackOptionsKey))
	{
		DesiredFilmbackSettings = FilmbackOptions[CurrentFilmbackOptionsKey];
	}

	SetMatteAspectRatio(DesiredFilmbackSettings.SensorWidth / DesiredFilmbackSettings.SensorHeight);
}

float UVirtualCameraCineCameraComponent::ChangeFocalLengthPreset(const bool bShiftUp)
{
	if (FocalLengthOptions.Num() > 0)
	{
		int32 TargetIndex = FindClosestPresetIndex(FocalLengthOptions, GetCurrentFocalLength());

		// If current focal length is a preset, a move still needs to occur when switching
		if (UKismetMathLibrary::NearlyEqual_FloatFloat(FocalLengthOptions[TargetIndex], GetCurrentFocalLength()))
		{
			TargetIndex += bShiftUp ? 1 : -1;
		}
		// If should shift down and target is above current, move down one
		else if (!bShiftUp && FocalLengthOptions[TargetIndex] > GetCurrentFocalLength())
		{
			TargetIndex -= 1;
		}
		// If should shift up and target is below current, move up one
		else if (bShiftUp && FocalLengthOptions[TargetIndex] < GetCurrentFocalLength())
		{
			TargetIndex += 1;
		}

		TargetIndex = FMath::Clamp<int32>(TargetIndex, 0, FocalLengthOptions.Num() - 1);

		SetCurrentFocalLength(FocalLengthOptions[TargetIndex]);
		return GetCurrentFocalLength();
	}

	// If there are no presets, a value still needs to be returned.
	// A negative value communicates that there are no presets, but everything is still functioning normally.
	return -1.f;
}

float UVirtualCameraCineCameraComponent::ChangeAperturePreset(const bool bShiftUp)
{
	if (ApertureOptions.Num() > 0)
	{
		int32 TargetIndex = FindClosestPresetIndex(ApertureOptions, GetCurrentAperture());

		// If current aperture is a preset, a move still needs to occur when switching
		if (UKismetMathLibrary::NearlyEqual_FloatFloat(ApertureOptions[TargetIndex], GetCurrentAperture()))
		{
			TargetIndex += bShiftUp ? 1 : -1;
		}
		// If should shift down and target is above current, move down one
		else if (!bShiftUp && ApertureOptions[TargetIndex] > GetCurrentAperture())
		{
			TargetIndex -= 1;
		}
		// If should shift up and target is below current, move up one
		else if (bShiftUp && ApertureOptions[TargetIndex] < GetCurrentAperture())
		{
			TargetIndex += 1;
		}

		TargetIndex = FMath::Clamp<int32>(TargetIndex, 0, ApertureOptions.Num() - 1);

		SetCurrentAperture(ApertureOptions[TargetIndex]);
		return GetCurrentFocalLength();
	}

	// If there are no presets, a value still needs to be returned.
	// A negative value communicates that there are no presets, but everything is still functioning normally.
	return -1.f;
}

bool UVirtualCameraCineCameraComponent::GetFilmbackPresetOptions(TArray<FString>& OutFilmbackPresetsArray) const
{
	int32 NumKeys = FilmbackOptions.GetKeys(OutFilmbackPresetsArray);
	return NumKeys > 0;
}

bool UVirtualCameraCineCameraComponent::SetFilmbackPresetOption(const FString& NewFilmbackPreset)
{
	// Search for filmback in filmback array and return it
	if (FilmbackOptions.Contains(NewFilmbackPreset))
	{
		DesiredFilmbackSettings = FilmbackOptions[NewFilmbackPreset];
		CurrentFilmbackOptionsKey = NewFilmbackPreset;
		SetMatteAspectRatio(DesiredFilmbackSettings.SensorWidth / DesiredFilmbackSettings.SensorHeight);
		return true;
	}

	return false;
}

void UVirtualCameraCineCameraComponent::GetMatteValues(TArray<float>& OutMatteValues) const 
{
	OutMatteValues = MatteOptions;
}

void UVirtualCameraCineCameraComponent::UpdateCameraView()
{
	// Check to make sure frames should be updated
	// Scenarios where updates are unwanted include taking a screenshot or recording a sequence
	if (bAllowCameraViewUpdates && FilmbackOptions.Contains(CurrentFilmbackOptionsKey))
	{
		DesiredFilmbackSettings = FilmbackOptions[CurrentFilmbackOptionsKey];
		DesiredFilmbackSettings.SensorAspectRatio = DesiredFilmbackSettings.SensorWidth / DesiredFilmbackSettings.SensorHeight;
		float ViewSizeAdjustmentForMatte = MatteAspectRatio / DesiredFilmbackSettings.SensorAspectRatio;

		// Set the actual camera filmback settings
		if (ViewSizeAdjustmentForMatte >= 1.f)
		{
			Filmback.SensorWidth = DesiredFilmbackSettings.SensorWidth / ViewSizeRatio.X;
			Filmback.SensorHeight = DesiredFilmbackSettings.SensorHeight / ViewSizeRatio.Y;
		}
		else
		{
			Filmback.SensorWidth = DesiredFilmbackSettings.SensorWidth / ViewSizeRatio.X * ViewSizeAdjustmentForMatte;
			Filmback.SensorHeight = DesiredFilmbackSettings.SensorHeight / ViewSizeRatio.Y * ViewSizeAdjustmentForMatte;
		}
	}
}

bool UVirtualCameraCineCameraComponent::SetMatteAspectRatio(const float NewMatteAspectRatio)
{
	if (NewMatteAspectRatio > 0)
	{
		MatteAspectRatio = NewMatteAspectRatio;
		return true;
	}

	return false;
}

void UVirtualCameraCineCameraComponent::SetFocusMethod(const EVirtualCameraFocusMethod NewFocusMethod)
{
	CurrentFocusMethod = NewFocusMethod;

	switch (NewFocusMethod)
	{
	case EVirtualCameraFocusMethod::None:
		bAutoFocusEnabled = false;
		FocusSettings.FocusMethod = ECameraFocusMethod::DoNotOverride;
		break;
	case EVirtualCameraFocusMethod::Auto:
		bAutoFocusEnabled = true;
		FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		break;
	case EVirtualCameraFocusMethod::Manual:
		bAutoFocusEnabled = false;
		FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		break;
	case EVirtualCameraFocusMethod::Tracking:
		bAutoFocusEnabled = false;
		FocusSettings.FocusMethod = ECameraFocusMethod::Tracking;
		break;
	default:  // Should never be reached, but just in case new focus methods are added
		UE_LOG(LogActor, Warning, TEXT("Specified focus method is not currently supported in Virtual Camera!"))
		break;
	}
}

void UVirtualCameraCineCameraComponent::SetFocusDistance(const float NewFocusDistance)
{
	FocusSettings.ManualFocusDistance = NewFocusDistance;
	FocusSettings.FocusOffset = 0.f;
}

void UVirtualCameraCineCameraComponent::SetTrackedActorForFocus(AActor* ActorToTrack, const FVector TrackingPointOffset)
{
	FocusSettings.TrackingFocusSettings.ActorToTrack = ActorToTrack;
	FocusSettings.TrackingFocusSettings.RelativeOffset = TrackingPointOffset;
}

void UVirtualCameraCineCameraComponent::AddBlendableToCamera(TScriptInterface<IBlendableInterface> BlendableToAdd, float Weight)
{
	PostProcessSettings.AddBlendable(BlendableToAdd, Weight);
}

void UVirtualCameraCineCameraComponent::SetFocusChangeSmoothness(const float NewFocusChangeSmoothness)
{
	FocusChangeSmoothness = FMath::Clamp<float>(NewFocusChangeSmoothness, 0.f, 1.f);

	// Translate the user-facing value into the actual interpolation speed
	FocusSettings.FocusSmoothingInterpSpeed = FMath::Lerp(10000.f, 1.f, NewFocusChangeSmoothness);
}

void UVirtualCameraCineCameraComponent::SetFocusVisualization(bool bShowFocusVisualization)
{
#if WITH_EDITOR
	if (FocusSettings.FocusMethod == ECameraFocusMethod::Disable)
	{
		UE_LOG(LogActor, Warning, TEXT("Camera focus mode is currently set to none, cannot display focus plane!"))
		return;
	}

	FocusSettings.bDrawDebugFocusPlane = bShowFocusVisualization;
#endif
}

bool UVirtualCameraCineCameraComponent::IsFocusVisualizationActivated() const
{
#if WITH_EDITOR
	return FocusSettings.FocusMethod != ECameraFocusMethod::Disable && FocusSettings.bDrawDebugFocusPlane;
#else
	return false;
#endif
}

int32 UVirtualCameraCineCameraComponent::FindClosestPresetIndex(const TArray<float>& ArrayToSearch, const float SearchValue) const
{
	float LowestDifference = 0.f;
	int32 ClosestPresetIndex = 0;

	for (int32 Index = 0; Index < ArrayToSearch.Num(); Index++)
	{
		float Difference = FMath::Abs(ArrayToSearch[Index] - SearchValue);
		if (Difference < LowestDifference || Index == 0)
		{
			LowestDifference = Difference;
			ClosestPresetIndex = Index;
		}
	}

	return ClosestPresetIndex;
}
