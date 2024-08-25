// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowPreviewScene.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/CollectionRenderingPatternUtility.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "InteractiveTool.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "FDataflowPreviewScene"

FDataflowPreviewScene::FDataflowPreviewScene(FPreviewScene::ConstructionValues ConstructionValues, TObjectPtr<UDataflowBaseContent> InEditorContent) 
	: FAdvancedPreviewScene(ConstructionValues), DataflowContent(InEditorContent)
{
	check(DataflowContent);
	SetFloorVisibility(true, true);

	RootSceneActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
}

FDataflowPreviewScene::~FDataflowPreviewScene()
{}

void FDataflowPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(DataflowContent);
	Collector.AddReferencedObject(RootSceneActor);
	
	GetDataflowContent()->AddContentObjects(Collector);
}

bool FDataflowPreviewScene::IsComponentSelected(const UPrimitiveComponent* InComponent) const
{
	if(DataflowModeManager.IsValid())
	{
		if (const UTypedElementSelectionSet* const TypedElementSelectionSet = DataflowModeManager->GetEditorSelectionSet())
		{
			if (const FTypedElementHandle ComponentElement = UEngineElementsLibrary::AcquireEditorComponentElementHandle(InComponent))
			{
				const bool bElementSelected = TypedElementSelectionSet->IsElementSelected(ComponentElement, FTypedElementIsSelectedOptions());
				return bElementSelected;
			}
		}
	}
	return false;
}

FBox FDataflowPreviewScene::GetBoundingBox() const
{
	FBox SceneBounds(ForceInitToZero);
	if(DataflowModeManager.IsValid())
	{
		USelection* const SelectedComponents = DataflowModeManager->GetSelectedComponents();

		TArray<TWeakObjectPtr<UObject>> SelectedObjects;
		const int32 NumSelected = SelectedComponents->GetSelectedObjects(SelectedObjects);
		
		if(NumSelected > 0)
		{
			for(const TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
			{
				if(const UPrimitiveComponent* SelectedComponent = Cast<UPrimitiveComponent>(SelectedObject))
				{
					SceneBounds += SelectedComponent->Bounds.GetBox();
				}
			}
		}
		else
		{
			SceneBounds += RootSceneActor->GetComponentsBoundingBox(true);
		}
	}
	return SceneBounds;
}

FDataflowConstructionScene::FDataflowConstructionScene(FPreviewScene::ConstructionValues ConstructionValues, TObjectPtr<UDataflowBaseContent> InEditorContent) 
	: FDataflowPreviewScene(ConstructionValues,InEditorContent)
{}

FDataflowConstructionScene::~FDataflowConstructionScene()
{
	ResetDynamicMeshComponents();
}

void FDataflowConstructionScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowPreviewScene::AddReferencedObjects(Collector);

	Collector.AddReferencedObjects(DynamicMeshComponents);
	Collector.AddReferencedObject(WireframeDraw);
}

FORCEINLINE Dataflow::FTimestamp LatestTimestamp(const UDataflow* Dataflow, const ::Dataflow::FContext* Context)
{
	if (Dataflow && Context)
	{
		return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
	}
	return ::Dataflow::FTimestamp::Invalid;
}

void FDataflowConstructionScene::TickDataflowScene(const float DeltaSeconds)
{
	if (const TSharedPtr<Dataflow::FContext> DataflowContext = DataflowContent->GetDataflowContext())
	{
		if (const UDataflow* Dataflow = DataflowContent->DataflowAsset)
		{
			const Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, DataflowContext.Get());
			if (SystemTimestamp >= DataflowContent->GetLastModifiedTimestamp() || DataflowContent->IsDirty())
			{
				DataflowContent->SetLastModifiedTimestamp(SystemTimestamp.Value + 1);

				if(DataflowContent->IsDirty())
				{
					UpdateConstructionScene();
				}
			}
		}
	}
	for (TObjectPtr<UInteractiveToolPropertySet>& Propset : PropertyObjectsToTick)
	{
		if (Propset)
		{
			if (Propset->IsPropertySetEnabled())
			{
				Propset->CheckAndUpdateWatched();
			}
			else
			{
				Propset->SilentUpdateWatched();
			}
		}
	}

	if (WireframeDraw)
	{
		WireframeDraw->OnTick(DeltaSeconds);
	}
}

