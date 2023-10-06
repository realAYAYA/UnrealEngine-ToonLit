// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassVisualizationComponent.h"
#include "Logging/LogMacros.h"
#include "MassVisualizer.h"
#include "MassRepresentationTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/InstancedStaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MassInstancedStaticMeshComponent.h"
#include "VisualLogger/VisualLogger.h"
#include "Rendering/NaniteResources.h"

//---------------------------------------------------------------
// UMassVisualizationComponent
//---------------------------------------------------------------

namespace UE::MassRepresentation
{
	int32 GCallUpdateInstances = 1;
	FAutoConsoleVariableRef  CVarCallUpdateInstances(TEXT("Mass.CallUpdateInstances"), GCallUpdateInstances, TEXT("Toggle between UpdateInstances and BatchUpdateTransform."));
}  // UE::MassRepresentation

void UMassVisualizationComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject) == false && GetOuter())
	{
		ensureMsgf(GetOuter()->GetClass()->IsChildOf(AMassVisualizer::StaticClass()), TEXT("UMassVisualizationComponent should only be added to AMassVisualizer-like instances"));
	}
}

int16 UMassVisualizationComponent::FindOrAddVisualDesc(const FStaticMeshInstanceVisualizationDesc& Desc)
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	int32 VisualIndex = InstancedStaticMeshInfos.IndexOfByPredicate([&Desc](const FMassInstancedStaticMeshInfo& Info) { return Info.GetDesc() == Desc; });
	if (VisualIndex == INDEX_NONE)
	{
		VisualIndex = InstancedStaticMeshInfos.Emplace(Desc);

		for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Desc.Meshes)
		{
			ISMCSharedData.FindOrAdd(GetTypeHash(MeshDesc), FMassISMCSharedData());
		}

		BuildLODSignificanceForInfo(InstancedStaticMeshInfos[VisualIndex]);

		bNeedStaticMeshComponentConstruction = true;
	}
	check(VisualIndex < INT16_MAX);
	return (int16)VisualIndex;
}

void UMassVisualizationComponent::ConstructStaticMeshComponents()
{
	AActor* ActorOwner = GetOwner();
	check(ActorOwner);
	
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		// Check if it is already created
		if (!Info.InstancedStaticMeshComponents.IsEmpty())
		{
			continue;
		}

		// Check if there are any specified meshes for this visual type
		if(Info.Desc.Meshes.Num() == 0)
		{
			UE_LOG(LogMassRepresentation, Error, TEXT("No associated meshes for this intanced static mesh type"));
			continue;
		}
		for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
		{
			FMassISMCSharedData* SharedData = ISMCSharedData.Find(GetTypeHash(MeshDesc));
			UInstancedStaticMeshComponent* ISMC = SharedData ? SharedData->GetISMComponent() : nullptr;

			if (SharedData == nullptr || SharedData->GetISMComponent() == nullptr)
			{
				ISMC = NewObject<UInstancedStaticMeshComponent>(ActorOwner, MeshDesc.ISMComponentClass);
				CA_ASSUME(ISMC);
				REDIRECT_OBJECT_TO_VLOG(ISMC, this);

				ISMC->SetStaticMesh(MeshDesc.Mesh);
				for (int32 ElementIndex = 0; ElementIndex < MeshDesc.MaterialOverrides.Num(); ++ElementIndex)
				{
					if (UMaterialInterface* MaterialOverride = MeshDesc.MaterialOverrides[ElementIndex])
					{
						ISMC->SetMaterial(ElementIndex, MaterialOverride);
					}
				}
				ISMC->SetCullDistances(0, 1000000); // @todo: Need to figure out what to do here, either LOD or cull distances.
				ISMC->SetupAttachment(ActorOwner->GetRootComponent());
				ISMC->SetCanEverAffectNavigation(false);
				ISMC->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				ISMC->SetCastShadow(MeshDesc.bCastShadows);
				ISMC->Mobility = MeshDesc.Mobility;
				ISMC->SetReceivesDecals(false);
				ISMC->RegisterComponent();

				if (SharedData == nullptr)
				{
					SharedData = &ISMCSharedData.Emplace(GetTypeHash(MeshDesc), FMassISMCSharedData(ISMC));
				}
				else
				{
					SharedData->SetISMComponent(*ISMC);
				}
			}

			check(SharedData);
			Info.AddISMComponent(*SharedData);
		}

		// Build the LOD significance ranges
		if (Info.LODSignificanceRanges.Num() == 0)
		{
			BuildLODSignificanceForInfo(Info);
		}
	}
}

