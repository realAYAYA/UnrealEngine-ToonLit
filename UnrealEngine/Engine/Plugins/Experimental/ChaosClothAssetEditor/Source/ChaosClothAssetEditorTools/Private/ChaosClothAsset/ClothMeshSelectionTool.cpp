// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothMeshSelectionTool.h"
#include "ChaosClothAsset/ClothEditorContextObject.h"
#include "ChaosClothAsset/ClothPatternVertexType.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "InteractiveToolManager.h"
#include "PreviewMesh.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "ModelingToolTargetUtil.h"
#include "GroupTopology.h"
#include "ToolSetupUtil.h"
#include "Selections/GeometrySelection.h"
#include "ContextObjectStore.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowEdNode.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothMeshSelectionTool)

#define LOCTEXT_NAMESPACE "ClothMeshSelectionTool"


// ------------------- Builder -------------------

void UClothMeshSelectionToolBuilder::GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Render);
}

const FToolTargetTypeRequirements& UClothMeshSelectionToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		UPrimitiveComponentBackedTarget::StaticClass());
	return TypeRequirements;
}

bool UClothMeshSelectionToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1);
}

UInteractiveTool* UClothMeshSelectionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClothMeshSelectionTool* const NewTool = NewObject<UClothMeshSelectionTool>(SceneState.ToolManager);

	UToolTarget* const Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);
	NewTool->SetWorld(SceneState.World);

	if (UClothEditorContextObject* const ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		NewTool->SetClothEditorContextObject(ContextObject);
	}

	return NewTool;
}

// ------------------- Properties -------------------

void UClothMeshSelectionToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UClothMeshSelectionToolProperties, Name))
	{
		UE::Chaos::ClothAsset::FWeightMapTools::MakeWeightMapName(Name);
	}
}


// ------------------- Tool -------------------
	
