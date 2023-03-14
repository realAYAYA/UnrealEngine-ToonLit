// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/LODManagerTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"

#include "ToolSetupUtil.h"
#include "MathUtil.h"
#include "AssetUtils/MeshDescriptionUtil.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "IndexedMeshToDynamicMesh.h"
#include "ModelingToolTargetUtil.h"

#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

// for lightmap access
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#include "Components/PrimitiveComponent.h"

#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "ToolTargetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LODManagerTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "ULODManagerTool"

/*
 * ToolBuilder
 */


const FToolTargetTypeRequirements& ULODManagerToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMaterialProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass(),
		UStaticMeshBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

bool ULODManagerToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	// disable multi-selection for now
	return SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1;
}

UMultiSelectionMeshEditingTool* ULODManagerToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<ULODManagerTool>(SceneState.ToolManager);
}




void ULODManagerActionPropertySet::PostAction(ELODManagerToolActions Action)
{
	if (ParentTool.IsValid() && Cast<ULODManagerTool>(ParentTool))
	{
		Cast<ULODManagerTool>(ParentTool)->RequestAction(Action);
	}
}




ULODManagerTool::ULODManagerTool()
{
}


void ULODManagerTool::Setup()
{
	UInteractiveTool::Setup();

	if (! ensure(Targets.Num() == 1) )
	{
		return;
	}


	LODPreviewProperties = NewObject<ULODManagerPreviewLODProperties>(this);
	AddToolPropertySource(LODPreviewProperties);
	LODPreviewProperties->VisibleLOD = this->DefaultLODName;
	LODPreviewProperties->WatchProperty(LODPreviewProperties->VisibleLOD, [this](FString NewLOD) { bPreviewLODValid = false; });
	LODPreviewProperties->WatchProperty(LODPreviewProperties->bShowSeams, [this](bool bNewValue) { bPreviewLODValid = false; });

	LODPreview = NewObject<UPreviewMesh>(this);
	LODPreview->CreateInWorld(GetTargetWorld(), (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Targets[0]));
	LODPreview->SetTangentsMode(EDynamicMeshComponentTangentsMode::ExternallyProvided);
	LODPreview->SetVisible(false);

	LODPreviewLines = NewObject<UPreviewGeometry>(this);
	LODPreviewLines->CreateInWorld(GetTargetWorld(), (FTransform)UE::ToolTarget::GetLocalToWorldTransform(Targets[0]));

	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Targets[0]);
	LODPreview->SetMaterials(MaterialSet.Materials);

	bLODInfoValid = false;

	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[0])))
	{
		if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			if (StaticMesh->IsHiResMeshDescriptionValid())
			{
				HiResSourceModelActions = NewObject<ULODManagerHiResSourceModelActions>(this);
				HiResSourceModelActions->Initialize(this);
				AddToolPropertySource(HiResSourceModelActions);
			}
		}
	}

	MaterialActions = NewObject<ULODManagerMaterialActions>(this);
	MaterialActions->Initialize(this);
	AddToolPropertySource(MaterialActions);

	LODInfoProperties = NewObject<ULODManagerLODProperties>(this);
	AddToolPropertySource(LODInfoProperties);
	bLODInfoValid = false;

	SetToolDisplayName(LOCTEXT("ToolName", "Manage LODs"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Inspect and Modify LODs of a StaticMesh Asset"),
		EToolMessageLevel::UserNotification);
}




void ULODManagerTool::OnShutdown(EToolShutdownType ShutdownType)
{
	if (LODPreview)
	{
		LODPreview->Disconnect();
		LODPreviewLines->Disconnect();
	}
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
	}
}



void ULODManagerTool::RequestAction(ELODManagerToolActions ActionType)
{
	if (PendingAction == ELODManagerToolActions::NoAction)
	{
		PendingAction = ActionType;
	}
}



void ULODManagerTool::OnTick(float DeltaTime)
{
	switch (PendingAction)
	{
	case ELODManagerToolActions::DeleteHiResSourceModel:
		DeleteHiResSourceModel();
		break;

	case ELODManagerToolActions::MoveHiResToLOD0:
		MoveHiResToLOD0();
		break;

	case ELODManagerToolActions::RemoveUnreferencedMaterials:
		RemoveUnreferencedMaterials();
		break;
	}
	PendingAction = ELODManagerToolActions::NoAction;

	if (bLODInfoValid == false)
	{
		UpdateLODInfo();
	}

	if (bPreviewLODValid == false)
	{
		UpdatePreviewLOD();
	}
}



