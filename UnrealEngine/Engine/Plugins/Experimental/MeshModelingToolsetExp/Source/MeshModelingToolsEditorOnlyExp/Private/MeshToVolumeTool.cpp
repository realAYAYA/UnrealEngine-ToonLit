// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshToVolumeTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "MeshSimplification.h"
#include "Util/ColorConstants.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingToolTargetUtil.h"

#include "Engine/BlockingVolume.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Model.h"
#include "BSPOps.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshToVolumeTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UMeshToVolumeTool"

/*
 * ToolBuilder
 */

bool UMeshToVolumeToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	// We don't want to allow this tool to run on selected volumes
	return ToolBuilderUtil::CountSelectedActorsOfType<AVolume>(SceneState) == 0 && Super::CanBuildTool(SceneState);
}

USingleSelectionMeshEditingTool* UMeshToVolumeToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UMeshToVolumeTool>(SceneState.ToolManager);
}

/*
 * Tool
 */
UMeshToVolumeTool::UMeshToVolumeTool()
{
	SetToolDisplayName(LOCTEXT("MeshToVolumeToolName", "Mesh To Volume"));
}


void UMeshToVolumeTool::Setup()
{
	UInteractiveTool::Setup();

	PreviewMesh = NewObject<UPreviewMesh>(this);
	PreviewMesh->bBuildSpatialDataStructure = false;
	PreviewMesh->CreateInWorld(UE::ToolTarget::GetTargetActor(Target)->GetWorld(), FTransform::Identity);
	PreviewMesh->SetTransform((FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target));
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(PreviewMesh, nullptr);

	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	PreviewMesh->ReplaceMesh(UE::ToolTarget::GetDynamicMeshCopy(Target));

	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	PreviewMesh->SetMaterials(MaterialSet.Materials);

	InputMesh.Copy(*PreviewMesh->GetMesh());

	VolumeEdgesSet = NewObject<ULineSetComponent>(PreviewMesh->GetRootComponent());
	VolumeEdgesSet->SetupAttachment(PreviewMesh->GetRootComponent());
	VolumeEdgesSet->SetLineMaterial(ToolSetupUtil::GetDefaultLineComponentMaterial(GetToolManager()));
	VolumeEdgesSet->RegisterComponent();

	UE::ToolTarget::HideSourceObject(Target);

	Settings = NewObject<UMeshToVolumeToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	Settings->WatchProperty(Settings->ConversionMode,
							[this](EMeshToVolumeMode NewMode)
							{ bVolumeValid = false; });


	HandleSourcesProperties = NewObject<UOnAcceptHandleSourcesProperties>(this);
	HandleSourcesProperties->RestoreProperties(this);
	AddToolPropertySource(HandleSourcesProperties);

	bVolumeValid = false;
	

	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Convert a Static Mesh to a Volume, or update an existing Volume"),
		EToolMessageLevel::UserNotification);

	// check for errors in input mesh
	bool bFoundBoundaryEdges = false;
	for (int32 BoundaryEdgeID : InputMesh.BoundaryEdgeIndicesItr())
	{
		bFoundBoundaryEdges = true;
		break;
	}
	if (bFoundBoundaryEdges)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OpenBoundaryEdges", "Input Mesh is non-Closed and may produce a broken Volume"),
			EToolMessageLevel::UserWarning);
	}
}

