// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvertToPolygonsTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "Drawing/PreviewGeometryActor.h"
#include "PreviewMesh.h"

#include "MeshOpPreviewHelpers.h"
#include "ModelingOperators.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshEditor.h"
#include "MeshRegionBoundaryLoops.h"
#include "Util/ColorConstants.h"
#include "Polygroups/PolygroupUtil.h"
#include "Util/ColorConstants.h"

#include "Polygroups/PolygroupsGenerator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConvertToPolygonsTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UConvertToPolygonsTool"

/*
 * ToolBuilder
 */

USingleSelectionMeshEditingTool* UConvertToPolygonsToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UConvertToPolygonsTool>(SceneState.ToolManager);
}

class FConvertToPolygonsOp : public  FDynamicMeshOperator
{
public:
	// parameters set by the tool
	EConvertToPolygonsMode ConversionMode = EConvertToPolygonsMode::FaceNormalDeviation;
	double AngleTolerance = 0.1f;
	int32 NumPoints = 10;
	bool bSubdivideExisting = false;
	FPolygroupsGenerator::EWeightingType WeightingType = FPolygroupsGenerator::EWeightingType::None;
	FVector3d WeightingCoeffs = FVector3d::One();

	bool bRespectUVSeams = false;
	bool bRespectHardNormals = false;

	double QuadMetricClamp = 1.0;
	double QuadAdjacencyWeight = 1.0;
	int QuadSearchRounds = 1;

	int32 MinGroupSize = 2;

	bool bCalculateNormals = false;

	// input mesh
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	// input polygroups, if available
	TSharedPtr<FPolygroupSet, ESPMode::ThreadSafe> SourcePolyGroups;

	// result
	FPolygroupsGenerator Generator;

	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		if ((Progress && Progress->Cancelled()) || !OriginalMesh)
		{
			return;
		}

		ResultMesh->Copy(*OriginalMesh, true, true, true, true);
	
		if (Progress && Progress->Cancelled())
		{
			return;
		}

		if (ConversionMode == EConvertToPolygonsMode::CopyFromLayer)
		{
			if (!ensure(SourcePolyGroups.IsValid())) return;
			for (int32 tid : ResultMesh->TriangleIndicesItr())
			{
				ResultMesh->SetTriangleGroup(tid, SourcePolyGroups->GetTriangleGroup(tid));
			}
			return;
		}

		Generator = FPolygroupsGenerator(ResultMesh.Get());
		Generator.MinGroupSize = MinGroupSize;

		switch (ConversionMode)
		{
		case EConvertToPolygonsMode::FromUVIslands:
		{
			Generator.FindPolygroupsFromUVIslands();
			break;
		}
		case EConvertToPolygonsMode::FromNormalSeams:
		{
			Generator.FindPolygroupsFromHardNormalSeams();
			break;
		}
		case EConvertToPolygonsMode::FromConnectedTris:
		{
			Generator.FindPolygroupsFromConnectedTris();
			break;
		}
		case EConvertToPolygonsMode::FaceNormalDeviation:
		{
			double DotTolerance = 1.0 - FMathd::Cos(AngleTolerance * FMathd::DegToRad);
			Generator.FindPolygroupsFromFaceNormals(
				DotTolerance,
				bRespectUVSeams, 
				bRespectHardNormals );
			break;
		}
		case EConvertToPolygonsMode::FindPolygons:
		{
			Generator.FindSourceMeshPolygonPolygroups(
				bRespectUVSeams, 
				bRespectHardNormals, 
				QuadMetricClamp, 
				QuadAdjacencyWeight,
				FMath::Clamp(QuadSearchRounds, 1, 100)
			);
			break;
		}
		case EConvertToPolygonsMode::FromFurthestPointSampling:
		{
			if (bSubdivideExisting)
			{
				FPolygroupSet InputGroups(OriginalMesh.Get());
				Generator.FindPolygroupsFromFurthestPointSampling(NumPoints, WeightingType, WeightingCoeffs, &InputGroups);
			}
			else
			{
				Generator.FindPolygroupsFromFurthestPointSampling(NumPoints, WeightingType, WeightingCoeffs, nullptr);
			}
			break;
		}
		default:
			check(0);
		}

		Generator.FindPolygroupEdges();

