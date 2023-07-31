// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

#include "GeometryCollection/GeometryCollectionComponent.h"

FGeometryCollectionMeshGroup::FMeshId FGeometryCollectionMeshGroup::AddMesh(const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, const FGeometryCollectionMeshInfo& ISMInstanceInfo)
{
	FMeshId* MeshIndex = Meshes.Find(MeshInstance);
	if (MeshIndex)
	{
		return *MeshIndex;
	}

	const FMeshId MeshInfoIndex = MeshInfos.Emplace(ISMInstanceInfo);
	Meshes.Add(MeshInstance, MeshInfoIndex);
	return MeshInfoIndex;
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (MeshInfos.IsValidIndex(MeshId))
	{
		return ISMPool.BatchUpdateInstancesTransforms(MeshInfos[MeshId], StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid mesh Id (%d) for this mesh group"), MeshId);
	return false;
}

void FGeometryCollectionMeshGroup::RemoveAllMeshes(FGeometryCollectionISMPool& ISMPool)
{
	for (const FGeometryCollectionMeshInfo& MeshInfo: MeshInfos)
	{
		ISMPool.RemoveISM(MeshInfo);
	}
	MeshInfos.Empty();
	Meshes.Empty();
}

FGeometryCollectionISM::FGeometryCollectionISM(AActor* OwmingActor, const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	check(MeshInstance.StaticMesh);
	check(OwmingActor);

	const FName ISMName = MakeUniqueObjectName(OwmingActor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), MeshInstance.StaticMesh->GetFName());
	if (UInstancedStaticMeshComponent* ISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(OwmingActor, ISMName, RF_Transient | RF_DuplicateTransient))
	{
		ISMC->SetStaticMesh(MeshInstance.StaticMesh);
		// material overrides
		for (int32 MaterialIndex = 0; MaterialIndex < MeshInstance.MaterialsOverrides.Num(); MaterialIndex++)
		{
			ISMC->SetMaterial(MaterialIndex, MeshInstance.MaterialsOverrides[MaterialIndex]);
		}
		ISMC->SetCullDistances(0, 0);
		ISMC->SetCanEverAffectNavigation(false);
		ISMC->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		ISMC->SetCastShadow(true);
		ISMC->SetMobility(EComponentMobility::Stationary);
		OwmingActor->AddInstanceComponent(ISMC);
		ISMC->RegisterComponent();
		
		ISMComponent = ISMC;
	}
}

int32 FGeometryCollectionISM::AddInstanceGroup(int32 InstanceCount)
{
	const int32 InstanceGroupIndex = InstanceGroups.AddGroup(InstanceCount);
	const FInstanceGroups::FInstanceGroupRange& NewInstanceGroup = InstanceGroups.GetGroup(InstanceGroupIndex);
	const int32 TotalInstanceCount = NewInstanceGroup.Start + NewInstanceGroup.Count;
	ISMComponent->PreAllocateInstancesMemory(TotalInstanceCount);
	for (int32 InstanceIndex = NewInstanceGroup.Start; InstanceIndex < TotalInstanceCount; InstanceIndex++)
	{
		ISMComponent->AddInstance(FTransform::Identity, true);
	}
	return InstanceGroupIndex;
}

FGeometryCollectionMeshInfo FGeometryCollectionISMPool::AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount)
{
	FGeometryCollectionMeshInfo Info;

	FISMIndex* ISMIndex = MeshToISMIndex.Find(MeshInstance);
	if (!ISMIndex)
	{
		Info.ISMIndex = ISMs.Emplace(OwningComponent->GetOwner(), MeshInstance);
		MeshToISMIndex.Add(MeshInstance, Info.ISMIndex);
	}
	else
	{
		Info.ISMIndex = *ISMIndex;
	}
	// add to the ISM 
	Info.InstanceGroupIndex = ISMs[Info.ISMIndex].AddInstanceGroup(InstanceCount);
	return Info;
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GetGroup(MeshInfo.InstanceGroupIndex);
		ensure((StartInstanceIndex + NewInstancesTransforms.Num()) <= InstanceGroup.Count);

		const int32 StartIndex = InstanceGroup.Start + StartInstanceIndex;
		return ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid ISM Id (%d) when updating the transform "), MeshInfo.ISMIndex);
	return false;
}

void FGeometryCollectionISMPool::RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GetGroup(MeshInfo.InstanceGroupIndex);

		// todo: be able to remove a range of instance may be useful
		TArray<int32> InstancesToRemove;
		InstancesToRemove.Reserve(InstanceGroup.Count);
		const int32 StartIndex = InstanceGroup.Start;
		const int32 LastIndex = InstanceGroup.Start + InstanceGroup.Count;
		for (int32 InstanceIndex = StartIndex; InstanceIndex < LastIndex; InstanceIndex++)
		{
			InstancesToRemove.Add(InstanceIndex);
		}
		ISM.ISMComponent->RemoveInstances(InstancesToRemove);
		ISM.InstanceGroups.RemoveGroup(MeshInfo.InstanceGroupIndex);
	}
}

void FGeometryCollectionISMPool::Clear()
{
	MeshToISMIndex.Reset();
	if (ISMs.Num() > 0)
	{
		if (AActor* OwningActor = ISMs[0].ISMComponent->GetOwner())
		{
			for(FGeometryCollectionISM& ISM : ISMs)
			{
				ISM.ISMComponent->UnregisterComponent();
				ISM.ISMComponent->DestroyComponent();
				OwningActor->RemoveInstanceComponent(ISM.ISMComponent);
			}
		}
		ISMs.Reset();
	}
}

UGeometryCollectionISMPoolComponent::UGeometryCollectionISMPoolComponent(const FObjectInitializer& ObjectInitializer)
	: NextMeshGroupId(0)
{
}

UGeometryCollectionISMPoolComponent::FMeshGroupId  UGeometryCollectionISMPoolComponent::CreateMeshGroup()
{
	MeshGroups.Add(NextMeshGroupId);
	return NextMeshGroupId++;
}

void UGeometryCollectionISMPoolComponent::DestroyMeshGroup(FMeshGroupId MeshGroupId)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		MeshGroup->RemoveAllMeshes(Pool);
		MeshGroups.Remove(MeshGroupId);
	}
}

UGeometryCollectionISMPoolComponent::FMeshId UGeometryCollectionISMPoolComponent::AddMeshToGroup(FMeshGroupId MeshGroupId, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		const FGeometryCollectionMeshInfo ISMInstanceInfo = Pool.AddISM(this, MeshInstance, InstanceCount);
		return MeshGroup->AddMesh(MeshInstance, InstanceCount, ISMInstanceInfo);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to add a mesh to a mesh group (%d) that does not exists"), MeshGroupId);
	return INDEX_NONE;
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		return MeshGroup->BatchUpdateInstancesTransforms(Pool, MeshId, StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to update instance with mesh group (%d) that not exists"), MeshGroupId);
	return false;
}
