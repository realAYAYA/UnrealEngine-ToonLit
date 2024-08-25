// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/LODManagerTool.h"
#include "Drawing/LineSetComponent.h"
#include "InteractiveToolManager.h"

#include "AssetUtils/MeshDescriptionUtil.h"
#include "Drawing/PreviewGeometryActor.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "IndexedMeshToDynamicMesh.h"
#include "ModelingToolTargetUtil.h"

#include "PreviewMesh.h"
#include "StaticMeshAttributes.h"

// for lightmap access
#include "Components/StaticMeshComponent.h"


#include "StaticMeshResources.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "ToolContextInterfaces.h"
#include "ToolTargetManager.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"

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
	LODPreviewProperties->bShowingDefaultLOD = true;
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

	// test capture a dirty state that results from undo while in the tool.
	auto IsLODInfoDirty = [&]()
		{
			UStaticMesh* StaticMesh = GetSingleStaticMesh();
			if (!StaticMesh || !LODInfoProperties)
			{
				return false;
			}

			// have we changed the number of materials
			bool bDirty = (LODInfoProperties->Materials.Num() != StaticMesh->GetStaticMaterials().Num());

			// what about hi-res mesh?
			bDirty = bDirty || (StaticMesh->IsHiResMeshDescriptionValid() ? LODInfoProperties->HiResSource.Num() == 0 : LODInfoProperties->HiResSource.Num() != 0);

			return bDirty;
		};

	bLODInfoValid = bLODInfoValid && !IsLODInfoDirty();

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
	if (StaticMeshComponent == nullptr)
	{
		// We may have removed the highest quality LOD ( i.e hi-res ) during tool operation or restored it by doing an in-tool undo,
		// so here we reset the target to current max quality.  Why? Because if the tool target 'Editing LOD' does not correspond to an existing LOD on 
		// the mesh then target is considered invalid  and GetTargetComponent will fail as will showing the source object on shutdown.
		if (UStaticMeshComponentToolTarget* StaticMeshComponentTarget = Cast<UStaticMeshComponentToolTarget>(Targets[0]))
		{
			StaticMeshComponentTarget->SetEditingLOD(EMeshLODIdentifier::MaxQuality);

			StaticMeshComponent = Cast<UStaticMeshComponent>(UE::ToolTarget::GetTargetComponent(Targets[0]));
		}	
	}

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

	LODInfoProperties->bNaniteEnabled = StaticMesh->IsNaniteEnabled();
	LODInfoProperties->KeepTrianglePercent = StaticMesh->NaniteSettings.KeepPercentTriangles * 100;

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

	bPreviewLODValid = false;   
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

	auto SetShowingDefaultLOD = [this](bool bNewValue)
	{
		if (bNewValue != LODPreviewProperties->bShowingDefaultLOD)
		{
			LODPreviewProperties->bShowingDefaultLOD = bNewValue;
			if (bNewValue == true)
			{
				LODPreviewProperties->VisibleLOD = DefaultLODName;
			}
			NotifyOfPropertyChangeByTool(LODPreviewProperties);
		}
	};

	FString SelectedLOD = LODPreviewProperties->VisibleLOD;
	const FLODName* FoundName = ActiveLODNames.Find(SelectedLOD);
	if (SelectedLOD.IsEmpty() || FoundName == nullptr || FoundName->IsDefault() )
	{
		// currently just showing the source object when Default is selected
		LODPreview->SetVisible(false);
		ClearPreviewLines();
		LODPreviewLines->SetAllVisible(false);
		UE::ToolTarget::ShowSourceObject(Targets[0]);
		SetShowingDefaultLOD(true);
		return;
	}

	TUniquePtr<FLODMeshInfo>* Found = LODMeshCache.Find(SelectedLOD);
	if (Found == nullptr)
	{
		CacheLODMesh(SelectedLOD, *FoundName);
		Found = LODMeshCache.Find(SelectedLOD);
	}

	SetShowingDefaultLOD(Found == nullptr);
	
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

	const bool bComponentsHiddenByTool = LODPreview->IsVisible();
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
			
			if (bComponentsHiddenByTool)
			{ 
				// temporarily set the visible flag - that way an undo after the tool is closed will restore a visible mesh
				UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
			}

			StaticMesh->Modify();

			StaticMesh->ModifyHiResMeshDescription();
			StaticMesh->ClearHiResMeshDescription();
			StaticMesh->CommitHiResMeshDescription();

			StaticMesh->PostEditChange();

			// restore the visibility state expected by the tool
			if (bComponentsHiddenByTool)
			{
				UE::ToolTarget::HideSourceObject(Targets[ComponentIdx]);
			}
		}
	}

	GetToolManager()->EndUndoTransaction();

	LODMeshCache.Reset();
	bLODInfoValid = false;
}



void ULODManagerTool::MoveHiResToLOD0()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("RestoreLOD0", "Move HiRes to LOD0"));

	const bool bComponentsHiddenByTool = LODPreview->IsVisible();
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

			if (bComponentsHiddenByTool)
			{
				// temporarily set the visible flag - that way an undo after the tool is closed will restore a visible mesh
				UE::ToolTarget::ShowSourceObject(Targets[ComponentIdx]);
			}

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

			// restore the visibility state expected by the tool
			if (bComponentsHiddenByTool)
			{
				UE::ToolTarget::HideSourceObject(Targets[ComponentIdx]);
			}
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

		// using RenderData instead of Static Mesh Attributes because not every Source Model will have
		// a Mesh Description to inspect, which caused a crash
		FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		
		// for each mesh, collect list of material indices, and then set MatUsedFlags
		TArray<TArray<int32>> MeshMaterialIndices;
		MeshMaterialIndices.SetNum(NumMeshes);
		
		for (int32 lod = 0; lod < NumMeshes; ++lod)
		{
			if (RenderData && RenderData->LODResources.IsValidIndex(lod))
			{
				FStaticMeshLODResources& LOD = RenderData->LODResources[lod];
				int NumSections = LOD.Sections.Num();

				for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
				{
					// For every section in the LOD, retrieves the index of the material used
					FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(lod, SectionIndex);
					int32 MaterialIndex = Info.MaterialIndex;
					if (StaticMesh->GetStaticMaterials().IsValidIndex(MaterialIndex))
					{
						// material is used, set array value at its index to true
						MatUsedFlags[MaterialIndex] = true;
						MeshMaterialIndices[lod].AddUnique(MaterialIndex);
					}
				}
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
				FMeshDescription* Mesh = StaticMesh->GetMeshDescription(k);
				if (Mesh)
				{
					StaticMesh->ModifyMeshDescription(k);
					Mesh->RemapPolygonGroups(RemapPolygonGroups);
					StaticMesh->CommitMeshDescription(k);
				}
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