void FDataflowConstructionScene::UpdateDynamicMeshComponents()
{
	using namespace UE::Geometry;//FDynamicMesh3

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	ResetDynamicMeshComponents();

	if (DataflowContent)
	{
		const TObjectPtr<UDataflow>& DataflowAsset = DataflowContent->GetDataflowAsset();
		const TSharedPtr<Dataflow::FEngineContext>& DataflowContext = DataflowContent->GetDataflowContext();
		if(DataflowAsset && DataflowContext)
		{
			for (const UDataflowEdNode* Target : DataflowAsset->GetRenderTargets())
			{
				if (Target)
				{
					FDynamicMesh3 DynamicMesh;
					TSharedPtr<FManagedArrayCollection> RenderCollection(new FManagedArrayCollection);
					GeometryCollection::Facades::FRenderingFacade Facade(*RenderCollection);
					Facade.DefineSchema();

					Target->Render(Facade, DataflowContext);
					Dataflow::Conversion::RenderingFacadeToDynamicMesh(Facade, DynamicMesh);
					
					if (Target == DataflowContent->GetPrimarySelectedNode())
					{
						DataflowContent->SetPrimaryRenderCollection(RenderCollection);
					}
					
					AddDynamicMeshComponent(MoveTemp(DynamicMesh), {});
				}
			}
		}
	}
}

void FDataflowConstructionScene::ResetDynamicMeshComponents()
{
	USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
	for(const TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent : DynamicMeshComponents)
	{
		DynamicMeshComponent->SelectionOverrideDelegate.Unbind();
		if (SelectedComponents->IsSelected(DynamicMeshComponent))
		{
			SelectedComponents->Deselect(DynamicMeshComponent);
			DynamicMeshComponent->PushSelectionToProxy();
		}
		RemoveComponent(DynamicMeshComponent);
	}
	DynamicMeshComponents.Reset();
}

TObjectPtr<UDynamicMeshComponent>& FDataflowConstructionScene::AddDynamicMeshComponent(UE::Geometry::FDynamicMesh3&& DynamicMesh, const TArray<UMaterialInterface*>& MaterialSet)
{
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent = NewObject<UDynamicMeshComponent>(RootSceneActor);
		
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
	
	// @todo(Material) This is just to have a material, we should transfer the materials from the assets if they have them. 
	if (DataflowContent && DataflowContent->DataflowAsset && DataflowContent->DataflowAsset->Material)
	{
		DynamicMeshComponent->ConfigureMaterialSet({ DataflowContent->DataflowAsset->Material });
	}
	else
	{
		DynamicMeshComponent->SetOverrideRenderMaterial(FDataflowEditorStyle::Get().VertexMaterial);
		DynamicMeshComponent->SetShadowsEnabled(false);
	}
	//else if (FDataflowEditorStyle::Get().DefaultMaterial)
	//{
	//	DynamicMeshComponent->ConfigureMaterialSet({ FDataflowEditorStyle::Get().DefaultMaterial });
	//}
	//else
	//{
	//	DynamicMeshComponent->ValidateMaterialSlots(true, false);
	//}

	DynamicMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
	DynamicMeshComponent->UpdateBounds();

	AddComponent(DynamicMeshComponent, DynamicMeshComponent->GetRelativeTransform());
		
	const int32 ElementIndex = DynamicMeshComponents.Emplace(DynamicMeshComponent);
	return DynamicMeshComponents[ElementIndex];
}

