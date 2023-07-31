// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvertMeshesTool.h"
#include "ComponentSourceInterfaces.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "Selection/ToolSelectionUtil.h"

#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConvertMeshesTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UConvertMeshesTool"

/*
 * ToolBuilder
 */
const FToolTargetTypeRequirements& UConvertMeshesToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMaterialProvider::StaticClass(),
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

UMultiSelectionMeshEditingTool* UConvertMeshesToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UConvertMeshesTool>(SceneState.ToolManager);
}



/*
 * Tool
 */


void UConvertMeshesTool::Setup()
{
	UInteractiveTool::Setup();

	OutputTypeProperties = NewObject<UCreateMeshObjectTypeProperties>(this);
	OutputTypeProperties->RestoreProperties(this, TEXT("ConvertMeshesTool"));
	OutputTypeProperties->InitializeDefault();
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	AddToolPropertySource(OutputTypeProperties);

	BasicProperties = NewObject<UConvertMeshesToolProperties>(this);
	BasicProperties->RestoreProperties(this);
	AddToolPropertySource(BasicProperties);

	SetToolDisplayName(LOCTEXT("ToolName", "Convert"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Convert Meshes to a different Mesh Object type"),
		EToolMessageLevel::UserNotification);
}

bool UConvertMeshesTool::CanAccept() const
{
	return Super::CanAccept();
}

void UConvertMeshesTool::OnShutdown(EToolShutdownType ShutdownType)
{
	OutputTypeProperties->SaveProperties(this, TEXT("ConvertMeshesTool"));
	BasicProperties->SaveProperties(this);

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("ConvertMeshesToolTransactionName", "Convert Meshes"));

		TArray<AActor*> NewSelectedActors;
		TSet<AActor*> DeleteActors;
		TArray<FCreateMeshObjectParams> NewMeshObjects;

		// Accumulate info for new mesh objects. Do not immediately create them because then
		// the new Actors will get a unique-name incremented suffix, because the convert-from
		// Actors still exist.
		for (int32 k = 0; k < Targets.Num(); ++k)
		{
			AActor* TargetActor = UE::ToolTarget::GetTargetActor(Targets[k]);
			check(TargetActor != nullptr);
			DeleteActors.Add(TargetActor);

			FTransform SourceTransform = (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Targets[k]);
			FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], true);
			FString AssetName = TargetActor->GetActorNameOrLabel();
			FComponentMaterialSet Materials = UE::ToolTarget::GetMaterialSet(Targets[0]);
			const FComponentMaterialSet* TransferMaterials = (BasicProperties->bTransferMaterials) ? &Materials : nullptr;

			FCreateMeshObjectParams NewMeshObjectParams;
			NewMeshObjectParams.TargetWorld = GetTargetWorld();
			NewMeshObjectParams.Transform = (FTransform)SourceTransform;
			NewMeshObjectParams.BaseName = AssetName;
			if (TransferMaterials != nullptr)
			{
				NewMeshObjectParams.Materials = TransferMaterials->Materials;
			}
			NewMeshObjectParams.SetMesh(MoveTemp(SourceMesh));

			NewMeshObjects.Add(MoveTemp(NewMeshObjectParams));
		}

		// delete all the existing Actors we want to get rid of
		for (AActor* DeleteActor : DeleteActors)
		{
			DeleteActor->Destroy();
		}

		// spawn new mesh objects
		for (FCreateMeshObjectParams& NewMeshObjectParams : NewMeshObjects)
		{
			OutputTypeProperties->ConfigureCreateMeshObjectParams(NewMeshObjectParams);
			FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
			if (Result.IsOK())
			{
				NewSelectedActors.Add(Result.NewActor);
			}
		}

		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewSelectedActors);

		GetToolManager()->EndUndoTransaction();
	}
}





#undef LOCTEXT_NAMESPACE