		if (bCalculateNormals && ConversionMode == EConvertToPolygonsMode::FaceNormalDeviation)
		{
			if (ResultMesh->HasAttributes() == false)
			{
				ResultMesh->EnableAttributes();
			}

			FDynamicMeshNormalOverlay* NormalOverlay = ResultMesh->Attributes()->PrimaryNormals();
			NormalOverlay->ClearElements();

			FDynamicMeshEditor Editor(ResultMesh.Get());
			for (const TArray<int>& Polygon : Generator.FoundPolygroups)
			{
				FVector3f Normal = (FVector3f)ResultMesh->GetTriNormal(Polygon[0]);
				Editor.SetTriangleNormals(Polygon, Normal);
			}

			FMeshNormals Normals(ResultMesh.Get());
			Normals.RecomputeOverlayNormals(ResultMesh->Attributes()->PrimaryNormals());
			Normals.CopyToOverlay(NormalOverlay, false);
		}

	}

	void SetTransform(const FTransformSRT3d& Transform)
	{
		ResultTransform = Transform;
	}


};

TUniquePtr<FDynamicMeshOperator> UConvertToPolygonsOperatorFactory::MakeNewOperator()
{
	// backpointer used to populate parameters.
	check(ConvertToPolygonsTool);

	// Create the actual operator type based on the requested operation
	TUniquePtr<FConvertToPolygonsOp>  MeshOp = MakeUnique<FConvertToPolygonsOp>();

	// Operator runs on another thread - copy data over that it needs.
	ConvertToPolygonsTool->UpdateOpParameters(*MeshOp);

	// give the operator
	return MeshOp;
}

/*
 * Tool
 */
UConvertToPolygonsTool::UConvertToPolygonsTool()
{
	SetToolDisplayName(LOCTEXT("ConvertToPolygonsToolName", "Generate PolyGroups"));
}

bool UConvertToPolygonsTool::CanAccept() const
{
	return Super::CanAccept() && (PreviewCompute == nullptr || PreviewCompute->HaveValidResult());
}

void UConvertToPolygonsTool::Setup()
{
	UInteractiveTool::Setup();

	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);

	OriginalDynamicMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(UE::ToolTarget::GetDynamicMeshCopy(Target));

	Settings = NewObject<UConvertToPolygonsToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);
	FTransform MeshTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target);
	UE::ToolTarget::HideSourceObject(Target);

	{
		// create the operator factory
		UConvertToPolygonsOperatorFactory* ConvertToPolygonsOperatorFactory = NewObject<UConvertToPolygonsOperatorFactory>(this);
		ConvertToPolygonsOperatorFactory->ConvertToPolygonsTool = this; // set the back pointer

		PreviewCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(ConvertToPolygonsOperatorFactory);
		PreviewCompute->Setup(GetTargetWorld(), ConvertToPolygonsOperatorFactory);
		ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewCompute->PreviewMesh, Target);
		PreviewCompute->SetIsMeshTopologyConstant(true, EMeshRenderAttributeFlags::Positions | EMeshRenderAttributeFlags::VertexNormals);

		// Give the preview something to display
		PreviewCompute->PreviewMesh->SetTransform(MeshTransform);
		PreviewCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
		PreviewCompute->PreviewMesh->UpdatePreview(OriginalDynamicMesh.Get());
		
		PreviewCompute->ConfigureMaterials(MaterialSet.Materials,
			ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
		);

		// show the preview mesh
		PreviewCompute->SetVisibility(true);

		// something to capture the polygons from the async task when it is done
		PreviewCompute->OnOpCompleted.AddLambda([this](const FDynamicMeshOperator* MeshOp) 
		{ 
			const FConvertToPolygonsOp*  ConvertToPolygonsOp = static_cast<const FConvertToPolygonsOp*>(MeshOp);
			this->PolygonEdges = ConvertToPolygonsOp->Generator.PolygroupEdges;
			UpdateVisualization();
		});

	}
	
	PreviewGeometry = NewObject<UPreviewGeometry>(this);
	PreviewGeometry->CreateInWorld(GetTargetWorld(), MeshTransform);

	Settings->WatchProperty(Settings->ConversionMode, [this](EConvertToPolygonsMode) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->bShowGroupColors, [this](bool) { UpdateVisualization(); });
	Settings->WatchProperty(Settings->AngleTolerance, [this](float) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->NumPoints, [this](int32) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->bSplitExisting, [this](bool) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->bNormalWeighted, [this](bool) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->NormalWeighting, [this](float) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->MinGroupSize, [this](int32) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->QuadAdjacencyWeight, [this](float) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->QuadMetricClamp, [this](float) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->QuadSearchRounds, [this](int) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->bRespectUVSeams, [this](int) { OnSettingsModified(); });
	Settings->WatchProperty(Settings->bRespectHardNormals, [this](int) { OnSettingsModified(); });

	// add group layer picking property set for source groups
	CopyFromLayerProperties = NewObject<UPolygroupLayersProperties>(this);
	CopyFromLayerProperties->InitializeGroupLayers(OriginalDynamicMesh.Get());
	CopyFromLayerProperties->WatchProperty(CopyFromLayerProperties->ActiveGroupLayer, [&](FName) { OnSelectedFromGroupLayerChanged(); });
	AddToolPropertySource(CopyFromLayerProperties);
	UpdateFromGroupLayer();
	SetToolPropertySourceEnabled(CopyFromLayerProperties, Settings->ConversionMode == EConvertToPolygonsMode::CopyFromLayer);

	// add picker for output group layer
	OutputProperties = NewObject<UOutputPolygroupLayerProperties>(this);
	AddToolPropertySource(OutputProperties);
	OutputProperties->OptionsList.Reset();
	OutputProperties->OptionsList.Add(TEXT("Default"));		// always have standard group
	if (OriginalDynamicMesh->Attributes())
	{
		for (int32 k = 0; k < OriginalDynamicMesh->Attributes()->NumPolygroupLayers(); k++)
		{
			FName Name = OriginalDynamicMesh->Attributes()->GetPolygroupLayer(k)->GetName();
			OutputProperties->OptionsList.Add(Name.ToString());
		}
	}
	OutputProperties->OptionsList.Add(TEXT("Create New..."));
	OutputProperties->WatchProperty(OutputProperties->GroupLayer, [&](FName NewName) { OutputProperties->bShowNewLayerName = (NewName == TEXT("Create New...")); });


	// start the compute
	PreviewCompute->InvalidateResult();

	// updates the triangle color visualization
	UpdateVisualization();

	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Cluster triangles of the Mesh into PolyGroups using various strategies"),
		EToolMessageLevel::UserNotification);
}