void UMeshToVolumeTool::OnShutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	HandleSourcesProperties->SaveProperties(this);

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	UE::ToolTarget::ShowSourceObject(Target);

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("MeshToVolumeToolTransactionName", "Create Volume"));

		AActor* TargetOwnerActor = UE::ToolTarget::GetTargetActor(Target);
		UWorld* TargetOwnerWorld = TargetOwnerActor->GetWorld();
		FTransform SetTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Target);

		AVolume* TargetVolume = nullptr;

		if (Settings->TargetVolume.IsValid() == false)
		{
			FRotator Rotation(0.0f, 0.0f, 0.0f);
			FActorSpawnParameters SpawnInfo;
			FTransform NewActorTransform = FTransform::Identity;
			UClass* VolumeClass = Settings->NewVolumeType.Get();
			if (VolumeClass)
			{
				TargetVolume = (AVolume*)TargetOwnerWorld->SpawnActor(VolumeClass, &NewActorTransform, SpawnInfo);
			}
			else
			{
				TargetVolume = TargetOwnerWorld->SpawnActor<ABlockingVolume>(FVector::ZeroVector, Rotation, SpawnInfo);
			}
			TargetVolume->BrushType = EBrushType::Brush_Add;
			UModel* Model = NewObject<UModel>(TargetVolume);
			TargetVolume->Brush = Model;
			TargetVolume->GetBrushComponent()->Brush = TargetVolume->Brush;
		}
		else
		{
			TargetVolume = Settings->TargetVolume.Get();
			SetTransform = TargetVolume->GetActorTransform();
			TargetVolume->Modify();
			TargetVolume->GetBrushComponent()->Modify();
		}

		UE::Conversion::DynamicMeshToVolume(InputMesh, Faces, TargetVolume);
		TargetVolume->SetActorTransform(SetTransform);
		TargetVolume->PostEditChange();

		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), TargetVolume);

		TArray<AActor*> Actors;
		Actors.Add(TargetOwnerActor);
		HandleSourcesProperties->ApplyMethod(Actors, GetToolManager());

		GetToolManager()->EndUndoTransaction();
	}
}

void UMeshToVolumeTool::OnTick(float DeltaTime)
{
	if (bVolumeValid == false)
	{
		RecalculateVolume();
	}
}

void UMeshToVolumeTool::Render(IToolsContextRenderAPI* RenderAPI)
{
}


void UMeshToVolumeTool::UpdateLineSet()
{
	FColor BoundaryEdgeColor(240, 15, 15);
	float BoundaryEdgeThickness = 0.5;
	float BoundaryEdgeDepthBias = 2.0f;

	VolumeEdgesSet->Clear();
	for (const UE::Conversion::FDynamicMeshFace& Face : Faces)
	{
		int32 NumV = Face.BoundaryLoop.Num();
		for (int32 k = 0; k < NumV; ++k)
		{
			VolumeEdgesSet->AddLine(
				(FVector)Face.BoundaryLoop[k], (FVector)Face.BoundaryLoop[(k+1)%NumV],
				BoundaryEdgeColor, BoundaryEdgeThickness, BoundaryEdgeDepthBias);
		}
	}

}

void UMeshToVolumeTool::RecalculateVolume()
{
	UE::Conversion::FMeshToVolumeOptions DefaultOptions;
	bool bShowTooLargeWarning = false;

	if (Settings->ConversionMode == EMeshToVolumeMode::MinimalPolygons)
	{
		// Since this tool is likely to be a sink, there isn't much reason to keep
		// the group differentiations if they are coplanar.
		bool bRespectGroupBoundaries = false;

		// Apply minimal-planar simplification to remove extra vertices along straight edges
		FDynamicMesh3 LocalMesh = InputMesh;
		LocalMesh.DiscardAttributes();
		FQEMSimplification PlanarSimplifier(&LocalMesh);
		PlanarSimplifier.SimplifyToMinimalPlanar(0.1);		// angle tolerance in degrees

		UE::Conversion::GetPolygonFaces(LocalMesh, Faces, bRespectGroupBoundaries);

		bShowTooLargeWarning = (LocalMesh.TriangleCount() > DefaultOptions.MaxTriangles);
	}
	else
	{
		UE::Conversion::GetTriangleFaces(InputMesh, Faces);

		bShowTooLargeWarning = (InputMesh.TriangleCount() > DefaultOptions.MaxTriangles);
	}

	if (bShowTooLargeWarning)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("LargeFaceCount", "Mesh has large face count, output Volume representation may be automatically simplified"),
			EToolMessageLevel::UserWarning);
	}
	else
	{
		GetToolManager()->DisplayMessage({}, EToolMessageLevel::UserWarning);
	}

	UpdateLineSet();
	bVolumeValid = true;
}


#undef LOCTEXT_NAMESPACE