UStaticMesh* ULODManagerTool::GetSingleStaticMesh()
{
	if (Targets.Num() > 1)
	{
		return nullptr;
	}
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[0]));
	if (StaticMeshComponent == nullptr || StaticMeshComponent->GetStaticMesh() == nullptr)
	{
		return nullptr;
	}
	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	return StaticMesh;
}







void ULODManagerTool::UpdateLODInfo()
{
	UStaticMesh* StaticMesh = GetSingleStaticMesh();
	if (bLODInfoValid == true || StaticMesh == nullptr)
	{
		return;
	}
	bLODInfoValid = true;

	LODInfoProperties->bNaniteEnabled = StaticMesh->NaniteSettings.bEnabled;
	LODInfoProperties->PercentTriangles = StaticMesh->NaniteSettings.FallbackPercentTriangles;

	TArray<FStaticMaterial> CurMaterialSet = StaticMesh->GetStaticMaterials();
	LODInfoProperties->Materials.Reset();
	for (const FStaticMaterial& Material : StaticMesh->GetStaticMaterials())
	{
		LODInfoProperties->Materials.Add(Material);
	}


	// per-LOD info
	LODInfoProperties->SourceLODs.Reset();
	int32 NumSourceModels = StaticMesh->GetNumSourceModels();
	for (int32 si = 0; si < NumSourceModels; ++si)
	{
		const FStaticMeshSourceModel& LODSourceModel = StaticMesh->GetSourceModel(si);
		const FMeshDescription* LODMesh = StaticMesh->GetMeshDescription(si);
		if (LODMesh != nullptr)		// generated LODs do not have mesh description...is that the only way to tell?
		{
			FLODManagerLODInfo LODInfo = { LODMesh->Vertices().Num(), LODMesh->Triangles().Num() };
			LODInfoProperties->SourceLODs.Add(LODInfo);
		}
	}

	LODInfoProperties->HiResSource.Reset();
	if (StaticMesh->IsHiResMeshDescriptionValid())
	{
		const FMeshDescription* HiResMesh = StaticMesh->GetHiResMeshDescription();
		FLODManagerLODInfo LODInfo = { HiResMesh->Vertices().Num(), HiResMesh->Triangles().Num() };
		LODInfoProperties->HiResSource.Add(LODInfo);
	}

	LODInfoProperties->RenderLODs.Reset();
	if (StaticMesh->HasValidRenderData())
	{
		const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		for (const FStaticMeshLODResources& LODResource : RenderData->LODResources)
		{
			FLODManagerLODInfo LODInfo;
			LODInfo.VertexCount = LODResource.VertexBuffers.PositionVertexBuffer.GetNumVertices();
			LODInfo.TriangleCount = LODResource.IndexBuffer.GetNumIndices() / 3;
			LODInfoProperties->RenderLODs.Add(LODInfo);
		}
	}


	UpdateLODNames();
}




void ULODManagerTool::UpdateLODNames()
{
	UStaticMesh* StaticMesh = GetSingleStaticMesh();
	if (StaticMesh == nullptr || LODPreviewProperties == nullptr)
	{
		return;
	}

	TArray<FString>& UILODNames = LODPreviewProperties->LODNamesList;
	UILODNames.Reset();
	ActiveLODNames.Reset();

	UILODNames.Add(DefaultLODName);
	ActiveLODNames.Add(DefaultLODName, FLODName());

	if (StaticMesh->IsHiResMeshDescriptionValid())
	{
		FString LODName(TEXT("SourceModel HiRes"));
		UILODNames.Add(LODName);
		ActiveLODNames.Add(LODName, { -1, -1, 0 });
	}

	int32 NumSourceModels = StaticMesh->GetNumSourceModels();
	for (int32 si = 0; si < NumSourceModels; ++si)
	{
		const FStaticMeshSourceModel& LODSourceModel = StaticMesh->GetSourceModel(si);
		const FMeshDescription* LODMesh = StaticMesh->GetMeshDescription(si);
		if (LODMesh != nullptr)		// generated LODs do not have mesh description...is that the only way to tell?
		{
			FString LODName = FString::Printf(TEXT("SourceModel LOD%d"), si);
			UILODNames.Add(LODName);
			ActiveLODNames.Add(LODName, { si, -1, -1 });
		}
	}

	if (StaticMesh->HasValidRenderData())
	{
		const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		int32 Index = 0;
		for (const FStaticMeshLODResources& LODResource : RenderData->LODResources)
		{
			FString LODName = FString::Printf(TEXT("RenderData LOD%d"), Index);
			UILODNames.Add(LODName);
			ActiveLODNames.Add(LODName, { -1, Index, -1 });
			Index++;
		}
	}


	bPreviewLODValid = false;    // should not always be necessary...
}





