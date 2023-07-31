// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraShakePreviewer.h"

#if WITH_EDITOR

#include "LevelEditorViewport.h"
#include "Camera/CameraModifier_CameraShake.h"

FCameraShakePreviewer::FCameraShakePreviewer()
	: PreviewCameraShake(NewObject<UCameraModifier_CameraShake>())
	, LastLocationModifier(FVector::ZeroVector)
	, LastRotationModifier(FRotator::ZeroRotator)
	, LastFOVModifier(0.f)
{
}

FCameraShakePreviewer::~FCameraShakePreviewer()
{
	if (!ensureMsgf(RegisteredViewportClients.Num() == 0, TEXT("Forgot to call UnRegisterViewModifier!")))
	{
		UnRegisterViewModifier();
	}

	PreviewCameraShake = nullptr;
}

void FCameraShakePreviewer::Update(float DeltaTime, bool bIsPlaying)
{
	LastDeltaTime = DeltaTime;

	if (!bIsPlaying)
	{
		LastLocationModifier = FVector::ZeroVector;
		LastRotationModifier = FRotator::ZeroRotator;
		LastFOVModifier = 0.f;
	}
}

void FCameraShakePreviewer::OnModifyView(FEditorViewportViewModifierParams& Params)
{
	const float DeltaTime = LastDeltaTime.Get(-1.f);
	if (DeltaTime > 0.f)
	{
		FMinimalViewInfo OriginalPOV(Params.ViewInfo);

		PreviewCameraShake->ModifyCamera(DeltaTime, Params.ViewInfo);

		LastLocationModifier = Params.ViewInfo.Location - OriginalPOV.Location;
		LastRotationModifier = Params.ViewInfo.Rotation - OriginalPOV.Rotation;
		LastFOVModifier = Params.ViewInfo.FOV - OriginalPOV.FOV;

		LastDeltaTime.Reset();
	}
	else
	{
		Params.ViewInfo.Location += LastLocationModifier;
		Params.ViewInfo.Rotation += LastRotationModifier;
		Params.ViewInfo.FOV += LastFOVModifier;
	}
}

void FCameraShakePreviewer::RegisterViewModifier()
{
	if (GEditor == nullptr)
	{
		return;
	}

	// Register our view modifier on all appropriate viewports, and remember which viewports we did that on.
	// We will later make sure to unregister on the same list, except for any viewport that somehow disappeared since,
	// which we will be notified about with the OnLevelViewportClientListChanged event.
	RegisteredViewportClients.Reset();
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{		
		if (LevelVC && LevelVC->AllowsCinematicControl() && LevelVC->GetViewMode() != VMI_Unknown)
		{
			RegisteredViewportClients.Add(LevelVC);
			LevelVC->ViewModifiers.AddRaw(this, &FCameraShakePreviewer::OnModifyView);
		}
	}

	GEditor->OnLevelViewportClientListChanged().AddRaw(this, &FCameraShakePreviewer::OnLevelViewportClientListChanged);
}

void FCameraShakePreviewer::UnRegisterViewModifier()
{
	if (GEditor == nullptr)
	{
		return;
	}

	GEditor->OnLevelViewportClientListChanged().RemoveAll(this);

	for (FLevelEditorViewportClient* ViewportClient : RegisteredViewportClients)
	{
		ViewportClient->ViewModifiers.RemoveAll(this);
	}
	RegisteredViewportClients.Reset();
}

void FCameraShakePreviewer::OnLevelViewportClientListChanged()
{
	if (GEditor != nullptr)
	{
		// If any viewports were removed while we were playing, simply get rid of them from our list of
		// registered viewports.
		TSet<FLevelEditorViewportClient*> PreviousViewportClients(RegisteredViewportClients);
		TSet<FLevelEditorViewportClient*> NewViewportClients(GEditor->GetLevelViewportClients());
		RegisteredViewportClients = PreviousViewportClients.Intersect(NewViewportClients).Array();
	}
}

#endif

