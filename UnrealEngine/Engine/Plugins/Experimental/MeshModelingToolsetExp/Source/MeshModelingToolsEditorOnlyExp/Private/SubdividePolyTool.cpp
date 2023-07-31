// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubdividePolyTool.h"
#include "ToolBuilderUtil.h"
#include "InteractiveToolManager.h"
#include "GroupTopology.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "ToolSetupUtil.h"
#include "Util/ColorConstants.h"
#include "DynamicMesh/MeshNormals.h"
#include "Components/DynamicMeshComponent.h"
#include "Drawing/PreviewGeometryActor.h"

#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ModelingToolTargetUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SubdividePolyTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "USubdividePolyTool"

class SubdivPostProcessor : public IRenderMeshPostProcessor
{
public:

	SubdivPostProcessor(int InSubdivisionLevel,
						ESubdivisionScheme InSubdivisionScheme,
						ESubdivisionOutputNormals InNormalComputationMethod,
						ESubdivisionOutputUVs InUVComputationMethod,
						bool bInNewPolyGroups) :
		SubdivisionLevel(InSubdivisionLevel),
		SubdivisionScheme(InSubdivisionScheme),
		NormalComputationMethod(InNormalComputationMethod),
		UVComputationMethod(InUVComputationMethod),
		bNewPolyGroups(bInNewPolyGroups)
	{}

	int SubdivisionLevel = 3;
	ESubdivisionScheme SubdivisionScheme = ESubdivisionScheme::CatmullClark;
	ESubdivisionOutputNormals NormalComputationMethod = ESubdivisionOutputNormals::Generated;
	ESubdivisionOutputUVs UVComputationMethod = ESubdivisionOutputUVs::Interpolated;
	bool bNewPolyGroups = false;

	void ProcessMesh(const FDynamicMesh3& Mesh, FDynamicMesh3& OutRenderMesh) final
	{
		if (Mesh.TriangleCount() == 0 || Mesh.VertexCount() == 0)
		{
			return;
		}

		constexpr bool bAutoCompute = true;
		FGroupTopology Topo(&Mesh, bAutoCompute);
		FSubdividePoly Subd(Topo, Mesh, SubdivisionLevel);
		Subd.SubdivisionScheme = SubdivisionScheme;
		Subd.NormalComputationMethod = NormalComputationMethod;
		Subd.UVComputationMethod = UVComputationMethod;
		Subd.bNewPolyGroups = bNewPolyGroups;

		ensure(Subd.ComputeTopologySubdivision());

		ensure(Subd.ComputeSubdividedMesh(OutRenderMesh));
	}
};


// Tool builder

USingleSelectionMeshEditingTool* USubdividePolyToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<USubdividePolyTool>(SceneState.ToolManager);
}


bool USubdividePolyTool::CheckGroupTopology(FText& Message)
{
	Message = FText();

	FGroupTopology Topo(OriginalMesh.Get(), true);
	FSubdividePoly TempSubD(Topo, *OriginalMesh, 1);
	FSubdividePoly::ETopologyCheckResult CheckResult = TempSubD.ValidateTopology();

	if (CheckResult == FSubdividePoly::ETopologyCheckResult::NoGroups)
	{
		Message = LOCTEXT("NoGroupsWarning",
						  "This object has no PolyGroups.\nUse the PolyGroups or Select Tool to assign PolyGroups.\nTool will be limited to Loop subdivision scheme.");
		return false;
	}

	if (CheckResult == FSubdividePoly::ETopologyCheckResult::InsufficientGroups)
	{
		Message = LOCTEXT("SingleGroupsWarning",
						  "This object has only one PolyGroup.\nUse the PolyGroups or Select Tool to assign PolyGroups.\nTool will be limited to Loop subdivision scheme.");
		return false;
	}

	if (CheckResult == FSubdividePoly::ETopologyCheckResult::UnboundedPolygroup)
	{
		Message = LOCTEXT("NoGroupBoundaryWarning",
			"Found a PolyGroup with no boundaries.\nUse the PolyGroups or Select Tool to assign PolyGroups.\nTool will be limited to Loop subdivision scheme.");
		return false;
	}

	if (CheckResult == FSubdividePoly::ETopologyCheckResult::MultiBoundaryPolygroup)
	{
		Message = LOCTEXT("MultipleGroupBoundaryWarning",
			"Found a PolyGroup with multiple boundaries, which is not supported.\nUse the PolyGroups or Select Tool to assign PolyGroups.\nTool will be limited to Loop subdivision scheme.");
		return false;
	}

	if (CheckResult == FSubdividePoly::ETopologyCheckResult::DegeneratePolygroup)
	{
		Message = LOCTEXT("DegenerateGroupPolygon",
			"One PolyGroup has fewer than three boundary edges.\nUse the PolyGroups or Select Tool to assign/fix PolyGroups.\nTool will be limited to Loop subdivision scheme.");
		return false;
	}

	return true;
}


