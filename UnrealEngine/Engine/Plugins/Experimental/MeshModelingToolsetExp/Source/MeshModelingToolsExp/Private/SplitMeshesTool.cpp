// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitMeshesTool.h"
#include "ComponentSourceInterfaces.h"
#include "InteractiveToolManager.h"
#include "ToolTargetManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "Selection/ToolSelectionUtil.h"
#include "Selections/MeshConnectedComponents.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicSubmesh3.h"

#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SplitMeshesTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "USplitMeshesTool"

/*
 * ToolBuilder
 */
const FToolTargetTypeRequirements& USplitMeshesToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMaterialProvider::StaticClass(),
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

UMultiSelectionMeshEditingTool* USplitMeshesToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<USplitMeshesTool>(SceneState.ToolManager);
}



/*
 * Tool
 */


void USplitMeshesTool::Setup()
{
	UInteractiveTool::Setup();

	OutputTypeProperties = NewObject<UCreateMeshObjectTypeProperties>(this);
	OutputTypeProperties->InitializeDefaultWithAuto();
	OutputTypeProperties->OutputType = UCreateMeshObjectTypeProperties::AutoIdentifier;
	OutputTypeProperties->RestoreProperties(this, TEXT("OutputTypeFromInputTool"));
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	AddToolPropertySource(OutputTypeProperties);

	BasicProperties = NewObject<USplitMeshesToolProperties>(this);
	BasicProperties->RestoreProperties(this);
	AddToolPropertySource(BasicProperties);

	SourceMeshes.SetNum(Targets.Num());
	for (int32 k = 0; k < Targets.Num(); ++k)
	{
		SourceMeshes[k].Mesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[k], true);
		SourceMeshes[k].Materials = UE::ToolTarget::GetMaterialSet(Targets[k]).Materials;
	}

	UpdateSplitMeshes();

	SetToolDisplayName(LOCTEXT("ToolName", "Split"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Split Meshes into parts"),
		EToolMessageLevel::UserNotification);
}

bool USplitMeshesTool::CanAccept() const
{
	return Super::CanAccept();
}

void USplitMeshesTool::OnShutdown(EToolShutdownType ShutdownType)
{
	OutputTypeProperties->SaveProperties(this, TEXT("OutputTypeFromInputTool"));
	BasicProperties->SaveProperties(this);

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SplitMeshesToolTransactionName", "Split Meshes"));

		TArray<AActor*> NewSelectedActors;
		TSet<AActor*> DeleteActors;

		for (int32 ti = 0; ti < Targets.Num(); ++ti)
		{
			FComponentsInfo& SplitInfo = SplitMeshes[ti];
			if (SplitInfo.bNoComponents)
			{
				continue;
			}
			AActor* TargetActor = UE::ToolTarget::GetTargetActor(Targets[ti]);
			check(TargetActor != nullptr);
			DeleteActors.Add(TargetActor);

			FTransform3d SourceTransform = UE::ToolTarget::GetLocalToWorldTransform(Targets[ti]);
			FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(Targets[ti], true);
			FString AssetName = TargetActor->GetActorNameOrLabel();

			FCreateMeshObjectParams BaseMeshObjectParams;
			BaseMeshObjectParams.TargetWorld = GetTargetWorld();

			if (OutputTypeProperties->OutputType == UCreateMeshObjectTypeProperties::AutoIdentifier)
			{
				UE::ToolTarget::ConfigureCreateMeshObjectParams(Targets[ti], BaseMeshObjectParams);
			}
			else
			{
				OutputTypeProperties->ConfigureCreateMeshObjectParams(BaseMeshObjectParams);
			}

			int32 NumComponents = SplitInfo.Meshes.Num();
			for (int32 k = 0; k < NumComponents; ++k)
			{
				FCreateMeshObjectParams NewMeshObjectParams = BaseMeshObjectParams;
				NewMeshObjectParams.BaseName = FString::Printf(TEXT("%s_%d"), *AssetName, k);
				FTransform3d PartTransform = SourceTransform;
				PartTransform.SetTranslation(SourceTransform.GetTranslation() + SplitInfo.Origins[k]);
				NewMeshObjectParams.Transform = (FTransform)PartTransform;
				if (BasicProperties->bTransferMaterials)
				{
					NewMeshObjectParams.Materials = SplitInfo.Materials[k];
				}
				NewMeshObjectParams.SetMesh(MoveTemp(SplitInfo.Meshes[k]));

				FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
				if (Result.IsOK())
				{
					NewSelectedActors.Add(Result.NewActor);
				}
			}
		}

		for (AActor* DeleteActor : DeleteActors)
		{
			DeleteActor->Destroy();
		}

		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), NewSelectedActors);

		GetToolManager()->EndUndoTransaction();
	}
}




