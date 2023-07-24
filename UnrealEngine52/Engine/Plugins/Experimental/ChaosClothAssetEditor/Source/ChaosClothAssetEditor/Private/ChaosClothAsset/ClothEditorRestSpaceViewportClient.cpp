// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorRestSpaceViewportClient.h"

#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "EdModeInteractiveToolsContext.h"
#include "CameraController.h"
#include "Components/DynamicMeshComponent.h"
#include "Framework/Application/SlateApplication.h"

FChaosClothEditorRestSpaceViewportClient::FChaosClothEditorRestSpaceViewportClient(FEditorModeTools* InModeTools,
	FPreviewScene* InPreviewScene,
	const TWeakPtr<SEditorViewport>& InEditorViewportWidget) :
	FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	EngineShowFlags.Grid = true;
	DrawHelper.bDrawGrid = true;

	bDrawAxes = false;

	// Don't automatically switch to wireframe rendering in ortho mode
	SetViewModes(EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Lit);

	EngineShowFlags.SetSelectionOutline(true);
}

void FChaosClothEditorRestSpaceViewportClient::Set2DMode(bool In2DMode)
{
	b2DMode = In2DMode;

	if (b2DMode)
	{
		SetViewportType(ELevelViewportType::LVT_OrthoXY);
	}
	else
	{
		SetViewportType(ELevelViewportType::LVT_Perspective);
	}
}


bool FChaosClothEditorRestSpaceViewportClient::ShouldOrbitCamera() const
{
	if (b2DMode)
	{
		return false;
	}
	else
	{
		return FEditorViewportClient::ShouldOrbitCamera();
	}
}

bool FChaosClothEditorRestSpaceViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	// See if any tool commands want to handle the key event
	const TSharedPtr<FUICommandList> PinnedToolCommandList = ToolCommandList.Pin();
	if (EventArgs.Event != IE_Released && PinnedToolCommandList.IsValid())
	{
		const FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (PinnedToolCommandList->ProcessCommandBindings(EventArgs.Key, KeyState, (EventArgs.Event == IE_Repeat)))
		{
			return true;
		}
	}

	return FEditorViewportClient::InputKey(EventArgs);
}


void FChaosClothEditorRestSpaceViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	// TODO: Add/modify selection if modifier keys are pressed
	USelection* SelectedComponents = ModeTools->GetSelectedComponents();
	SelectedComponents->Modify();
	SelectedComponents->BeginBatchSelectOperation();

	TArray<UDynamicMeshComponent*> PreviouslySelectedComponents;
	SelectedComponents->GetSelectedObjects<UDynamicMeshComponent>(PreviouslySelectedComponents);
	SelectedComponents->DeselectAll(UDynamicMeshComponent::StaticClass());

	if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		const HActor* ActorProxy = static_cast<HActor*>(HitProxy);
		if (ActorProxy && ActorProxy->Actor )
		{
			const AActor* Actor = ActorProxy->Actor;
			const TSet<UActorComponent*>& OwnedComponents = Actor->GetComponents();
			for (UActorComponent* Component : OwnedComponents)
			{
				if (UDynamicMeshComponent* DynamicMeshComp = Cast<UDynamicMeshComponent>(Component))
				{
					SelectedComponents->Select(DynamicMeshComp);
					DynamicMeshComp->PushSelectionToProxy();
				}
			}
		}
	}

	SelectedComponents->EndBatchSelectOperation();

	for (UDynamicMeshComponent* Component : PreviouslySelectedComponents)
	{
		Component->PushSelectionToProxy();
	}

}

void FChaosClothEditorRestSpaceViewportClient::SetEditorViewportWidget(TWeakPtr<SEditorViewport> InEditorViewportWidget)
{
	EditorViewportWidget = InEditorViewportWidget;
}

void FChaosClothEditorRestSpaceViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}