void ULODManagerTool::UpdatePreviewLines(FLODMeshInfo& MeshInfo)
{
	if (MeshInfo.bInfoCached == false)
	{
		MeshInfo.bInfoCached = true;
		for (int32 eid : MeshInfo.Mesh.EdgeIndicesItr())
		{
			if (MeshInfo.Mesh.IsBoundaryEdge(eid))
			{
				MeshInfo.BoundaryEdges.Add(eid);
			}
		}
	}

	float LineWidthMultiplier = 1.0f;
	FColor BoundaryEdgeColor(240, 15, 15);
	float BoundaryEdgeThickness = LineWidthMultiplier * 4.0f;
	float BoundaryEdgeDepthBias = 2.0f;

	ULineSetComponent* BoundaryLines = LODPreviewLines->FindLineSet(TEXT("BoundaryLines"));
	if (BoundaryLines == nullptr)
	{
		BoundaryLines = LODPreviewLines->AddLineSet(TEXT("BoundaryLines"));
	}
	if (BoundaryLines)
	{
		BoundaryLines->Clear();
		for (int32 eid : MeshInfo.BoundaryEdges)
		{
			FVector3d A, B;
			MeshInfo.Mesh.GetEdgeV(eid, A, B);
			BoundaryLines->AddLine((FVector)A, (FVector)B, BoundaryEdgeColor, BoundaryEdgeThickness, BoundaryEdgeDepthBias);
		}
	}
}


void ULODManagerTool::ClearPreviewLines()
{
	ULineSetComponent* BoundaryLines = LODPreviewLines->FindLineSet(TEXT("BoundaryLines"));
	if (BoundaryLines)
	{
		BoundaryLines->Clear();
	}
}


void ULODManagerTool::UpdatePreviewLOD()
{
	UStaticMesh* StaticMesh = GetSingleStaticMesh();
	if (StaticMesh == nullptr || LODPreviewProperties == nullptr || bPreviewLODValid == true)
	{
		return;
	}
	bPreviewLODValid = true;

	FString SelectedLOD = LODPreviewProperties->VisibleLOD;
	const FLODName* FoundName = ActiveLODNames.Find(SelectedLOD);
	if (SelectedLOD.IsEmpty() || FoundName == nullptr || FoundName->IsDefault() )
	{
		// currently just showing the source object when Default is selected
		LODPreview->SetVisible(false);
		ClearPreviewLines();
		LODPreviewLines->SetAllVisible(false);
		UE::ToolTarget::ShowSourceObject(Targets[0]);
		return;
	}

	TUniquePtr<FLODMeshInfo>* Found = LODMeshCache.Find(SelectedLOD);
	if (Found == nullptr)
	{
		CacheLODMesh(SelectedLOD, *FoundName);
		Found = LODMeshCache.Find(SelectedLOD);
	}
	
	if (Found)
	{
		UE::ToolTarget::HideSourceObject(Targets[0]);

		LODPreview->ReplaceMesh((*Found)->Mesh);
		LODPreview->SetVisible(true);
		UpdatePreviewLines(**Found);
		LODPreviewLines->SetAllVisible(LODPreviewProperties->bShowSeams);
	}
	else
	{
		LODPreview->SetVisible(false);
		ClearPreviewLines();
		LODPreviewLines->SetAllVisible(false);
		UE::ToolTarget::ShowSourceObject(Targets[0]);
	}
}