void UMassVisualizationComponent::BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info)
{
	TArray<float> AllLODSignificances;
	auto UniqueInsertOrdered = [&AllLODSignificances](const float Significance)
	{
		int i = 0;
		for (; i < AllLODSignificances.Num(); ++i)
		{
			// I did not use epsilon check here on purpose, because it will make it hard later meshes inside.
			if (Significance == AllLODSignificances[i])
			{
				return;
			}
			if (AllLODSignificances[i] > Significance)
			{
				break;
			}
		}
		AllLODSignificances.Insert(Significance, i);
	};
	for (const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
	{
		UniqueInsertOrdered(MeshDesc.MinLODSignificance);
		UniqueInsertOrdered(MeshDesc.MaxLODSignificance);
	}

	if (AllLODSignificances.Num() > 1)
	{
		Info.LODSignificanceRanges.SetNum(AllLODSignificances.Num() - 1);
		for (int i = 0; i < Info.LODSignificanceRanges.Num(); ++i)
		{
			FMassLODSignificanceRange& Range = Info.LODSignificanceRanges[i];
			Range.MinSignificance = AllLODSignificances[i];
			Range.MaxSignificance = AllLODSignificances[i+1];
			Range.ISMCSharedDataPtr = &ISMCSharedData;

			for (int j = 0; j < Info.Desc.Meshes.Num(); ++j)
			{
				const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Info.Desc.Meshes[j];
				const bool bAddMeshInRange = (Range.MinSignificance >= MeshDesc.MinLODSignificance && Range.MinSignificance < MeshDesc.MaxLODSignificance);
				if (bAddMeshInRange)
				{
					Range.StaticMeshRefs.Add(GetTypeHash(MeshDesc));
				}
			}
		}
	}
}

void UMassVisualizationComponent::ClearAllVisualInstances()
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		Info.ClearVisualInstance(ISMCSharedData);
	}
	InstancedStaticMeshInfos.Reset();
	
	// Pool should already be empty, got a problem if it's not
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = It.Value().GetISMComponent())
		{
			InstancedStaticMeshComponent->ClearInstances();
			InstancedStaticMeshComponent->DestroyComponent();
		}
	}

	ISMCSharedData.Reset();
}

void UMassVisualizationComponent::DirtyVisuals()
{
	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info : InstancedStaticMeshInfos)
	{
		for (UInstancedStaticMeshComponent* InstancedStaticMeshComponent : Info.InstancedStaticMeshComponents)
		{
			InstancedStaticMeshComponent->MarkRenderStateDirty();
		}
	}
}

void UMassVisualizationComponent::BeginVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent BeginVisualChanges")

	// Conditionally construct static mesh components
	if (bNeedStaticMeshComponentConstruction)
	{
		ConstructStaticMeshComponents();
		bNeedStaticMeshComponentConstruction = false;
	}
}

void UMassVisualizationComponent::EndVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent EndVisualChanges")

	// Batch update gathered instance transforms
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		FMassISMCSharedData& SharedData = It.Value();
		if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = SharedData.GetISMComponent())
		{
			if (UMassInstancedStaticMeshComponent* MassISMComponent = Cast<UMassInstancedStaticMeshComponent>(InstancedStaticMeshComponent))
			{
				MassISMComponent->ApplyVisualChanges(SharedData);
				continue;
			}

			const int32 NumCustomDataFloats = SharedData.StaticMeshInstanceCustomFloats.Num() / (FMath::Max(1, SharedData.UpdateInstanceIds.Num()));

			// Ensure InstanceCustomData is passed if NumCustomDataFloats > 0. If it is, also make sure
			// its length is NumCustomDataFloats * InstanceTransforms.Num()
			ensure(NumCustomDataFloats == 0 || (SharedData.StaticMeshInstanceCustomFloats.Num() == NumCustomDataFloats * SharedData.UpdateInstanceIds.Num()));

			// Resize PerInstanceSMData & PerInstanceSMCustomData to new (possibly culled or expanded) transform batch length
			const int32 NewNumInstances = SharedData.StaticMeshInstanceTransforms.Num();

			// Update PerInstanceSMData transforms
			if ((bool)UE::MassRepresentation::GCallUpdateInstances)
			{
				InstancedStaticMeshComponent->UpdateInstances(SharedData.UpdateInstanceIds, SharedData.StaticMeshInstanceTransforms, SharedData.StaticMeshInstancePrevTransforms, NumCustomDataFloats, SharedData.StaticMeshInstanceCustomFloats);
				if (UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(InstancedStaticMeshComponent))
				{
					HISMComp->BuildTreeIfOutdated(true, false);
				}
			}
			else
			{
				// Update NumCustomDataFloats
				InstancedStaticMeshComponent->NumCustomDataFloats = NumCustomDataFloats;
				if (NumCustomDataFloats > 0)
				{
					InstancedStaticMeshComponent->PerInstanceSMCustomData = SharedData.StaticMeshInstanceCustomFloats;
				}

				InstancedStaticMeshComponent->PerInstanceSMData.SetNum(NewNumInstances, /*bAllowShrinking*/false);
				InstancedStaticMeshComponent->PerInstancePrevTransform.SetNum(NewNumInstances, /*bAllowShrinking*/false);

				// Update PerInstanceSMData transforms
				InstancedStaticMeshComponent->BatchUpdateInstancesTransforms(/*StartInstanceIndex*/0, SharedData.StaticMeshInstanceTransforms, SharedData.StaticMeshInstancePrevTransforms, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/false);

				// Nanite ISMC? 
				TObjectPtr<UStaticMesh> StaticMeshObjectPtr = InstancedStaticMeshComponent->GetStaticMesh();
				FStaticMeshRenderData* StaticMeshRenderData = nullptr;
				if (UStaticMesh* StaticMesh = StaticMeshObjectPtr.Get())
				{
					StaticMeshRenderData = StaticMesh->GetRenderData();
				}
				const bool bNaniteISMC = UseNanite(InstancedStaticMeshComponent->GetScene()->GetShaderPlatform()) && StaticMeshRenderData && StaticMeshRenderData->HasValidNaniteData();
				if (bNaniteISMC)
				{
					// ISMC currently rebuilds PerInstanceRenderData regardless of whether it's using a 
					// Nanite::FSceneProxy which doesn't actually use this data. So to skip that we 
					// reset the InstanceUpdateCmdBuffer here after BatchUpdateInstancesTransforms has
					// marked it dirty but before CreateSceneProxy checks it
					// @todo This should be in ISMC code
					InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.Cmds.Reset();
					InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.NumAdds = 0;
					InstancedStaticMeshComponent->InstanceUpdateCmdBuffer.NumEdits = 0;
				}

				// Dirty render state
				InstancedStaticMeshComponent->MarkRenderStateDirty();
			}
		}
	}

	// Reset instance transform scratch buffers
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		FMassISMCSharedData& SharedData = It.Value();
		SharedData.ResetAccumulatedData();
	}
}