void UClothMeshSelectionTool::Setup()
{
	using namespace UE::Geometry;

	//
	// Preview 
	//

	// Create the preview mesh
	PreviewMesh = NewObject<UPreviewMesh>(this, TEXT("PreviewMesh"));
	PreviewMesh->CreateInWorld(GetTargetWorld(), FTransform::Identity);

	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, Target);

	// We will use the preview mesh's spatial data structure
	PreviewMesh->bBuildSpatialDataStructure = true;

	// set materials
	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	PreviewMesh->SetMaterials(MaterialSet.Materials);

	// configure secondary render material for selected triangles
	UMaterialInterface* SelectionMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor::Yellow, GetToolManager());
	if (SelectionMaterial != nullptr)
	{
		PreviewMesh->SetSecondaryRenderMaterial(SelectionMaterial);
	}

	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	PreviewMesh->UpdatePreview(UE::ToolTarget::GetDynamicMeshCopy(Target));
	PreviewMesh->SetVisible(true);

	//
	// SelectionMechanic
	//

	SelectionMechanic = NewObject<UPolygonSelectionMechanic>(this);
	SelectionMechanic->bAddSelectionFilterPropertiesToParentTool = false;   // We'll do this ourselves later
	SelectionMechanic->Setup(this);
	SelectionMechanic->Properties->RestoreProperties(this);
	SelectionMechanic->Properties->bDisplayPolygroupReliantControls = false;	// this is for polygroup-specific selections like edge loops
	
	SelectionMechanic->Properties->bCanSelectVertices = true;
	SelectionMechanic->Properties->bCanSelectEdges = false;		// For now do not allow edge selection
	SelectionMechanic->Properties->bCanSelectFaces = true;
	
	SelectionMechanic->SetShowEdges(false);
	SelectionMechanic->SetShowSelectableCorners(false);

	SelectionMechanic->PolyEdgesRenderer.DepthBias = 0.01f;
	SelectionMechanic->PolyEdgesRenderer.LineThickness = 1.0f;
	SelectionMechanic->PolyEdgesRenderer.PointSize = 2.0f;

	SelectionMechanic->SelectionRenderer.DepthBias = 0.01f;
	SelectionMechanic->SelectionRenderer.LineThickness = 1.0f;
	SelectionMechanic->SelectionRenderer.PointSize = 2.0f;

	SelectionMechanic->OnSelectionChanged.AddWeakLambda(this, [this]()
	{
		bAnyChangeMade = true;
		PreviewMesh->FastNotifySecondaryTrianglesChanged();
	});

	SelectionMechanic->OnFaceSelectionPreviewChanged.AddWeakLambda(this, [this]()
	{
		PreviewMesh->FastNotifySecondaryTrianglesChanged();
	});


	// Enable only one selection mode at a time (this is different from other mesh modeling tools using the SelectionMechanic)

	SelectionMechanic->Properties->WatchProperty(SelectionMechanic->Properties->bSelectVertices, [this](bool bNewSelectVertices)
	{
		SelectionMechanic->Properties->bSelectFaces = !bNewSelectVertices;
	});

	SelectionMechanic->Properties->WatchProperty(SelectionMechanic->Properties->bSelectFaces, [this](bool bNewSelectFaces)
	{
		SelectionMechanic->Properties->bSelectVertices = !bNewSelectFaces;
	});

	// Set up Topology and SelectionMechanic using Preview's DynamicMesh

	PreviewMesh->ProcessMesh([this](const FDynamicMesh3& Mesh)
	{
		Topology = MakeUnique<FTriangleGroupTopology>(&Mesh, true);

		SelectionMechanic->Initialize(&Mesh,
			FTransform3d(),
			GetTargetWorld(),
			Topology.Get(),
			[this]() { return PreviewMesh->GetSpatial(); });
	});

	PreviewMesh->EnableSecondaryTriangleBuffers([this](const FDynamicMesh3* Mesh, int32 TriangleID)
	{
		return SelectionMechanic->GetActiveSelection().IsSelectedTriangle(Mesh, Topology.Get(), TriangleID);
	});

	// Hide input target mesh
	UE::ToolTarget::HideSourceObject(Target);

	// Initialize the Selection from the selected Dataflow node
	FString ExistingSelectionName;
	FGroupTopologySelection ExistingNodeSelection;
	GetSelectedNodeInfo(ExistingSelectionName, ExistingNodeSelection);

	constexpr bool bBroadcastChange = false;
	SelectionMechanic->SetSelection(ExistingNodeSelection, bBroadcastChange);

	if (ExistingNodeSelection.SelectedCornerIDs.Num() > 0)
	{
		check(ExistingNodeSelection.SelectedEdgeIDs.Num() == 0);
		check(ExistingNodeSelection.SelectedGroupIDs.Num() == 0);
		SelectionMechanic->Properties->bSelectVertices = true;
		SelectionMechanic->Properties->bSelectFaces = false;
	} 
	else
	{
		SelectionMechanic->Properties->bSelectVertices = false;
		SelectionMechanic->Properties->bSelectFaces = true;
	}

	// Setup non-manifold mapping if necessary
	if (ClothEditorContextObject)
	{
		if (const TSharedPtr<const FManagedArrayCollection> ClothCollection = ClothEditorContextObject->GetSelectedClothCollection().Pin())
		{
			PreviewMesh->ProcessMesh([this, ClothCollection](const FDynamicMesh3& Mesh)
			{
				using namespace UE::Chaos::ClothAsset;
				const FNonManifoldMappingSupport NonManifoldMapping(Mesh);

				bHasNonManifoldMapping = NonManifoldMapping.IsNonManifoldVertexInSource();

				if (bHasNonManifoldMapping)
				{
					const FCollectionClothConstFacade Cloth(ClothCollection.ToSharedRef());
					check(Cloth.IsValid());

					const TConstArrayView<int32> SimVertex3DLookup = Cloth.GetSimVertex3DLookup();

					DynamicMeshToSelection.SetNumUninitialized(Mesh.VertexCount());
					SelectionToDynamicMesh.Reset();
					SelectionToDynamicMesh.SetNum(Cloth.GetNumSimVertices3D());
					for (int32 DynamicMeshVert = 0; DynamicMeshVert < Mesh.VertexCount(); ++DynamicMeshVert)
					{
						DynamicMeshToSelection[DynamicMeshVert] = NonManifoldMapping.GetOriginalNonManifoldVertexID(DynamicMeshVert);
						SelectionToDynamicMesh[DynamicMeshToSelection[DynamicMeshVert]].Add(DynamicMeshVert);
					}
				}
			});
		}
	}


	//
	// Properties
	//

	ToolProperties = NewObject<UClothMeshSelectionToolProperties>();
	ToolProperties->WatchProperty(ToolProperties->bShowVertices, [this](bool bNewShowVertices)
	{
		SelectionMechanic->SetShowSelectableCorners(bNewShowVertices);
	});

	ToolProperties->WatchProperty(ToolProperties->bShowEdges, [this](bool bNewShowEdges)
	{
		SelectionMechanic->SetShowEdges(bNewShowEdges);
	});

	ToolProperties->Name = ExistingSelectionName;

	ToolProperties->WatchProperty(ToolProperties->Name, [this, OriginalName = ToolProperties->Name](const FString& NewName)
	{
		if (NewName != OriginalName)
		{
			bAnyChangeMade = true;
		}
	});

	AddToolPropertySource(ToolProperties);
	ToolProperties->RestoreProperties(this);

	AddToolPropertySource(SelectionMechanic->Properties);
}

void UClothMeshSelectionTool::OnShutdown(EToolShutdownType ShutdownType)
{
	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SelectionToolTransactionName", "Mesh Selection"));
		UpdateSelectedNode();
		GetToolManager()->EndUndoTransaction();
	}

	SelectionMechanic->Properties->SaveProperties(this);
	ToolProperties->SaveProperties(this);

	if (PreviewMesh != nullptr)
	{
		UE::ToolTarget::ShowSourceObject(Target);
		PreviewMesh->Disconnect();
		PreviewMesh = nullptr;
	}

	SelectionMechanic->Shutdown();

	Topology.Reset();
}

void UClothMeshSelectionTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	SelectionMechanic->Render(RenderAPI);
}

void UClothMeshSelectionTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	SelectionMechanic->DrawHUD(Canvas, RenderAPI);
}