void UConvertToPolygonsTool::UpdateOpParameters(FConvertToPolygonsOp& ConvertToPolygonsOp) const
{
	ConvertToPolygonsOp.bCalculateNormals = Settings->bCalculateNormals;
	ConvertToPolygonsOp.ConversionMode    = Settings->ConversionMode;
	ConvertToPolygonsOp.AngleTolerance    = Settings->AngleTolerance;
	ConvertToPolygonsOp.NumPoints = Settings->NumPoints;
	ConvertToPolygonsOp.bSubdivideExisting = Settings->bSplitExisting;
	ConvertToPolygonsOp.WeightingType = (Settings->bNormalWeighted) ? FPolygroupsGenerator::EWeightingType::NormalDeviation : FPolygroupsGenerator::EWeightingType::None;
	ConvertToPolygonsOp.WeightingCoeffs = FVector3d(Settings->NormalWeighting, 1.0, 1.0);
	ConvertToPolygonsOp.MinGroupSize = Settings->MinGroupSize;
	ConvertToPolygonsOp.QuadMetricClamp = Settings->QuadMetricClamp;
	ConvertToPolygonsOp.QuadAdjacencyWeight = Settings->QuadAdjacencyWeight;
	ConvertToPolygonsOp.QuadSearchRounds = Settings->QuadSearchRounds;
	ConvertToPolygonsOp.bRespectUVSeams = Settings->bRespectUVSeams;
	ConvertToPolygonsOp.bRespectHardNormals = Settings->bRespectHardNormals;

	ConvertToPolygonsOp.OriginalMesh = OriginalDynamicMesh;
	if (ActiveFromGroupSet.IsValid())
	{
		ConvertToPolygonsOp.SourcePolyGroups = ActiveFromGroupSet;
	}

	FTransform LocalToWorld = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target);
	ConvertToPolygonsOp.SetTransform(LocalToWorld);
}