//---------------------------------------------------------------
// FMassInstancedStaticMeshInfo
//---------------------------------------------------------------

void FMassInstancedStaticMeshInfo::ClearVisualInstance(FMassISMCSharedDataMap& ISMCSharedData)
{
	for (int i = 0; i < Desc.Meshes.Num(); i++)
	{
		const uint32 MeshDescHash = GetTypeHash(Desc.Meshes[i]);
		FMassISMCSharedData* SharedData = ISMCSharedData.Find(MeshDescHash);
		if (SharedData && SharedData->ReleaseReference() == 0)
		{
			if (UInstancedStaticMeshComponent* ISMC = SharedData->GetISMComponent())
			{
				ISMC->ClearInstances();
				ISMC->DestroyComponent();
			}
			ISMCSharedData.Remove(MeshDescHash);
		}
	}

	InstancedStaticMeshComponents.Reset();
	LODSignificanceRanges.Reset();
}

//---------------------------------------------------------------
// FMassLODSignificanceRange
//---------------------------------------------------------------

void FMassLODSignificanceRange::AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
		{
			continue;
		}

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];

		SharedData.UpdateInstanceIds.Add(InstanceId);
		SharedData.StaticMeshInstanceTransforms.Add(Transform);
		SharedData.StaticMeshInstancePrevTransforms.Add(PrevTransform);
	}
}

void FMassLODSignificanceRange::AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
		{
			continue;
		}

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.StaticMeshInstanceCustomFloats.Append(CustomFloats);
	}
}

void FMassLODSignificanceRange::AddInstance(const int32 InstanceId, const FTransform& Transform)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.UpdateInstanceIds.Add(InstanceId);
		SharedData.StaticMeshInstanceTransforms.Add(Transform);
		SharedData.StaticMeshInstancePrevTransforms.Add(Transform);
	}
}

void FMassLODSignificanceRange::RemoveInstance(const int32 InstanceId)
{
	check(ISMCSharedDataPtr);
	for (int i = 0; i < StaticMeshRefs.Num(); i++)
	{
		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
		SharedData.RemoveInstanceIds.Add(InstanceId);
	}
}

void FMassLODSignificanceRange::WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const int32 FloatsPerInstance, const int32 StartFloatIndex, const TArray<uint32>& ExcludeStaticMeshRefs)
{
	check(ISMCSharedDataPtr);
	if (StaticMeshRefs.IsValidIndex(StaticMeshIndex))
	{
		if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[StaticMeshIndex]))
		{
			return;
		}

		FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[StaticMeshIndex]];

		int32 StartIndex = FloatsPerInstance * SharedData.WriteIterator + StartFloatIndex;

		ensure(SharedData.StaticMeshInstanceCustomFloats.Num() >= StartIndex + CustomFloats.Num());

		for (int CustomFloatIdx = 0; CustomFloatIdx < CustomFloats.Num(); CustomFloatIdx++)
		{
			SharedData.StaticMeshInstanceCustomFloats[StartIndex + CustomFloatIdx] = CustomFloats[CustomFloatIdx];
		}
		SharedData.WriteIterator++;
	}
}