bool UClothMeshSelectionTool::CanAccept() const
{
	return bAnyChangeMade;
}

void UClothMeshSelectionTool::SetClothEditorContextObject(TObjectPtr<UClothEditorContextObject> InClothEditorContextObject)
{
	ClothEditorContextObject = InClothEditorContextObject;
}

bool UClothMeshSelectionTool::GetSelectedNodeInfo(FString& OutSelectionName, UE::Geometry::FGroupTopologySelection& OutSelection)
{
	const FChaosClothAssetSelectionNode* const MeshSelectionNode = ClothEditorContextObject->GetSingleSelectedNodeOfType<FChaosClothAssetSelectionNode>();
	check(MeshSelectionNode);

	// We need to sanitize the incoming indices, as the user can manually set them to anything on the node

	auto AppendVerticesIfValid = [this]<typename T>(TSet<int32>& Dest, const T& Source)
	{
		PreviewMesh->ProcessMesh([this, &Dest, &Source](const UE::Geometry::FDynamicMesh3& Mesh)
		{
			for (const int32& VertexIndex : Source)
			{
				if (Mesh.IsVertex(VertexIndex))
				{
					Dest.Add(VertexIndex);
				}
			}
		});
	};

	auto AppendFacesIfValid = [this](TSet<int32>& Dest, const TSet<int32>& Source)
	{
		PreviewMesh->ProcessMesh([this, &Dest, &Source](const UE::Geometry::FDynamicMesh3& Mesh)
		{
			for (const int32& FaceIndex : Source)
			{
				if (Mesh.IsTriangle(FaceIndex))
				{
					Dest.Add(FaceIndex);
				}
			}
		});
	};


	if (MeshSelectionNode->Type == EChaosClothAssetSelectionType::SimVertex2D || MeshSelectionNode->Type == EChaosClothAssetSelectionType::SimVertex3D || MeshSelectionNode->Type == EChaosClothAssetSelectionType::RenderVertex)
	{
		if (bHasNonManifoldMapping)
		{
			for (const int32 SelectionIndex : MeshSelectionNode->Indices)
			{
				AppendVerticesIfValid(OutSelection.SelectedCornerIDs, SelectionToDynamicMesh[SelectionIndex]);
			}
		}
		else
		{
			AppendVerticesIfValid(OutSelection.SelectedCornerIDs, MeshSelectionNode->Indices);
		}
	}
	else if (MeshSelectionNode->Type == EChaosClothAssetSelectionType::SimFace || MeshSelectionNode->Type == EChaosClothAssetSelectionType::RenderFace)
	{
		AppendFacesIfValid(OutSelection.SelectedGroupIDs, MeshSelectionNode->Indices);
	}

	OutSelectionName = MeshSelectionNode->Name;

	return true;
}


void UClothMeshSelectionTool::UpdateSelectedNode()
{
	const UE::Geometry::FGroupTopologySelection& Selection = SelectionMechanic->GetActiveSelection();
	const UE::Chaos::ClothAsset::EClothPatternVertexType ViewMode = ClothEditorContextObject->GetConstructionViewMode();

	TSet<int32> Indices;
	EChaosClothAssetSelectionType Type = EChaosClothAssetSelectionType::SimVertex2D;

	if (SelectionMechanic->Properties->bSelectVertices)
	{
		check(!SelectionMechanic->Properties->bSelectEdges);
		check(!SelectionMechanic->Properties->bSelectFaces);

		Indices = Selection.SelectedCornerIDs;

		switch(ViewMode)
		{
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D:
			Type = EChaosClothAssetSelectionType::SimVertex2D;
			break;
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D:
			Type = EChaosClothAssetSelectionType::SimVertex3D;
			break;
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Render:
			Type = EChaosClothAssetSelectionType::RenderVertex;
			break;
		}

	}
	else
	{
		Indices = Selection.SelectedGroupIDs;

		switch (ViewMode)
		{
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D:
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D:
			Type = EChaosClothAssetSelectionType::SimFace;
			break;
		case UE::Chaos::ClothAsset::EClothPatternVertexType::Render:
			Type = EChaosClothAssetSelectionType::RenderFace;
			break;
		}
	}

	FChaosClothAssetSelectionNode* const MeshSelectionNode = ClothEditorContextObject->GetSingleSelectedNodeOfType<FChaosClothAssetSelectionNode>();
	check(MeshSelectionNode);

	MeshSelectionNode->Name = ToolProperties->Name;
	MeshSelectionNode->Type = Type;

	if (SelectionMechanic->Properties->bSelectVertices && bHasNonManifoldMapping)
	{
		MeshSelectionNode->Indices.Reset();
		for (const int32 DynamicMeshIdx : Indices)
		{
			const int32 MappedSelectionIndex = DynamicMeshToSelection[DynamicMeshIdx];
			MeshSelectionNode->Indices.Add(MappedSelectionIndex);
		}
	}
	else
	{
		MeshSelectionNode->Indices = MoveTemp(Indices);
	}

	MeshSelectionNode->Invalidate();
}


#undef LOCTEXT_NAMESPACE