void USubdividePolyTool::CapSubdivisionLevel(ESubdivisionScheme Scheme, int DesiredLevel)
{
	// Stolen from UDisplaceMeshTool::ValidateSubdivisions
	constexpr int MaxFaces = 3000000;

	int NumOriginalFaces = MaxFaces;
	if (Scheme == ESubdivisionScheme::Loop)
	{
		NumOriginalFaces = OriginalMesh->TriangleCount();
	}
	else
	{
		constexpr bool bAutoCompute = true;
		FGroupTopology Topo(OriginalMesh.Get(), bAutoCompute);
		NumOriginalFaces = Topo.Groups.Num();
	}
	int MaxLevel = (int)floor(log2(MaxFaces / (NumOriginalFaces+1)) / 2.0);

	FText WarningText;
	if (DesiredLevel > MaxLevel)
	{
		WarningText = FText::Format(LOCTEXT("SubdivisionLevelTooHigh", "Subdivision level clamped: desired subdivision level ({0}) exceeds maximum level ({1}) for a mesh with this number of faces."),
										  FText::AsNumber(DesiredLevel),
										  FText::AsNumber(MaxLevel));
		

		Properties->SubdivisionLevel = MaxLevel;
		Properties->SilentUpdateWatched();		// Don't trigger this function again due to setting SubdivisionLevel above
	}
	
	if (!PersistentErrorMessage.IsEmpty())
	{
		FText Delimiter = FText::FromString("\n");
		WarningText = WarningText.IsEmpty() ?  PersistentErrorMessage : FText::Join(Delimiter, PersistentErrorMessage, WarningText);
	}
	// if WarningText is empty this will clear possible lingering warning message 
	GetToolManager()->DisplayMessage(WarningText, EToolMessageLevel::UserWarning);
}


