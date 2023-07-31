// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetEditorViewportClient.h"

#include "ComponentVisualizer.h"
#include "SmartObjectComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UnrealEdGlobals.h"
#include "SmartObjectAssetToolkit.h"
#include "SmartObjectAssetEditorSettings.h"
#include "Editor/UnrealEdEngine.h"

FSmartObjectAssetEditorViewportClient::FSmartObjectAssetEditorViewportClient(const TSharedRef<const FSmartObjectAssetToolkit>& InAssetEditorToolkit, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(&InAssetEditorToolkit->GetEditorModeManager(), InPreviewScene, InEditorViewportWidget)
	, AssetEditorToolkit(InAssetEditorToolkit)
{
	EngineShowFlags.DisableAdvancedFeatures();
	bUsingOrbitCamera = true;

	// Set if the grid will be drawn
	DrawHelper.bDrawGrid =  GetDefault<USmartObjectAssetEditorSettings>()->bShowGridByDefault;
}

void FSmartObjectAssetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	const TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(USmartObjectComponent::StaticClass());
	if (Visualizer.IsValid())
	{
		if (const USmartObjectComponent* Component = PreviewSmartObjectComponent.Get())
		{
			Visualizer->DrawVisualization(Component, View, PDI);
		}
	}
}

void FSmartObjectAssetEditorViewportClient::SetPreviewComponent(USmartObjectComponent* InPreviewComponent)
{
	PreviewSmartObjectComponent = InPreviewComponent;
	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewMesh(UStaticMesh* InStaticMesh)
{
	if (PreviewMeshComponent == nullptr)
	{
		PreviewMeshComponent = NewObject<UStaticMeshComponent>();
		ON_SCOPE_EXIT { PreviewScene->AddComponent(PreviewMeshComponent.Get(),FTransform::Identity); };
	}

	PreviewMeshComponent->SetStaticMesh(InStaticMesh);
	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActor(AActor* InActor)
{
	if (AActor* Actor = PreviewActor.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(Actor);
		PreviewActor.Reset();
	}

	if (InActor != nullptr)
	{
		PreviewActor = PreviewScene->GetWorld()->SpawnActor(InActor->GetClass());
	}

	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActorClass(const UClass* ActorClass)
{
	if (AActor* Actor = PreviewActorFromClass.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(Actor);
		PreviewActorFromClass.Reset();
	}

	if (ActorClass != nullptr)
	{
		PreviewActorFromClass = PreviewScene->GetWorld()->SpawnActor(const_cast<UClass*>(ActorClass));
	}

	FocusViewportOnBox(GetPreviewBounds());
}

FBox FSmartObjectAssetEditorViewportClient::GetPreviewBounds() const
{
	FBoxSphereBounds Bounds(FSphere(FVector::ZeroVector, 100.f));
	if (const AActor* Actor = PreviewActor.Get())
	{
		Bounds = Bounds+ Actor->GetComponentsBoundingBox();
	}

	if (const AActor* Actor = PreviewActorFromClass.Get())
	{
		Bounds = Bounds+ Actor->GetComponentsBoundingBox();
	}

	if (const UStaticMeshComponent* Component = PreviewMeshComponent.Get())
	{
		Bounds = Bounds + Component->CalcBounds(FTransform::Identity);
	}

	const TSharedRef<const FSmartObjectAssetToolkit> Toolkit = AssetEditorToolkit.Pin().ToSharedRef();
	const TArray< UObject* >* EditedObjects = Toolkit->GetObjectsCurrentlyBeingEdited();
	if (EditedObjects != nullptr)
	{
		for (const UObject* EditedObject : *EditedObjects)
		{
			const USmartObjectDefinition* SmartObjectDefinition = Cast<USmartObjectDefinition>(EditedObject);
			if (IsValid(SmartObjectDefinition))
			{
				Bounds = Bounds + SmartObjectDefinition->GetBounds();
			}
		}
	}

	return Bounds.GetBox();
}

