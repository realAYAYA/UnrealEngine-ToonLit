// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshInspectorTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"

#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Properties/MeshStatisticsProperties.h"
#include "Properties/MeshAnalysisProperties.h"

#include "Drawing/LineSetComponent.h"
#include "ToolDataVisualizer.h"

#include "SceneManagement.h" // for FPrimitiveDrawInterface

#include "ToolSetupUtil.h"
#include "AssetUtils/MeshDescriptionUtil.h"
#include "ModelingToolTargetUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshInspectorTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UMeshInspectorTool"

/*
 * ToolBuilder
 */


USingleSelectionMeshEditingTool* UMeshInspectorToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UMeshInspectorTool>(SceneState.ToolManager);
}

/*
 * Tool
 */
UMeshInspectorTool::UMeshInspectorTool()
{
}

void UMeshInspectorTool::Setup()
{
	UInteractiveTool::Setup();

	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->bBuildSpatialDataStructure = false;
	PreviewMesh->CreateInWorld(GetTargetWorld(), FTransform::Identity);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, Target);
	PreviewMesh->SetTransform((FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target));

	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	PreviewMesh->SetMaterials(MaterialSet.Materials);
	DefaultMaterial = PreviewMesh->GetMaterial(0);

	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::ExternallyProvided);
	FDynamicMesh3 InputMeshWithTangents = UE::ToolTarget::GetDynamicMeshCopy(Target, true);
	PreviewMesh->ReplaceMesh(MoveTemp(InputMeshWithTangents));

	DrawnLineSet = NewObject<ULineSetComponent>(PreviewMesh->GetRootComponent(), "MeshInspectorToolLineSet");

	DrawnLineSet->SetupAttachment(PreviewMesh->GetRootComponent());

	DrawnLineSet->SetLineMaterial(ToolSetupUtil::GetDefaultLineComponentMaterial(GetToolManager()));
	DrawnLineSet->RegisterComponent();

	Precompute();

	UE::ToolTarget::HideSourceObject(Target);

	// initialize our properties
	Settings = NewObject<UMeshInspectorProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	MaterialSettings = NewObject<UExistingMeshMaterialProperties>(this);
	const FDynamicMesh3* TargetMesh = PreviewMesh->GetPreviewDynamicMesh();
	TArray<FString> UVChannelNamesList;
	for (int32 k = 0; k < TargetMesh->Attributes()->NumUVLayers(); ++k)
	{
		UVChannelNamesList.Add(FString::Printf(TEXT("UV %d"), k));
	}
	MaterialSettings->UpdateUVChannels(0, UVChannelNamesList);
	MaterialSettings->RestoreProperties(this);
	AddToolPropertySource(MaterialSettings);

	PreviewMesh->EnableWireframe(Settings->bWireframe);

	UMeshStatisticsProperties* Statistics = NewObject<UMeshStatisticsProperties>(this);
	Statistics->Update(*PreviewMesh->GetPreviewDynamicMesh());
	AddToolPropertySource(Statistics);

	UMeshAnalysisProperties* MeshAnalysis = NewObject<UMeshAnalysisProperties>(this);
	MeshAnalysis->Update(*PreviewMesh->GetPreviewDynamicMesh(), (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target));
	AddToolPropertySource(MeshAnalysis);

	UpdateVisualization();

	SetToolDisplayName(LOCTEXT("ToolName", "Mesh Inspector"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Visualize various geometric properties and attributes of the Mesh"),
		EToolMessageLevel::UserNotification);
}


void UMeshInspectorTool::OnShutdown(EToolShutdownType ShutdownType)
{
	UE::ToolTarget::ShowSourceObject(Target);

	if (PreviewMesh != nullptr)
	{
		PreviewMesh->SetVisible(false);
		PreviewMesh->Disconnect();
		PreviewMesh = nullptr;
	}

	Settings->SaveProperties(this);
	MaterialSettings->SaveProperties(this);
}