void USubdividePolyTool::Setup()
{
	UInteractiveTool::Setup();
	SetToolDisplayName(LOCTEXT("ToolName", "Subdivide"));

	if (!Target)
	{
		return;
	}

	GetToolManager()->DisplayMessage(LOCTEXT("SubdividePolyToolMessage", "Set the subdivision level and hit Accept to create a new subdivided mesh"), EToolMessageLevel::UserNotification);

	bool bWantVertexNormals = false;
	OriginalMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(bWantVertexNormals, false, false, false);
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(UE::ToolTarget::GetMeshDescription(Target), *OriginalMesh);

	const bool bCatmullClarkOK = CheckGroupTopology(PersistentErrorMessage);

	if (!bCatmullClarkOK)
	{
		GetToolManager()->DisplayMessage(PersistentErrorMessage, EToolMessageLevel::UserWarning);
	}
	
	Properties = NewObject<USubdividePolyToolProperties>(this, TEXT("Subdivide Mesh Tool Settings"));
	Properties->RestoreProperties(this);

	Properties->bCatmullClarkOK = bCatmullClarkOK;
	if (!bCatmullClarkOK)
	{
		Properties->SubdivisionScheme = ESubdivisionScheme::Loop;
	}

	AddToolPropertySource(Properties);
	SetToolPropertySourceEnabled(Properties, true);

	IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	PreviewMesh = NewObject<UPreviewMesh>(this);
	if (PreviewMesh == nullptr)
	{
		return;
	}
	PreviewMesh->CreateInWorld(GetTargetWorld(), FTransform::Identity);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, nullptr);

	PreviewMesh->SetTransform(TargetComponent->GetWorldTransform());
	PreviewMesh->UpdatePreview(OriginalMesh.Get());

	UDynamicMeshComponent* PreviewDynamicMeshComponent = (UDynamicMeshComponent*)PreviewMesh->GetRootComponent();
	if (PreviewDynamicMeshComponent == nullptr)
	{
		return;
	}

	if (!ensure(Properties->SubdivisionLevel >= 1)) // Should be enforced by UPROPERTY meta tags
	{
		Properties->SubdivisionLevel = 1;
	}
	
	CapSubdivisionLevel(Properties->SubdivisionScheme, Properties->SubdivisionLevel);
	PreviewDynamicMeshComponent->SetRenderMeshPostProcessor(MakeUnique<SubdivPostProcessor>(Properties->SubdivisionLevel,
																							Properties->SubdivisionScheme,
																							Properties->NormalComputationMethod,
																							Properties->UVComputationMethod,
																							Properties->bNewPolyGroups));

	// Use the input mesh's material on the preview
	FComponentMaterialSet MaterialSet;
	Cast<IMaterialProvider>(Target)->GetMaterialSet(MaterialSet);
	for (int k = 0; k < MaterialSet.Materials.Num(); ++k)
	{
		PreviewMesh->SetMaterial(k, MaterialSet.Materials[k]);
	}

	// configure secondary render material
	UMaterialInterface* SelectionMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor(0.8f, 0.75f, 0.0f), GetToolManager());
	if (SelectionMaterial != nullptr)
	{
		PreviewMesh->SetSecondaryRenderMaterial(SelectionMaterial);
	}

	// dynamic mesh configuration settings
	auto RebuildMeshPostProcessor = [this]()
	{
		UDynamicMeshComponent* PreviewDynamicMeshComponent = (UDynamicMeshComponent*)PreviewMesh->GetRootComponent();
		PreviewDynamicMeshComponent->SetRenderMeshPostProcessor(MakeUnique<SubdivPostProcessor>(Properties->SubdivisionLevel,
																								Properties->SubdivisionScheme,
																								Properties->NormalComputationMethod,
																								Properties->UVComputationMethod,
																								Properties->bNewPolyGroups));
		PreviewDynamicMeshComponent->NotifyMeshUpdated();
	};

	// Watch for property changes
	Properties->WatchProperty(Properties->SubdivisionLevel, [this, RebuildMeshPostProcessor](int NewSubdLevel)
	{
		CapSubdivisionLevel(Properties->SubdivisionScheme, NewSubdLevel);
		RebuildMeshPostProcessor();
	});

	Properties->WatchProperty(Properties->SubdivisionScheme, [this, RebuildMeshPostProcessor](ESubdivisionScheme NewScheme)
	{
		CapSubdivisionLevel(NewScheme, Properties->SubdivisionLevel);
		RebuildMeshPostProcessor();
		bPreviewGeometryNeedsUpdate = true;		// Switch from rendering poly cage to all triangle edges
	});

	Properties->WatchProperty(Properties->NormalComputationMethod, [this, RebuildMeshPostProcessor](ESubdivisionOutputNormals)
	{
		RebuildMeshPostProcessor();
	});
	Properties->WatchProperty(Properties->UVComputationMethod, [this, RebuildMeshPostProcessor](ESubdivisionOutputUVs)
	{
		RebuildMeshPostProcessor();
	});
	Properties->WatchProperty(Properties->bNewPolyGroups, [this, RebuildMeshPostProcessor](bool)
	{
		RebuildMeshPostProcessor();
	});


	auto RenderGroupsChanged = [this](bool bNewRenderGroups)
	{
		if (bNewRenderGroups)
		{
			PreviewMesh->SetOverrideRenderMaterial(ToolSetupUtil::GetSelectionMaterial(GetToolManager()));
			PreviewMesh->SetTriangleColorFunction([](const FDynamicMesh3* Mesh, int TriangleID)
			{
				return LinearColors::SelectFColor(Mesh->GetTriangleGroup(TriangleID));
			}, UPreviewMesh::ERenderUpdateMode::FullUpdate);
		}
		else
		{
			PreviewMesh->SetOverrideRenderMaterial(nullptr);
			PreviewMesh->SetTriangleColorFunction(nullptr, UPreviewMesh::ERenderUpdateMode::FullUpdate);
		}
	};

	Properties->WatchProperty(Properties->bRenderGroups, RenderGroupsChanged);

	// Render with polygroup colors
	RenderGroupsChanged(Properties->bRenderGroups);

	Properties->WatchProperty(Properties->bRenderCage, [this](bool bNewRenderCage)
	{
		bPreviewGeometryNeedsUpdate = true;
	});

	PreviewGeometry = NewObject<UPreviewGeometry>(this);
	PreviewGeometry->CreateInWorld(TargetComponent->GetOwnerActor()->GetWorld(), TargetComponent->GetWorldTransform());
	CreateOrUpdatePreviewGeometry();

	// regenerate preview geo if mesh changes due to undo/redo/etc
	PreviewDynamicMeshComponent->OnMeshChanged.AddLambda([this]() { bPreviewGeometryNeedsUpdate = true; });

	TargetComponent->SetOwnerVisibility(false);
	PreviewMesh->SetVisible(true);
}

void USubdividePolyTool::CreateOrUpdatePreviewGeometry()
{
	if (!Properties->bRenderCage)
	{
		PreviewGeometry->RemoveLineSet(TEXT("TopologyEdges"));
		PreviewGeometry->RemoveLineSet(TEXT("AllEdges"));
		return;
	}

	if (Properties->SubdivisionScheme == ESubdivisionScheme::Loop)
	{
		int NumEdges = OriginalMesh->EdgeCount();

		PreviewGeometry->RemoveLineSet(TEXT("TopologyEdges"));

		PreviewGeometry->CreateOrUpdateLineSet(TEXT("AllEdges"),
											   NumEdges,
											   [this](int32 Index, TArray<FRenderableLine>& LinesOut)
		{
			FIndex2i EdgeVertices = OriginalMesh->GetEdgeV(Index);

			if (EdgeVertices[0] == FDynamicMesh3::InvalidID || EdgeVertices[1] == FDynamicMesh3::InvalidID)
			{
				return;
			}
			FVector A = (FVector)OriginalMesh->GetVertex(EdgeVertices[0]);
			FVector B = (FVector)OriginalMesh->GetVertex(EdgeVertices[1]);
			const float TopologyLineThickness = 4.0f;
			const FColor TopologyLineColor(255, 0, 0);
			LinesOut.Add(FRenderableLine(A, B, TopologyLineColor, TopologyLineThickness));
		});
	}
	else
	{
		FGroupTopology Topology(OriginalMesh.Get(), true);
		int NumEdges = Topology.Edges.Num();

		PreviewGeometry->RemoveLineSet(TEXT("AllEdges"));

		PreviewGeometry->CreateOrUpdateLineSet(TEXT("TopologyEdges"),
											   NumEdges,
											   [&Topology, this](int32 Index, TArray<FRenderableLine>& LinesOut)
		{
			const FGroupTopology::FGroupEdge& Edge = Topology.Edges[Index];
			FIndex2i EdgeCorners = Edge.EndpointCorners;

			if (EdgeCorners[0] == FDynamicMesh3::InvalidID || EdgeCorners[1] == FDynamicMesh3::InvalidID)
			{
				return;
			}

			FIndex2i EdgeVertices{ Topology.Corners[EdgeCorners[0]].VertexID,
									Topology.Corners[EdgeCorners[1]].VertexID };
			FVector A = (FVector)OriginalMesh->GetVertex(EdgeVertices[0]);
			FVector B = (FVector)OriginalMesh->GetVertex(EdgeVertices[1]);

			const float TopologyLineThickness = 4.0f;
			const FColor TopologyLineColor(255, 0, 0);
			LinesOut.Add(FRenderableLine(A, B, TopologyLineColor, TopologyLineThickness));
		});
	}
}


void USubdividePolyTool::OnShutdown(EToolShutdownType ShutdownType)
{
	if (Properties)
	{
		Properties->SaveProperties(this);
	}

	if (PreviewGeometry)
	{
		PreviewGeometry->Disconnect();
	}
	
	Cast<IPrimitiveComponentBackedTarget>(Target)->SetOwnerVisibility(true);

	if (PreviewMesh)
	{
		if (ShutdownType == EToolShutdownType::Accept)
		{
			GetToolManager()->BeginUndoTransaction(LOCTEXT("USubdividePolyTool", "Subdivide Mesh"));

			UDynamicMeshComponent* PreviewDynamicMeshComponent = (UDynamicMeshComponent*)PreviewMesh->GetRootComponent();
			FDynamicMesh3* DynamicMeshResult = PreviewDynamicMeshComponent->GetRenderMesh();

			UE::ToolTarget::CommitMeshDescriptionUpdateViaDynamicMesh(Target, *DynamicMeshResult, true);

			GetToolManager()->EndUndoTransaction();
		}
		PreviewMesh->Disconnect();
		PreviewMesh = nullptr;
	}
}

bool USubdividePolyTool::CanAccept() const
{
	return PreviewMesh != nullptr;
}


void USubdividePolyTool::OnTick(float DeltaTime)
{
	if (bPreviewGeometryNeedsUpdate)
	{
		CreateOrUpdatePreviewGeometry();
		bPreviewGeometryNeedsUpdate = false;
	}
}


#undef LOCTEXT_NAMESPACE