bool ULODManagerTool::CacheLODMesh(const FString& Name, FLODName LODName)
{
	UStaticMesh* StaticMesh = GetSingleStaticMesh();
	if (!ensure(StaticMesh)) return false;

	FMeshDescriptionToDynamicMesh Converter;

	if (LODName.OtherIndex == 0)		// HiRes SourceModel
	{
		FMeshDescription* HiResMeshDescription = StaticMesh->GetHiResMeshDescription();
		if (HiResMeshDescription != nullptr)
		{
			// TODO: we only need to do this copy if normals or tangents are set to auto-generated...
			FMeshBuildSettings HiResBuildSettings = StaticMesh->GetHiResSourceModel().BuildSettings;
			FMeshDescription TmpMeshDescription(*HiResMeshDescription);
			UE::MeshDescription::InitializeAutoGeneratedAttributes(TmpMeshDescription, &HiResBuildSettings);

			TUniquePtr<FLODMeshInfo> NewMeshInfo = MakeUnique<FLODMeshInfo>();
			Converter.Convert(&TmpMeshDescription, NewMeshInfo->Mesh, true);
			LODMeshCache.Add(Name, MoveTemp(NewMeshInfo));
			return true;
		}

	}
	else if (LODName.SourceModelIndex >= 0)
	{
		FMeshDescription* LODMeshDescription = StaticMesh->GetMeshDescription(LODName.SourceModelIndex);
		if (LODMeshDescription != nullptr)
		{
			// TODO: we only need to do this copy if normals or tangents are set to auto-generated...
			FMeshBuildSettings LODBuildSettings = StaticMesh->GetSourceModel(LODName.SourceModelIndex).BuildSettings;
			FMeshDescription TmpMeshDescription(*LODMeshDescription);
			UE::MeshDescription::InitializeAutoGeneratedAttributes(TmpMeshDescription, &LODBuildSettings);

			TUniquePtr<FLODMeshInfo> NewMeshInfo = MakeUnique<FLODMeshInfo>();
			Converter.Convert(&TmpMeshDescription, NewMeshInfo->Mesh, true);
			LODMeshCache.Add(Name, MoveTemp(NewMeshInfo));
			return true;
		}
	}
	else if (LODName.RenderDataIndex >= 0)
	{
		if (StaticMesh->HasValidRenderData())
		{
			const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
			if (LODName.RenderDataIndex < RenderData->LODResources.Num())
			{
				const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODName.RenderDataIndex];
				TUniquePtr<FLODMeshInfo> NewMeshInfo = MakeUnique<FLODMeshInfo>();
				UE::Conversion::RenderBuffersToDynamicMesh(LODResource.VertexBuffers, LODResource.IndexBuffer, LODResource.Sections, NewMeshInfo->Mesh);
				LODMeshCache.Add(Name, MoveTemp(NewMeshInfo));
				return true;
			}
		}
	}

	return false;
}



void ULODManagerTool::DeleteHiResSourceModel()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("DeleteHiResSourceModel", "Delete HiRes Source"));
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[ComponentIdx]));
		if (StaticMeshComponent == nullptr || StaticMeshComponent->GetStaticMesh() == nullptr)
		{
			continue;
		}
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

		if (StaticMesh->IsHiResMeshDescriptionValid())
		{
			StaticMesh->Modify();

			StaticMesh->ModifyHiResMeshDescription();
			StaticMesh->ClearHiResMeshDescription();
			StaticMesh->CommitHiResMeshDescription();

			StaticMesh->PostEditChange();
		}
	}

	GetToolManager()->EndUndoTransaction();

	LODMeshCache.Reset();
	bLODInfoValid = false;
}



void ULODManagerTool::MoveHiResToLOD0()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("RestoreLOD0", "Move HiRes to LOD0"));
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[ComponentIdx]));
		if (StaticMeshComponent == nullptr || StaticMeshComponent->GetStaticMesh() == nullptr)
		{
			continue;
		}
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

		if (StaticMesh->IsHiResMeshDescriptionValid())
		{
			StaticMesh->Modify();

			StaticMesh->ModifyHiResMeshDescription();
			FMeshDescription* HiResMeshDescription = StaticMesh->GetHiResMeshDescription();

			StaticMesh->ModifyMeshDescription(0);
			FMeshDescription* LOD0MeshDescription = StaticMesh->GetMeshDescription(0);
			*LOD0MeshDescription = *HiResMeshDescription;

			StaticMesh->ClearHiResMeshDescription();
			StaticMesh->CommitHiResMeshDescription();

			StaticMesh->CommitMeshDescription(0);

			StaticMesh->PostEditChange();
		}
	}

	GetToolManager()->EndUndoTransaction();

	LODMeshCache.Reset();
	bLODInfoValid = false;
}




