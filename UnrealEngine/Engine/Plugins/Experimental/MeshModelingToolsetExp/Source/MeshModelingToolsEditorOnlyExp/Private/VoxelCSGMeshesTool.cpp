// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelCSGMeshesTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "Selection/ToolSelectionUtil.h"
#include "ModelingToolTargetUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "CompositionOps/VoxelBooleanMeshesOp.h"

#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolTargetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VoxelCSGMeshesTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UVoxelCSGMeshesTool"


/*
 * ToolBuilder
 */
bool UVoxelCSGMeshesToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 2;
}

UMultiSelectionMeshEditingTool* UVoxelCSGMeshesToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UVoxelCSGMeshesTool>(SceneState.ToolManager);
}

/*
 * Tool
 */

UVoxelCSGMeshesTool::UVoxelCSGMeshesTool()
{
}

void UVoxelCSGMeshesTool::Setup()
{
	UInteractiveTool::Setup();

	CSGProps = NewObject<UVoxelCSGMeshesToolProperties>();
	CSGProps->RestoreProperties(this);
	AddToolPropertySource(CSGProps);

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
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, nullptr);
	Preview->OnMeshUpdated.AddLambda([this](UMeshOpPreviewWithBackgroundCompute* Compute) {
		MeshStatisticsProperties->Update(*Compute->PreviewMesh->GetPreviewDynamicMesh());
		UpdateAcceptWarnings(Compute->HaveEmptyResult() ? EAcceptWarning::EmptyForbidden : EAcceptWarning::NoWarning);
	});

	CreateLowQualityPreview();

	Preview->ConfigureMaterials(
		ToolSetupUtil::GetDefaultSculptMaterial(GetToolManager()),
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
	);
	
	Preview->InvalidateResult();    // start compute

	SetToolDisplayName(LOCTEXT("ToolName", "Voxel Boolean"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Compute a CSG Boolean of the input meshes using voxelization techniques. UVs, sharp edges, and small/thin features will be lost. Increase Voxel Count to enhance accuracy."),
		EToolMessageLevel::UserNotification);
}


void UVoxelCSGMeshesTool::OnShutdown(EToolShutdownType ShutdownType)
{
	CSGProps->SaveProperties(this);
	HandleSourcesProperties->SaveProperties(this);

	FDynamicMeshOpResult Result = Preview->Shutdown();
	// Restore (unhide) the source meshes
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
	}
	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("BooleanMeshes", "Boolean Meshes"));

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



void UVoxelCSGMeshesTool::OnTick(float DeltaTime)
{
	Preview->Tick(DeltaTime);
}

void UVoxelCSGMeshesTool::Render(IToolsContextRenderAPI* RenderAPI)
{
}

bool UVoxelCSGMeshesTool::CanAccept() const
{
	return Super::CanAccept() && Preview->HaveValidNonEmptyResult();
}

void UVoxelCSGMeshesTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Preview->InvalidateResult();
}

TUniquePtr<FDynamicMeshOperator> UVoxelCSGMeshesTool::MakeNewOperator()
{
	TUniquePtr<FVoxelBooleanMeshesOp> CSGOp = MakeUnique<FVoxelBooleanMeshesOp>();
	CSGOp->Operation      = (FVoxelBooleanMeshesOp::EBooleanOperation)(int)CSGProps->Operation;
	CSGOp->VoxelCount     = CSGProps->VoxelCount;
	CSGOp->AdaptivityD    = CSGProps->MeshAdaptivity;
	CSGOp->IsoSurfaceD    = CSGProps->OffsetDistance;
	CSGOp->bAutoSimplify  = CSGProps->bAutoSimplify;
	CSGOp->InputMeshArray = InputMeshes;
	return CSGOp;
}

void UVoxelCSGMeshesTool::CacheInputMeshes()
{
	InputMeshes.Reset();

	// Package the selected meshes and transforms for consumption by the CSGTool
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::Geometry::FVoxelBooleanMeshesOp::FInputMesh InputMesh;
		InputMesh.Mesh = UE::ToolTarget::GetMeshDescription(Targets[ComponentIdx]);
		InputMesh.Transform = (FTransform) UE::ToolTarget::GetLocalToWorldTransform(Targets[ComponentIdx]);
		InputMeshes.Add(InputMesh);
	}
}

void UVoxelCSGMeshesTool::CreateLowQualityPreview()
{

	FProgressCancel NullInterrupter;
	FVoxelBooleanMeshesOp BooleanOp;

	BooleanOp.Operation = (FVoxelBooleanMeshesOp::EBooleanOperation)(int)CSGProps->Operation;
	BooleanOp.VoxelCount = 12;
	BooleanOp.AdaptivityD = 0.01;
	BooleanOp.bAutoSimplify = true;
	BooleanOp.InputMeshArray = InputMeshes;
	
	BooleanOp.CalculateResult(&NullInterrupter);
	TUniquePtr<FDynamicMesh3> FastPreviewMesh = BooleanOp.ExtractResult();


	Preview->PreviewMesh->SetTransform((FTransform)BooleanOp.GetResultTransform());
	Preview->PreviewMesh->UpdatePreview(FastPreviewMesh.Get());  // copies the mesh @todo we could just give ownership to the Preview!
	Preview->SetVisibility(true);
}

void UVoxelCSGMeshesTool::GenerateAsset(const FDynamicMeshOpResult& OpResult)
{
	if (ensure(OpResult.Mesh.Get() != nullptr))
	{
		FCreateMeshObjectParams NewMeshObjectParams;
		NewMeshObjectParams.TargetWorld = GetTargetWorld();
		NewMeshObjectParams.Transform = (FTransform)OpResult.Transform;
		NewMeshObjectParams.BaseName = TEXT("CSGMesh");
		NewMeshObjectParams.Materials.Add(ToolSetupUtil::GetDefaultMaterial());
		NewMeshObjectParams.SetMesh(OpResult.Mesh.Get());
		FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
		if (Result.IsOK() && Result.NewActor != nullptr)
		{
			ToolSelectionUtil::SetNewActorSelection(GetToolManager(), Result.NewActor);
		}
	}
}

#undef LOCTEXT_NAMESPACE