void USplitMeshesTool::UpdateSplitMeshes()
{
	SplitMeshes.Reset();
	SplitMeshes.SetNum(SourceMeshes.Num());
	NoSplitCount = 0;

	for (int32 si = 0; si < SourceMeshes.Num(); ++si)
	{
		FComponentsInfo& SplitInfo = SplitMeshes[si];
		const FDynamicMesh3* SourceMesh = &SourceMeshes[si].Mesh;
		const TArray<UMaterialInterface*> SourceMaterials = SourceMeshes[si].Materials;

		FMeshConnectedComponents MeshComponents(SourceMesh);
		MeshComponents.FindConnectedTriangles();
		int32 NumComponents = MeshComponents.Num();

		if (NumComponents < 2)
		{
			SplitInfo.bNoComponents = true;
			NoSplitCount++;
			continue;
		}
		SplitInfo.bNoComponents = false;

		SplitInfo.Meshes.SetNum(NumComponents);
		SplitInfo.Materials.SetNum(NumComponents);
		SplitInfo.Origins.SetNum(NumComponents);
		for (int32 k = 0; k < NumComponents; ++k)
		{
			FDynamicSubmesh3 SubmeshCalc(SourceMesh, MeshComponents[k].Indices);
			FDynamicMesh3& Submesh = SubmeshCalc.GetSubmesh();
			TArray<UMaterialInterface*> NewMaterials;

			// remap materials
			FDynamicMeshMaterialAttribute* MaterialIDs = Submesh.HasAttributes() ? Submesh.Attributes()->GetMaterialID() : nullptr;
			if (MaterialIDs)
			{
				TArray<int32> UniqueIDs;
				for (int32 tid : Submesh.TriangleIndicesItr())
				{
					int32 MaterialID = MaterialIDs->GetValue(tid);
					int32 Index = UniqueIDs.IndexOfByKey(MaterialID);
					if (Index == INDEX_NONE)
					{
						int32 NewMaterialID = UniqueIDs.Num();
						UniqueIDs.Add(MaterialID);
						NewMaterials.Add(SourceMaterials[MaterialID]);
						MaterialIDs->SetValue(tid, NewMaterialID);
					}
					else
					{
						MaterialIDs->SetValue(tid, Index);
					}
				}
			}
			
			// TODO: Consider whether to expose bCenterPivots as an option to the user
			constexpr bool bCenterPivots = false;
			FVector3d Origin = FVector3d::ZeroVector;
			if (bCenterPivots)
			{
				// reposition mesh
				FAxisAlignedBox3d Bounds = Submesh.GetBounds();
				Origin = Bounds.Center();
				MeshTransforms::Translate(Submesh, -Origin);
			}

			SplitInfo.Meshes[k] = MoveTemp(Submesh);
			SplitInfo.Materials[k] = MoveTemp(NewMaterials);
			SplitInfo.Origins[k] = Origin;
		}
	}

	if (NoSplitCount > 0)
	{
		GetToolManager()->DisplayMessage(
			FText::Format(LOCTEXT("NoComponentsMessage", "{0} of {1} Input Meshes cannot be Split."), NoSplitCount, SourceMeshes.Num()), EToolMessageLevel::UserWarning);
	}
	else
	{
		GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserWarning);
	}

}



#undef LOCTEXT_NAMESPACE