void ULODManagerTool::RemoveUnreferencedMaterials()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("RemoveUnreferencedMaterials", "Remove Unreferenced Materials"));
	for (int32 ComponentIdx = 0; ComponentIdx < Targets.Num(); ComponentIdx++)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[ComponentIdx]));
		if (StaticMeshComponent == nullptr || StaticMeshComponent->GetStaticMesh() == nullptr)
		{
			continue;
		}
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

		TArray<FStaticMaterial> CurMaterialSet = StaticMesh->GetStaticMaterials();
		int32 NumMaterials = CurMaterialSet.Num();
		TArray<bool> MatUsedFlags;
		MatUsedFlags.Init(false, NumMaterials);

		TArray<const FMeshDescription*> Meshes;
		for (int32 k = 0; k < StaticMesh->GetNumSourceModels(); ++k)
		{
			if (StaticMesh->IsSourceModelValid(k))
			{
				Meshes.Add(StaticMesh->GetMeshDescription(k));
			}
		}
		if (StaticMesh->IsHiResMeshDescriptionValid())
		{
			Meshes.Add(StaticMesh->GetHiResMeshDescription());
		}
		int32 NumMeshes = Meshes.Num();


		// for each mesh, collect list of material indices, and then set MatUsedFlags
		TArray<TArray<int32>> MeshMaterialIndices;
		MeshMaterialIndices.SetNum(NumMeshes);
		for (int32 mi = 0; mi < NumMeshes; ++mi)
		{
			TMap<FPolygonGroupID, int32> PolygonGroupToSectionIndex;
			TMap<FPolygonGroupID, int32> PolygonGroupToMaterialIndex;
			FStaticMeshConstAttributes Attributes(*Meshes[mi]);
			TPolygonGroupAttributesConstRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
			for (FPolygonGroupID PolygonGroupID : Meshes[mi]->PolygonGroups().GetElementIDs() )
			{
				int32& SectionIndex = PolygonGroupToSectionIndex.FindOrAdd(PolygonGroupID);
				int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(PolygonGroupImportedMaterialSlotNames[PolygonGroupID]);
				if (MaterialIndex == INDEX_NONE)
				{
					MaterialIndex = PolygonGroupID.GetValue();
				}
				PolygonGroupToMaterialIndex.Add(PolygonGroupID, MaterialIndex);
			}

			for (const FTriangleID TriangleID : Meshes[mi]->Triangles().GetElementIDs())
			{
				const FPolygonGroupID PolygonGroupID = Meshes[mi]->GetTrianglePolygonGroup(TriangleID);
				const int32 MaterialIndex = PolygonGroupToMaterialIndex[PolygonGroupID];
				MatUsedFlags[MaterialIndex] = true;
				MeshMaterialIndices[mi].AddUnique(MaterialIndex);
			}
		}

		// construct new material set and ID remap
		TArray<FStaticMaterial> NewMaterialSet;
		TArray<int32> MaterialIDMap;
		TArray<int32> RemovedMaterials;
		TMap<FPolygonGroupID, FPolygonGroupID> RemapPolygonGroups;
		MaterialIDMap.Init(-1, NumMaterials);
		for (int32 k = 0; k < NumMaterials; ++k)
		{
			if (MatUsedFlags[k] == true )
			{
				MaterialIDMap[k] = NewMaterialSet.Num();
				RemapPolygonGroups.Add(FPolygonGroupID(k), FPolygonGroupID(MaterialIDMap[k]));
				NewMaterialSet.Add(CurMaterialSet[k]);
			}
			else
			{
				RemovedMaterials.Add(k);
			}
		}
		if (NewMaterialSet.Num() == CurMaterialSet.Num())
		{
			continue;
		}

		StaticMesh->Modify();

		// update material set
		StaticMesh->GetStaticMaterials() = NewMaterialSet;

		// now remap groups in each mesh
		int32 MeshIndex = 0;
		for (int32 k = 0; k < StaticMesh->GetNumSourceModels(); ++k)
		{
			if (StaticMesh->IsSourceModelValid(k))
			{
				StaticMesh->ModifyMeshDescription(k);
				FMeshDescription* Mesh = StaticMesh->GetMeshDescription(k);
				Mesh->RemapPolygonGroups(RemapPolygonGroups);
				StaticMesh->CommitMeshDescription(k);
			}
		}
		if (StaticMesh->IsHiResMeshDescriptionValid())
		{
			StaticMesh->ModifyHiResMeshDescription();
			FMeshDescription* HiResMeshDescription = StaticMesh->GetHiResMeshDescription();
			HiResMeshDescription->RemapPolygonGroups(RemapPolygonGroups);
			StaticMesh->CommitHiResMeshDescription();
		}

		StaticMesh->PostEditChange();
	}

	GetToolManager()->EndUndoTransaction();
	bLODInfoValid = false;
}

#undef LOCTEXT_NAMESPACE

