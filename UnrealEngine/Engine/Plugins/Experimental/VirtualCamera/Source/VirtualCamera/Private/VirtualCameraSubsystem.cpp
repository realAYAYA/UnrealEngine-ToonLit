// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraSubsystem.h"
#include "LevelSequencePlaybackController.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"

UVirtualCameraSubsystem::UVirtualCameraSubsystem()
	: bIsStreaming(false)
{
	SequencePlaybackController = CreateDefaultSubobject<ULevelSequencePlaybackController>("SequencePlaybackController");
#if WITH_EDITOR
	USelection::SelectionChangedEvent.AddUObject(this, &UVirtualCameraSubsystem::HandleSelectionChangedEvent);
	USelection::SelectObjectEvent.AddUObject(this, &UVirtualCameraSubsystem::HandleSelectObjectEvent);
#endif
}

bool UVirtualCameraSubsystem::StartStreaming()
{
	if (bIsStreaming)
	{
		return false;
	}

	if (ActiveCameraController)
	{
		bIsStreaming = ActiveCameraController->StartStreaming();
	}

	if (bIsStreaming)
	{
		FEditorScriptExecutionGuard ScriptGuard;
		OnStreamStartedDelegate.Broadcast();
	}

	return bIsStreaming;
}

bool UVirtualCameraSubsystem::StopStreaming()
{
	if (!bIsStreaming)
	{
		return false;
	}

	if (ActiveCameraController)
	{
		bIsStreaming = !(ActiveCameraController->StopStreaming());
	}

	if (!bIsStreaming)
	{
		FEditorScriptExecutionGuard ScriptGuard;
		OnStreamStoppedDelegate.Broadcast();
	}

	return bIsStreaming;
}

bool UVirtualCameraSubsystem::IsStreaming() const
{
	return bIsStreaming;
}

#if WITH_EDITOR
void UVirtualCameraSubsystem::HandleSelectionChangedEvent(UObject* ChangedObject)
{
	USelection* Selection = Cast<USelection>(ChangedObject);
	if (Selection)
	{
		if (AActor* SelectedActor = Selection->GetBottom<AActor>()) 
		{
			OnSelectedAnyActorDelegate.Broadcast(SelectedActor); 
		}
	}
}

void UVirtualCameraSubsystem::HandleSelectObjectEvent(UObject* ChangedObject)
{
	if (AActor* SelectedActorCheck1 = Cast<AActor>(ChangedObject))
	{
		OnSelectedActorInViewportDelegate.Broadcast(SelectedActorCheck1);
	}
	else if (AActor* SelectedActorCheck2 = ChangedObject->GetTypedOuter<AActor>())
	{
		OnSelectedActorInViewportDelegate.Broadcast(SelectedActorCheck2);
	}
}
#endif

TScriptInterface<IVirtualCameraController> UVirtualCameraSubsystem::GetVirtualCameraController() const
{
	return ActiveCameraController;
}

void UVirtualCameraSubsystem::SetVirtualCameraController(TScriptInterface<IVirtualCameraController> VirtualCamera)
{
	ActiveCameraController = VirtualCamera;
	//todo deactive the last current, initialize the new active, call back
}