void FDataflowConstructionScene::AddWireframeMeshElementsVisualizer()
{
	ensure(WireframeDraw==nullptr);
	if (DynamicMeshComponents.Num())
	{
		// Set up the wireframe display of the rest space mesh.

		WireframeDraw = NewObject<UMeshElementsVisualizer>(RootSceneActor);
		WireframeDraw->CreateInWorld(GetWorld(), FTransform::Identity);

		WireframeDraw->Settings->DepthBias = 2.0;
		WireframeDraw->Settings->bAdjustDepthBiasUsingMeshSize = false;
		WireframeDraw->Settings->bShowWireframe = true;
		WireframeDraw->Settings->bShowBorders = true;
		WireframeDraw->Settings->bShowUVSeams = false;

		WireframeDraw->WireframeComponent->BoundaryEdgeThickness = 2;

		WireframeDraw->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) 
		{
			for (auto DynamicMeshComponent : DynamicMeshComponents) ProcessFunc(*DynamicMeshComponent->GetMesh());
		});

		for (auto DynamicMeshComponent : DynamicMeshComponents)
		{
			DynamicMeshComponent->OnMeshChanged.Add(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
			{
				WireframeDraw->NotifyMeshChanged();
			}));

			const bool bRestSpaceMeshVisible = DynamicMeshComponent->GetVisibleFlag();
			WireframeDraw->Settings->bVisible = bRestSpaceMeshVisible && bConstructionViewWireframe;
		}
		PropertyObjectsToTick.Add(WireframeDraw->Settings);
	}
}

void FDataflowConstructionScene::ResetWireframeMeshElementsVisualizer()
{
	if (WireframeDraw)
	{
		WireframeDraw->Disconnect();
	}
	WireframeDraw = nullptr;
}

void FDataflowConstructionScene::UpdateWireframeMeshElementsVisualizer()
{
	ResetWireframeMeshElementsVisualizer();
	AddWireframeMeshElementsVisualizer();
}

bool FDataflowConstructionScene::HasRenderableGeometry()
{
	for (auto& DynamicMeshComponent : DynamicMeshComponents)
	{
		if (DynamicMeshComponent->GetMesh()->TriangleCount() > 0)
		{
			return true;
		}
	}
	return false;
}

void FDataflowConstructionScene::ResetConstructionScene()
{
	// Some objects, like the UMeshElementsVisualizer and Settings Objects
	// are not part of a tool, so they won't get ticked.This member holds
	// ticked objects that get rebuilt on Update
	PropertyObjectsToTick.Empty();

	ResetWireframeMeshElementsVisualizer();

	ResetDynamicMeshComponents();
}

void FDataflowConstructionScene::UpdateConstructionScene()
{
	ResetConstructionScene();

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	UpdateDynamicMeshComponents();
	
	// Attach a wireframe renderer to the DynamicMeshComponents
	UpdateWireframeMeshElementsVisualizer();

	// Manage Selection and Tool Interaction
    if (DataflowModeManager.IsValid())
    {
    	USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
    	SelectedComponents->DeselectAll();
    	for (const TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent : DynamicMeshComponents)
    	{
    		SelectedComponents->Select(DynamicMeshComponent);
    		DynamicMeshComponent->PushSelectionToProxy();
    	}
    }
	DataflowContent->SetIsDirty(false);
}

FDataflowSimulationScene::FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, TObjectPtr<UDataflowBaseContent> InEditorContent) 
	: FDataflowPreviewScene(ConstructionValues,InEditorContent)
{
	DataflowContent->RegisterWorldContent(this, RootSceneActor);

	TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
	RootSceneActor->GetComponents(PrimComponents);

	for(UPrimitiveComponent* PrimComponent : PrimComponents)
	{
		PrimComponent->SelectionOverrideDelegate =
			UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
	}
}

FDataflowSimulationScene::~FDataflowSimulationScene()
{
	TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
	RootSceneActor->GetComponents(PrimComponents);

	for(UPrimitiveComponent* PrimComponent : PrimComponents)
	{
		PrimComponent->SelectionOverrideDelegate.Unbind();
	}
	
	DataflowContent->UnregisterWorldContent(this);
}

void FDataflowSimulationScene::TickDataflowScene(const float DeltaSeconds)
{
	GetWorld()->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE

