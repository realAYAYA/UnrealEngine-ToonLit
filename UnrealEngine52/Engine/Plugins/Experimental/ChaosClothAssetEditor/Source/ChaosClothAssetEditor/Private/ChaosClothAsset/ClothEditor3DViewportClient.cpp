// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditor3DViewportClient.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothEditorToolkit.h"
#include "EditorModeManager.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "EdModeInteractiveToolsContext.h"
#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Animation/SkeletalMeshActor.h"

FChaosClothAssetEditor3DViewportClient::FChaosClothAssetEditor3DViewportClient(FEditorModeTools* InModeTools,
	FPreviewScene* InPreviewScene, 
	const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	// Call this once with the default value to get everything in a consistent state
	EnableRenderMeshWireframe(bRenderMeshWireframe);

	EngineShowFlags.SetSelectionOutline(true);

	// Set up Gizmo and TransformProxy

	UModeManagerInteractiveToolsContext* const InteractiveToolsContext = ModeTools->GetInteractiveToolsContext();
	TransformProxy = NewObject<UTransformProxy>();

	UInteractiveGizmoManager* const GizmoManager = InteractiveToolsContext->GizmoManager;
	const FString GizmoIdentifier = TEXT("ChaosClothAssetEditor3DViewportClientGizmoIdentifier");
	Gizmo = GizmoManager->Create3AxisTransformGizmo(this, GizmoIdentifier);

	UInteractiveToolManager* const ToolManager = InteractiveToolsContext->ToolManager;
	Gizmo->SetActiveTarget(TransformProxy, ToolManager);
	Gizmo->SetVisibility(true);
	Gizmo->bUseContextGizmoMode = false;
	Gizmo->bUseContextCoordinateSystem = false;
	Gizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Combined;
}

void FChaosClothAssetEditor3DViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEditorViewportClient::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(TransformProxy);
	Collector.AddReferencedObject(Gizmo);
}

void FChaosClothAssetEditor3DViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	auto GetLatestTimestamp = [](const UDataflow* Dataflow, const Dataflow::FContext* Context) -> Dataflow::FTimestamp
	{
		if (Dataflow && Context)
		{
			return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
		}
		return Dataflow::FTimestamp::Invalid;
	};

	if (ClothToolkit.IsValid())
	{
		if (const TSharedPtr<Dataflow::FContext> Context = ClothToolkit->GetDataflowContext())
		{
			if (const UDataflow* const Dataflow = ClothToolkit->GetDataflow())
			{
				if (UDataflowComponent* const DataflowComponent = ClothEdMode->GetDataflowComponent())
				{
					const Dataflow::FTimestamp SystemTimestamp = GetLatestTimestamp(Dataflow, Context.Get());

					if (SystemTimestamp >= LastModifiedTimestamp)
					{
						if (Dataflow->GetRenderTargets().Num())
						{
							// Component Object Rendering
							DataflowComponent->ResetRenderTargets();
							DataflowComponent->SetDataflow(Dataflow);
							DataflowComponent->SetContext(Context);
							for (const UDataflowEdNode* const Node : Dataflow->GetRenderTargets())
							{
								DataflowComponent->AddRenderTarget(Node);
							}
						}
						else
						{
							DataflowComponent->ResetRenderTargets();
						}

						LastModifiedTimestamp = GetLatestTimestamp(Dataflow, Context.Get()).Value + 1;
					}
				}
			}
		}
	}

	// Note: we don't tick the PreviewWorld here, that is done in UChaosClothAssetEditorMode::ModeTick()

}

void FChaosClothAssetEditor3DViewportClient::EnableRenderMeshWireframe(bool bEnable)
{
	bRenderMeshWireframe = bEnable;

	if (ClothComponent)
	{
		ClothComponent->SetForceWireframe(bRenderMeshWireframe);
	}
}

void FChaosClothAssetEditor3DViewportClient::SetClothComponent(TObjectPtr<UChaosClothComponent> InClothComponent)
{
	ClothComponent = InClothComponent;

	if (ClothComponent)
	{
		TransformProxy->AddComponent(ClothComponent);
		TransformProxy->SetTransform(ClothComponent->GetComponentToWorld());
	}
}