void UConvertToPolygonsTool::OnShutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	UE::ToolTarget::ShowSourceObject(Target);

	PreviewGeometry->Disconnect();
	PreviewGeometry = nullptr;

	if (PreviewCompute)
	{
		FDynamicMeshOpResult Result = PreviewCompute->Shutdown();
		if (ShutdownType == EToolShutdownType::Accept)
		{
			GetToolManager()->BeginUndoTransaction(LOCTEXT("ConvertToPolygonsToolTransactionName", "Find Polygroups"));
			FDynamicMesh3* DynamicMeshResult = Result.Mesh.Get();
			if (ensure(DynamicMeshResult != nullptr))
			{
				if (OutputProperties->GroupLayer != TEXT("Default"))
				{
					// if we want to write to any layer other than default, we have to find or create it
					FDynamicMesh3 UseResultMesh = *OriginalDynamicMesh;
					if (UseResultMesh.HasAttributes() == false)
					{
						UseResultMesh.EnableAttributes();
					}

					FDynamicMeshPolygroupAttribute* UseAttribLayer = nullptr;
					if (OutputProperties->GroupLayer == TEXT("Create New..."))
					{
						// append new group layer and set it's name
						int32 TargetLayerIdx = UseResultMesh.Attributes()->NumPolygroupLayers();
						UseResultMesh.Attributes()->SetNumPolygroupLayers(TargetLayerIdx + 1);
						UseAttribLayer = UseResultMesh.Attributes()->GetPolygroupLayer(TargetLayerIdx);
						FString UseUniqueName = UE::Geometry::MakeUniqueGroupLayerName(UseResultMesh, OutputProperties->NewLayerName);
						UseAttribLayer->SetName( FName(UseUniqueName) );
					}
					else
					{
						UseAttribLayer = UE::Geometry::FindPolygroupLayerByName(UseResultMesh, OutputProperties->GroupLayer);
					}

					if (UseAttribLayer)
					{
						// copy the generated groups from the Op output mesh (stored in primary groups) to the target layer
						for (int32 tid : UseResultMesh.TriangleIndicesItr())
						{
							UseAttribLayer->SetValue(tid, DynamicMeshResult->GetTriangleGroup(tid));
						}
						UE::ToolTarget::CommitDynamicMeshUpdate(Target, UseResultMesh, true);
					}
					else
					{
						// If we can't find or create the layer (which should not be possible) the tool is going to do nothing,
						// this is the safest option at this point
						UE_LOG(LogGeometry, Warning, TEXT("Output group layer missing?"));
					}
				}
				else
				{
					// todo: have not actually modified topology here, but groups-only update is not supported yet
					UE::ToolTarget::CommitDynamicMeshUpdate(Target, *DynamicMeshResult, true);
				}
			}
			GetToolManager()->EndUndoTransaction();
		}
	}
}

void UConvertToPolygonsTool::OnSettingsModified()
{
	SetToolPropertySourceEnabled(CopyFromLayerProperties, Settings->ConversionMode == EConvertToPolygonsMode::CopyFromLayer);

	PreviewCompute->InvalidateResult();
}


void UConvertToPolygonsTool::OnTick(float DeltaTime)
{
	PreviewCompute->Tick(DeltaTime);
}


void UConvertToPolygonsTool::OnSelectedFromGroupLayerChanged()
{
	UpdateFromGroupLayer();
	PreviewCompute->InvalidateResult();
}

void UConvertToPolygonsTool::UpdateFromGroupLayer()
{
	if (CopyFromLayerProperties->HasSelectedPolygroup() == false)
	{
		ActiveFromGroupSet = MakeShared<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe>(OriginalDynamicMesh.Get());
	}
	else
	{
		FName SelectedName = CopyFromLayerProperties->ActiveGroupLayer;
		FDynamicMeshPolygroupAttribute* FoundAttrib = UE::Geometry::FindPolygroupLayerByName(*OriginalDynamicMesh, SelectedName);
		ensureMsgf(FoundAttrib, TEXT("Selected Attribute Not Found! Falling back to Default group layer."));
		ActiveFromGroupSet = MakeShared<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe>(OriginalDynamicMesh.Get(), FoundAttrib);
	}
}


void UConvertToPolygonsTool::UpdateVisualization()
{
	if (!PreviewCompute)
	{
		return;
	}

	IMaterialProvider* MaterialTarget = Cast<IMaterialProvider>(Target);
	FComponentMaterialSet MaterialSet;
	if (Settings->bShowGroupColors)
	{
		int32 NumMaterials = MaterialTarget->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{ 
			MaterialSet.Materials.Add(ToolSetupUtil::GetSelectionMaterial(GetToolManager()));
		}
		PreviewCompute->PreviewMesh->SetTriangleColorFunction([this](const FDynamicMesh3* Mesh, int TriangleID)
		{
			return LinearColors::SelectFColor(Mesh->GetTriangleGroup(TriangleID));
		}, 
		UPreviewMesh::ERenderUpdateMode::FastUpdate);
	}
	else
	{
		MaterialTarget->GetMaterialSet(MaterialSet);
		PreviewCompute->PreviewMesh->ClearTriangleColorFunction(UPreviewMesh::ERenderUpdateMode::FastUpdate);
	}
	PreviewCompute->ConfigureMaterials(MaterialSet.Materials,
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager()));

	FColor GroupLineColor = FColor::Red;
	float GroupLineThickness = 2.0f;

	PreviewGeometry->CreateOrUpdateLineSet(TEXT("GroupBorders"), PolygonEdges.Num(), 
		[&](int32 k, TArray<FRenderableLine>& LinesOut) {
			FVector3d A, B;
			OriginalDynamicMesh->GetEdgeV(PolygonEdges[k], A, B);
			LinesOut.Add(FRenderableLine(A, B, GroupLineColor, GroupLineThickness));
		}, 1);

}



#undef LOCTEXT_NAMESPACE

