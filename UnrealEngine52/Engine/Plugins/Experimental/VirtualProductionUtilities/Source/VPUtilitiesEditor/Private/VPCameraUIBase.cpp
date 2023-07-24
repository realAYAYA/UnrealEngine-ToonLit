// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPCameraUIBase.h"
#include "VPBookmark.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Editor/EditorEngine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"


#define LOCTEXT_NAMESPACE "VPCameraUIBase"


bool UVPCameraUIBase::Initialize()
{
	const bool SuperInitialized = UUserWidget::Initialize();

	USelection::SelectNoneEvent.AddUObject(this, &UVPCameraUIBase::OnEditorSelectNone);
	USelection::SelectionChangedEvent.AddUObject(this, &UVPCameraUIBase::OnEditorSelectionChanged);
	USelection::SelectObjectEvent.AddUObject(this, &UVPCameraUIBase::OnEditorSelectionChanged);

	bool bCameraFound = false;
	AActor* Actor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (Actor && !Actor->IsEditorOnly())
	{
		ACameraActor* CameraActor = Cast<ACameraActor>(Actor);
		if (CameraActor)
		{
			SelectedCamera = CameraActor;
			SelectedCameraComponent = CameraActor->GetCameraComponent();
			bCameraFound = true;
			OnSelectedCameraChanged();
		}

		if (!bCameraFound)
		{
			OnEditorSelectNone();
		}
	}
	return SuperInitialized;
}


void UVPCameraUIBase::BeginDestroy()
{
	USelection::SelectNoneEvent.RemoveAll(this);
	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectObjectEvent.RemoveAll(this);

	Super::BeginDestroy();
}


void UVPCameraUIBase::OnEditorSelectionChanged(UObject* NewSelection)
{
	bool bCameraFound = false;
	AActor* Actor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (Actor && !Actor->IsEditorOnly())
	{
		ACameraActor* CameraActor = Cast<ACameraActor>(Actor);
		if (CameraActor)
		{
			SelectedCamera = CameraActor;
			SelectedCameraComponent = CameraActor->GetCameraComponent();
			bCameraFound = true;
			OnSelectedCameraChanged();
		}
		if (!bCameraFound)
		{
			OnEditorSelectNone();
		}
	}
	
}


void UVPCameraUIBase::OnEditorSelectNone()
{
	SelectedCamera = nullptr;
	SelectedCameraComponent = nullptr;
	OnSelectedCameraChanged();
}


#undef LOCTEXT_NAMESPACE