void UMeshInspectorTool::Precompute()
{
	BoundaryEdges.Reset();
	BoundaryBowties.Reset();
	UVSeamEdges.Reset();
	UVBowties.Reset();
	NormalSeamEdges.Reset();
	GroupBoundaryEdges.Reset();
	MissingUVTriangleEdges.Reset();

	const FDynamicMesh3* TargetMesh = PreviewMesh->GetPreviewDynamicMesh();
	const FDynamicMeshUVOverlay* UVOverlay =
		TargetMesh->HasAttributes() ? TargetMesh->Attributes()->PrimaryUV() : nullptr;
	const FDynamicMeshNormalOverlay* NormalOverlay =
		TargetMesh->HasAttributes() ? TargetMesh->Attributes()->PrimaryNormals() : nullptr;

	for (int eid : TargetMesh->EdgeIndicesItr())
	{
		if (TargetMesh->IsBoundaryEdge(eid))
		{
			BoundaryEdges.Add(eid);
		}
		if (UVOverlay != nullptr && UVOverlay->IsSeamEdge(eid))
		{
			UVSeamEdges.Add(eid);
		}
		if (NormalOverlay != nullptr && NormalOverlay->IsSeamEdge(eid))
		{
			NormalSeamEdges.Add(eid);
		}
		if (TargetMesh->IsGroupBoundaryEdge(eid))
		{
			GroupBoundaryEdges.Add(eid);
		}
	}

	for (int vid : TargetMesh->VertexIndicesItr())
	{
		if (TargetMesh->IsBowtieVertex(vid))
		{
			BoundaryBowties.Add(vid);
		}

		if (UVOverlay != nullptr && UVOverlay->IsBowtieInOverlay(vid))
		{
			UVBowties.Add(vid);
		}
	}

	for (int tid : TargetMesh->TriangleIndicesItr())
	{
		if (UVOverlay != nullptr && !UVOverlay->IsSetTriangle(tid))
		{
			const FIndex3i Edges = TargetMesh->GetTriEdges(tid);
			MissingUVTriangleEdges.Add(Edges[0]);
			MissingUVTriangleEdges.Add(Edges[1]);
			MissingUVTriangleEdges.Add(Edges[2]);
		}

	}
}


void UMeshInspectorTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	const FDynamicMesh3* TargetMesh = PreviewMesh->GetPreviewDynamicMesh();
	FTransform Transform = PreviewMesh->GetTransform();

	if (BoundaryBowties.Num() > 0 && Settings->bBowtieVertices)
	{
		FToolDataVisualizer BowtieRenderer;
		BowtieRenderer.PointColor = FColor(240, 15, 15);
		BowtieRenderer.PointSize = 25.0;
		BowtieRenderer.BeginFrame(RenderAPI);
		BowtieRenderer.SetTransform(Transform);
		for (int32 vid : BoundaryBowties)
		{
			BowtieRenderer.DrawPoint(TargetMesh->GetVertex(vid));
		}
		BowtieRenderer.EndFrame();
	}

	if (UVBowties.Num() > 0 && Settings->bUVBowties)
	{
		FToolDataVisualizer BowtieRenderer;
		BowtieRenderer.PointColor = FColor(15, 240, 15);
		BowtieRenderer.PointSize = 12.0;
		BowtieRenderer.BeginFrame(RenderAPI);
		BowtieRenderer.SetTransform(Transform);
		for (int32 vid : UVBowties)
		{
			BowtieRenderer.DrawPoint(TargetMesh->GetVertex(vid));
		}
		BowtieRenderer.EndFrame();
	}
}


void UMeshInspectorTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	GetToolManager()->PostInvalidation();
	UpdateVisualization();
}