void FChaosClothAssetEditor3DViewportClient::SetClothEdMode(TObjectPtr<UChaosClothAssetEditorMode> InClothEdMode)
{
	ClothEdMode = InClothEdMode;
}

void FChaosClothAssetEditor3DViewportClient::SetClothEditorToolkit(TSharedPtr<const FChaosClothAssetEditorToolkit> InClothToolkit)
{
	ClothToolkit = InClothToolkit;
}

void FChaosClothAssetEditor3DViewportClient::SoftResetSimulation()
{
	if (ClothEdMode)
	{
		ClothEdMode->SoftResetSimulation();
	}
}

void FChaosClothAssetEditor3DViewportClient::HardResetSimulation()
{
	if (ClothEdMode)
	{
		ClothEdMode->HardResetSimulation();
	}
}

void FChaosClothAssetEditor3DViewportClient::SuspendSimulation()
{
	if (ClothEdMode)
	{
		ClothEdMode->SuspendSimulation();
	}
}

void FChaosClothAssetEditor3DViewportClient::ResumeSimulation()
{
	if (ClothEdMode)
	{
		ClothEdMode->ResumeSimulation();
	}
}

bool FChaosClothAssetEditor3DViewportClient::IsSimulationSuspended() const
{
	if (ClothEdMode)
	{
		return ClothEdMode->IsSimulationSuspended();
	}

	return false;
}

void FChaosClothAssetEditor3DViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
}

FBox FChaosClothAssetEditor3DViewportClient::PreviewBoundingBox() const
{
	if (ClothEdMode)
	{
		return ClothEdMode->PreviewBoundingBox();
	}

	return FBox(ForceInitToZero);
}

void FChaosClothAssetEditor3DViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	const bool bIsShiftKeyDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);
	const bool bIsCtrlKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);

	USelection* SelectedComponents = ModeTools->GetSelectedComponents();

	TArray<UPrimitiveComponent*> PreviouslySelectedComponents;
	SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(PreviouslySelectedComponents);

	SelectedComponents->Modify();

	if (!bIsShiftKeyDown && !bIsCtrlKeyDown)
	{
		SelectedComponents->DeselectAll();
	}

	if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		const HActor* const ActorProxy = static_cast<HActor*>(HitProxy);
		if (ActorProxy && ActorProxy->Actor)
		{
			const AActor* const Actor = ActorProxy->Actor;		
			USceneComponent* RootComponent = Actor->GetRootComponent();
			if (RootComponent)
			{
				if (bIsShiftKeyDown)
				{
					SelectedComponents->Select(RootComponent);
				}
				else if (bIsCtrlKeyDown)
				{
					// Don't use USelection::ToggleSelect here because that checks membership in GObjectSelection, not the given USelection...
					TArray<USceneComponent*> Components;
					SelectedComponents->GetSelectedObjects(Components);
					const bool bIsSelected = Components.Contains(RootComponent);	

					if (bIsSelected)
					{
						SelectedComponents->Deselect(RootComponent);
					}
					else
					{
						SelectedComponents->Select(RootComponent);
					}
				}
				else
				{
					SelectedComponents->Select(RootComponent);
				}

				if (UPrimitiveComponent* const PrimitiveComponent = Cast<UPrimitiveComponent>(RootComponent))
				{
					PrimitiveComponent->PushSelectionToProxy();
				}
			}
		}
	}

	for (UPrimitiveComponent* const Component : PreviouslySelectedComponents)
	{
		Component->PushSelectionToProxy();
	}

	// Update TransformProxy

	TransformProxy = NewObject<UTransformProxy>();
	TArray<USceneComponent*> Components;
	SelectedComponents->GetSelectedObjects(Components);
	for (USceneComponent* SelectedComponent : Components)
	{
		TransformProxy->AddComponent(SelectedComponent);
	}

	// Update gizmo

	if (Components.Num() > 0)
	{
		Gizmo->SetActiveTarget(TransformProxy);
		Gizmo->SetVisibility(true);
	}
	else
	{
		Gizmo->ClearActiveTarget();
		Gizmo->SetVisibility(false);
	}

}
