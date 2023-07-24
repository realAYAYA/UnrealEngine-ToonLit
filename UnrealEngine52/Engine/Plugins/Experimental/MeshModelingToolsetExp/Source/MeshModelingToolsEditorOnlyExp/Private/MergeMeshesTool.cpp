// Copyright Epic Games, Inc. All Rights Reserved.

#include "MergeMeshesTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "ModelingToolTargetUtil.h"
#include "Selection/ToolSelectionUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "CompositionOps/VoxelMergeMeshesOp.h"

#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MergeMeshesTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UMergeMeshesTool"

/*
 * ToolBuilder
 */


const FToolTargetTypeRequirements& UMergeMeshesToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

bool UMergeMeshesToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	constexpr int32 MinRequiredComponents = 1;
	const bool bHasComponents = SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) >= MinRequiredComponents;
	return ( bHasComponents );
}

UMultiSelectionMeshEditingTool* UMergeMeshesToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UMergeMeshesTool>(SceneState.ToolManager);
}




/*
 * Tool
 */

UMergeMeshesTool::UMergeMeshesTool()
{
}


void UMergeMeshesTool::Setup()
{
	UInteractiveTool::Setup();

	MergeProps = NewObject<UMergeMeshesToolProperties>();
	MergeProps->RestoreProperties(this);
	AddToolPropertySource(MergeProps);

	MeshStatisticsProperties = NewObject<UMeshStatisticsProperties>(this);
	AddToolPropertySource(MeshStatisticsProperties);

	HandleSourcesProperties = NewObject<UOnAcceptHandleSourcesProperties>(this);
	HandleSourcesProperties->RestoreProperties(this);
	AddToolPropertySource(HandleSourcesProperties);

	// Hide the source meshes
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::ToolTarget::HideSourceObject(Targets[ComponentIdx]);
	}

	// save transformed version of input meshes (maybe this could happen in the Operator?)
	CacheInputMeshes();

	// initialize the PreviewMesh+BackgroundCompute object
	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>(this, "Preview");
	Preview->Setup(GetTargetWorld(), this);
	Preview->OnMeshUpdated.AddLambda([this](UMeshOpPreviewWithBackgroundCompute* Compute) {
		MeshStatisticsProperties->Update(*Compute->PreviewMesh->GetPreviewDynamicMesh());
		UpdateAcceptWarnings(Compute->HaveEmptyResult() ? EAcceptWarning::EmptyForbidden : EAcceptWarning::NoWarning);
	});
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, nullptr);

	CreateLowQualityPreview(); // update the preview with a low-quality result
	
	Preview->ConfigureMaterials(
		ToolSetupUtil::GetDefaultSculptMaterial(GetToolManager()),
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
	);

	Preview->InvalidateResult();    // start compute


	SetToolDisplayName(LOCTEXT("ToolName", "Voxel Merge"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Combine the input meshes into closed solids using voxelization techniques. UVs, sharp edges, and small/thin features will be lost. Increase Voxel Count to enhance accuracy."),
		EToolMessageLevel::UserNotification);
}


void UMergeMeshesTool::OnShutdown(EToolShutdownType ShutdownType)
{
	MergeProps->SaveProperties(this);
	HandleSourcesProperties->SaveProperties(this);

	FDynamicMeshOpResult Result = Preview->Shutdown();
	// Restore (unhide) the source meshes
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
	}
	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("MergeMeshes", "Merge Meshes"));

		// Generate the result
		GenerateAsset(Result);

		TArray<AActor*> Actors;
		for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
		{
			Actors.Add(UE::ToolTarget::GetTargetActor(Targets[ComponentIdx]));
		}
		HandleSourcesProperties->ApplyMethod(Actors, GetToolManager());

		GetToolManager()->EndUndoTransaction();
	}
}



void UMergeMeshesTool::OnTick(float DeltaTime)
{
	Preview->Tick(DeltaTime);
}

bool UMergeMeshesTool::CanAccept() const
{
	return Super::CanAccept() && Preview->HaveValidNonEmptyResult();
}

void UMergeMeshesTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Preview->InvalidateResult();
}




TUniquePtr<FDynamicMeshOperator> UMergeMeshesTool::MakeNewOperator()
{
	TUniquePtr<FVoxelMergeMeshesOp> MergeOp = MakeUnique<FVoxelMergeMeshesOp>();
	MergeOp->VoxelCount     = MergeProps->VoxelCount;
	MergeOp->AdaptivityD    = MergeProps->MeshAdaptivity;
	MergeOp->IsoSurfaceD    = MergeProps->OffsetDistance;
	MergeOp->bAutoSimplify  = MergeProps->bAutoSimplify;
	MergeOp->InputMeshArray = InputMeshes;
	return MergeOp;
}



void UMergeMeshesTool::CacheInputMeshes()
{
	InputMeshes.Reset();

	// Package the selected meshes and transforms for consumption by the CSGTool
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::Geometry::FVoxelMergeMeshesOp::FInputMesh InputMesh;
		InputMesh.Mesh = UE::ToolTarget::GetMeshDescription(Targets[ComponentIdx]);
		InputMesh.Transform = (FTransform) UE::ToolTarget::GetLocalToWorldTransform(Targets[ComponentIdx]);
		InputMeshes.Add(InputMesh);
	}
}

void UMergeMeshesTool::CreateLowQualityPreview()
{

	FProgressCancel NullInterrupter;
	FVoxelMergeMeshesOp MergeMeshesOp;

	MergeMeshesOp.VoxelCount = 12;
	MergeMeshesOp.AdaptivityD = 0.001;
	MergeMeshesOp.bAutoSimplify = true;
	MergeMeshesOp.InputMeshArray = InputMeshes;
	
	MergeMeshesOp.CalculateResult(&NullInterrupter);
	TUniquePtr<FDynamicMesh3> FastPreviewMesh = MergeMeshesOp.ExtractResult();


	Preview->PreviewMesh->SetTransform((FTransform)MergeMeshesOp.GetResultTransform());
	Preview->PreviewMesh->UpdatePreview(FastPreviewMesh.Get());  // copies the mesh @todo we could just give ownership to the Preview!
	Preview->SetVisibility(true);
}


void UMergeMeshesTool::GenerateAsset(const FDynamicMeshOpResult& OpResult)
{
	check(OpResult.Mesh.Get() != nullptr);
	
	FCreateMeshObjectParams NewMeshObjectParams;
	NewMeshObjectParams.TargetWorld = GetTargetWorld();
	NewMeshObjectParams.Transform = (FTransform)OpResult.Transform;
	NewMeshObjectParams.BaseName = TEXT("MergedMesh");
	NewMeshObjectParams.Materials.Add(ToolSetupUtil::GetDefaultMaterial());
	NewMeshObjectParams.SetMesh(OpResult.Mesh.Get());
	FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
	if (Result.IsOK() && Result.NewActor != nullptr)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), Result.NewActor);
	}

}



#undef LOCTEXT_NAMESPACE