void UMeshInspectorTool::UpdateVisualization()
{
	PreviewMesh->EnableWireframe(Settings->bWireframe);

	MaterialSettings->UpdateMaterials();
	UMaterialInterface* OverrideMaterial = MaterialSettings->GetActiveOverrideMaterial();
	if (OverrideMaterial == nullptr)
	{
		PreviewMesh->ClearOverrideRenderMaterial();
	}
	else
	{
		PreviewMesh->SetOverrideRenderMaterial(OverrideMaterial);
	}

	FColor BoundaryEdgeColor(240, 15, 15);
	float BoundaryEdgeThickness = LineWidthMultiplier * 4.0;
	FColor UVSeamColor(15, 240, 15);
	float UVSeamThickness = LineWidthMultiplier * 2.0;
	const FColor MissingUVColor(240, 15, 15);
	const float MissingUVThickness = LineWidthMultiplier * 2.0;
	FColor NormalSeamColor(15, 240, 240);
	float NormalSeamThickness = LineWidthMultiplier * 2.0;
	FColor PolygonBorderColor(240, 15, 240);
	float PolygonBorderThickness = LineWidthMultiplier * 2.0;
	FColor NormalColor(15, 15, 240);
	float NormalThickness = LineWidthMultiplier * 2.0f;
	FColor TangentColor(240, 15, 15);
	FColor BinormalColor(15, 240, 15);
	float TangentThickness = LineWidthMultiplier * 2.0f;

	float BoundaryEdgeDepthBias = 0.2f;
	float UVSeamDepthBias = 0.3f;
	const float MissingUVDepthBias = 0.3f;
	float NormalSeamDepthBias = 0.3f;
	float PolygonBorderDepthBias = 0.2f;
	float NormalDepthBias = 0.0f;
	float TangentDepthBias = 0.35f;

	// Used to scale normals and tangents appropriately
	FVector ComponentScale = DrawnLineSet->GetComponentTransform().GetScale3D();
	FVector3f InverseScale(1 / ComponentScale.X, 1 / ComponentScale.Y, 1 / ComponentScale.Z);

	const FDynamicMesh3* TargetMesh = PreviewMesh->GetPreviewDynamicMesh();
	FVector3d A, B;

	DrawnLineSet->Clear();
	if (Settings->bBoundaryEdges)
	{
		for (int eid : BoundaryEdges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			DrawnLineSet->AddLine((FVector)A, (FVector)B, BoundaryEdgeColor, BoundaryEdgeThickness, BoundaryEdgeDepthBias);
		}
	}

	if (Settings->bUVSeams)
	{
		for (int eid : UVSeamEdges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			DrawnLineSet->AddLine((FVector)A, (FVector)B, UVSeamColor, UVSeamThickness, UVSeamDepthBias);
		}
	}

	if (Settings->bMissingUVs)
	{
		for (int eid : MissingUVTriangleEdges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			DrawnLineSet->AddLine((FVector)A, (FVector)B, MissingUVColor, MissingUVThickness, UVSeamDepthBias);
		}
	}

	if (Settings->bNormalSeams)
	{
		for (int eid : NormalSeamEdges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			DrawnLineSet->AddLine((FVector)A, (FVector)B, NormalSeamColor, NormalSeamThickness, NormalSeamDepthBias);
		}
	}

	if (Settings->bPolygonBorders)
	{
		for (int eid : GroupBoundaryEdges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			DrawnLineSet->AddLine((FVector)A, (FVector)B, PolygonBorderColor, PolygonBorderThickness, PolygonBorderDepthBias);
		}
	}

	if (Settings->bNormalVectors && TargetMesh->HasAttributes() && TargetMesh->Attributes()->PrimaryNormals() != nullptr)
	{
		// Note that for normals and tangent vectors, we want to allow the origin of the vector to undergo the full
		// transform of the component, but the direction we travel along the vector has to be inversely scaled
		// to end up with the original length in the end (and to reflect the fact that Unreal does not consider
		// the scale transform when adjusting normals for an object).
		FVector3f NormalScaling = Settings->NormalLength * InverseScale;

		const FDynamicMeshNormalOverlay* NormalOverlay = TargetMesh->Attributes()->PrimaryNormals();
		FVector3d TriV[3];
		FVector3f TriN[3];
		for (int tid : TargetMesh->TriangleIndicesItr())
		{
			if ( NormalOverlay->IsSetTriangle(tid) )
			{
				TargetMesh->GetTriVertices(tid, TriV[0], TriV[1], TriV[2]);
				NormalOverlay->GetTriElements(tid, TriN[0], TriN[1], TriN[2]);
				for (int j = 0; j < 3; ++j)
				{
					DrawnLineSet->AddLine((FVector)TriV[j], (FVector)((FVector3f)TriV[j] + NormalScaling * TriN[j]),
						NormalColor, NormalThickness, NormalDepthBias);
				}
			}
		}
	}

	if (Settings->bTangentVectors && PreviewMesh->GetMesh()->Attributes()->HasTangentSpace() )
	{
		// See note in normals about scaling
		FVector3f TangentScaling = Settings->TangentLength * InverseScale;

		FDynamicMeshTangents Tangents(PreviewMesh->GetMesh());
		for (int TID : TargetMesh->TriangleIndicesItr())
		{
			FVector3d TriV[3];
			TargetMesh->GetTriVertices(TID, TriV[0], TriV[1], TriV[2]);
			for (int SubIdx = 0; SubIdx < 3; SubIdx++)
			{
				FVector3f Vert(TriV[SubIdx]);
				FVector3f Tangent, Bitangent;
				Tangents.GetTangentVectors(TID, SubIdx, Tangent, Bitangent);
				DrawnLineSet->AddLine((FVector)Vert, (FVector)(Vert + TangentScaling * Tangent),
					TangentColor, TangentThickness, TangentDepthBias);
				DrawnLineSet->AddLine((FVector)Vert, (FVector)(Vert + TangentScaling * Bitangent),
					BinormalColor, TangentThickness, TangentDepthBias);
			}
		}
	}
}


bool UMeshInspectorTool::HasAccept() const
{
	return false;
}

bool UMeshInspectorTool::CanAccept() const
{
	return false;
}



void UMeshInspectorTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 1,
		TEXT("IncreaseLineWidth"), 
		LOCTEXT("IncreaseLineWidth", "Increase Line Width"),
		LOCTEXT("IncreaseLineWidthTooltip", "Increase line width of rendering"),
		EModifierKey::Shift, EKeys::Equals,
		[this]() { IncreaseLineWidthAction(); });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 2,
		TEXT("DecreaseLineWidth"), 
		LOCTEXT("DecreaseLineWidth", "Decrease Line Width"),
		LOCTEXT("DecreaseLineWidthTooltip", "Decrease line width of rendering"),
		EModifierKey::None, EKeys::Equals,
		[this]() { DecreaseLineWidthAction(); });
}



void UMeshInspectorTool::IncreaseLineWidthAction()
{
	LineWidthMultiplier = LineWidthMultiplier * 1.25f;
	GetToolManager()->PostInvalidation();
}

void UMeshInspectorTool::DecreaseLineWidthAction()
{
	LineWidthMultiplier = LineWidthMultiplier * (1.0f / 1.25f);
	GetToolManager()->PostInvalidation();
}



#undef LOCTEXT_NAMESPACE

