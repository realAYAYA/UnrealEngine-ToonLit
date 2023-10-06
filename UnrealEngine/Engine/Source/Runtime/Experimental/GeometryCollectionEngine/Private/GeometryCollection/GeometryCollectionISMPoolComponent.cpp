// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionISMPoolComponent)

// Using a FreeList forces the calling of UnregisterComponent which can be 
// slow if called many times in a frame. Disable for now, but can maybe enable
// if we defer and throttle UnregisterComponent calls.
static bool GUseComponentFreeList = false;
FAutoConsoleVariableRef CVarISMPoolUseComponentFreeList(
	TEXT("r.ISMPool.UseComponentFreeList"),
	GUseComponentFreeList,
	TEXT("Recycle ISM components in the Pool."));


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
	return BatchUpdateInstancesTransforms(ISMPool, MeshId, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
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

FGeometryCollectionISM::FGeometryCollectionISM(AActor* InOwningActor, const FGeometryCollectionStaticMeshInstance& InMeshInstance)
{
	MeshInstance = InMeshInstance;

	check(MeshInstance.StaticMesh);
	check(InOwningActor);

	UHierarchicalInstancedStaticMeshComponent* HISMC = nullptr;
	UInstancedStaticMeshComponent* ISMC = nullptr;
	
	if (MeshInstance.Desc.bUseHISM)
	{
		const FName ISMName = MakeUniqueObjectName(InOwningActor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), MeshInstance.StaticMesh->GetFName());
		ISMC = HISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(InOwningActor, ISMName, RF_Transient | RF_DuplicateTransient);
	}
	else
	{
		const FName ISMName = MakeUniqueObjectName(InOwningActor, UInstancedStaticMeshComponent::StaticClass(), MeshInstance.StaticMesh->GetFName());
		ISMC = NewObject<UInstancedStaticMeshComponent>(InOwningActor, ISMName, RF_Transient | RF_DuplicateTransient);
	}

	if (!ensure(ISMC != nullptr))
	{
		return;
	}

	ISMC->SetStaticMesh(MeshInstance.StaticMesh);
	for (int32 MaterialIndex = 0; MaterialIndex < MeshInstance.MaterialsOverrides.Num(); MaterialIndex++)
	{
		ISMC->SetMaterial(MaterialIndex, MeshInstance.MaterialsOverrides[MaterialIndex]);
	}
	for (int32 DataIndex = 0; DataIndex < MeshInstance.CustomPrimitiveData.Num(); DataIndex++)
	{
		ISMC->SetDefaultCustomPrimitiveDataFloat(DataIndex, MeshInstance.CustomPrimitiveData[DataIndex]);
	}

	ISMC->SetRemoveSwap();
	ISMC->NumCustomDataFloats = MeshInstance.Desc.NumCustomDataFloats;
	ISMC->SetReverseCulling(MeshInstance.Desc.bReverseCulling);
	ISMC->SetMobility(MeshInstance.Desc.bIsStaticMobility ? EComponentMobility::Static : EComponentMobility::Stationary);
	ISMC->SetCullDistances(MeshInstance.Desc.StartCullDistance, MeshInstance.Desc.EndCullDistance);
	ISMC->SetCastShadow(MeshInstance.Desc.bAffectShadow);
	ISMC->bAffectDynamicIndirectLighting = MeshInstance.Desc.bAffectDynamicIndirectLighting;
	ISMC->bAffectDistanceFieldLighting = MeshInstance.Desc.bAffectDistanceFieldLighting;
	ISMC->SetCanEverAffectNavigation(false);
	ISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMC->bOverrideMinLOD = MeshInstance.Desc.MinLod > 0;	
	ISMC->MinLOD = MeshInstance.Desc.MinLod;
	ISMC->ComponentTags.Append(MeshInstance.Desc.Tags);

	if (HISMC)
	{
		HISMC->SetLODDistanceScale(MeshInstance.Desc.LodScale);
	}

	InOwningActor->AddInstanceComponent(ISMC);
	ISMC->RegisterComponent();
	ISMC->SetVisibility(false);
	ISMComponent = ISMC;
}

FInstanceGroups::FInstanceGroupId FGeometryCollectionISM::AddInstanceGroup(int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	// When adding new group it will always have a single range
	const FInstanceGroups::FInstanceGroupId InstanceGroupIndex = InstanceGroups.AddGroup(InstanceCount);
	const FInstanceGroups::FInstanceGroupRange& NewInstanceGroup = InstanceGroups.GroupRanges[InstanceGroupIndex];

	FTransform ZeroScaleTransform;
	ZeroScaleTransform.SetIdentityZeroScale();
	TArray<FTransform> ZeroScaleTransforms;
	ZeroScaleTransforms.Init(ZeroScaleTransform, InstanceCount);

	ISMComponent->SetVisibility(true);
	ISMComponent->PreAllocateInstancesMemory(InstanceCount);
	TArray<int32> RenderInstances = ISMComponent->AddInstances(ZeroScaleTransforms, true, true);

	// Ensure that remapping arrays are big enough to hold any new items.
	InstanceIndexToRenderIndex.SetNum(InstanceGroups.GetMaxInstanceIndex(), false);
	RenderIndexToInstanceIndex.SetNum(ISMComponent->PerInstanceSMData.Num(), false);
	// Store mapping between our fixed instance index and the mutable ISM render index.
	// todo: Improve ISM API so that we don't need to pay the memory overhead here to manage this.
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		InstanceIndexToRenderIndex[NewInstanceGroup.Start + InstanceIndex] = RenderInstances[InstanceIndex];
		RenderIndexToInstanceIndex[RenderInstances[InstanceIndex]] = NewInstanceGroup.Start + InstanceIndex;
	}

	// Set any custom data.
	if (CustomDataFloats.Num())
	{
		const int32 NumCustomDataFloats = ISMComponent->NumCustomDataFloats;
		if (ensure(NumCustomDataFloats * InstanceCount == CustomDataFloats.Num()))
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				ISMComponent->SetCustomData(RenderInstances[InstanceIndex], CustomDataFloats.Slice(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats));
			}
		}
	}

	return InstanceGroupIndex;
}

FGeometryCollectionISMPool::FISMIndex FGeometryCollectionISMPool::AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	FISMIndex* ISMIndexPtr = MeshToISMIndex.Find(MeshInstance);
	if (ISMIndexPtr != nullptr)
	{
		return *ISMIndexPtr;
	}

	FISMIndex ISMIndex = INDEX_NONE;
	if (FreeList.Num())
	{
		// Take an ISM from the current FreeList instead of allocating a new slot.
		ISMIndex = FreeList.Last();
		FreeList.RemoveAt(FreeList.Num() - 1);
		ISMs[ISMIndex] = FGeometryCollectionISM(OwningComponent->GetOwner(), MeshInstance);
	}
	else
	{
		ISMIndex = ISMs.Emplace(OwningComponent->GetOwner(), MeshInstance);
	}
	
	MeshToISMIndex.Add(MeshInstance, ISMIndex);
	return ISMIndex;
}

FGeometryCollectionMeshInfo FGeometryCollectionISMPool::AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	FGeometryCollectionMeshInfo Info;
	Info.ISMIndex = AddISM(OwningComponent, MeshInstance);
	Info.InstanceGroupIndex = ISMs[Info.ISMIndex].AddInstanceGroup(InstanceCount, CustomDataFloats);
	return Info;
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	return BatchUpdateInstancesTransforms(MeshInfo, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	constexpr bool bUseArrayView = true;

	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		ensure((StartInstanceIndex + NewInstancesTransforms.Num()) <= InstanceGroup.Count);

		// If ISM component has identity transform (the common case) then we can skip world space to component space maths inside BatchUpdateInstancesTransforms()
		bWorldSpace &= !ISM.ISMComponent->GetComponentTransform().Equals(FTransform::Identity, 0.f);

		int32 StartIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start];
		int32 TransformIndex = 0;
		int32 BatchCount = 1;

		if constexpr (bUseArrayView)
		{
			for (int InstanceIndex = StartInstanceIndex + 1; InstanceIndex < NewInstancesTransforms.Num(); ++InstanceIndex)
			{
				// Flush batch for non-sequential instances.
				int32 RenderIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
				if (RenderIndex != (StartIndex + BatchCount))
				{
					TArrayView<const FTransform> BatchedTransformsView = MakeArrayView(NewInstancesTransforms.GetData() + TransformIndex, BatchCount);
					ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchedTransformsView, bWorldSpace, bMarkRenderStateDirty, bTeleport);
					StartIndex = RenderIndex;
					TransformIndex += BatchCount;
					BatchCount = 0;
				}
				BatchCount++;
			}

			// last one
			TArrayView<const FTransform> BatchedTransformsView = MakeArrayView(NewInstancesTransforms.GetData() + TransformIndex, BatchCount);
			return ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchedTransformsView, bWorldSpace, bMarkRenderStateDirty, bTeleport);
		}
		else
		{
			TArray<FTransform> BatchTransforms; // Can't use TArrayView because blueprint function doesn't support that 
			BatchTransforms.Reserve(NewInstancesTransforms.Num());
			BatchTransforms.Add(NewInstancesTransforms[TransformIndex++]);
			for (int InstanceIndex = StartInstanceIndex + 1; InstanceIndex < NewInstancesTransforms.Num(); ++InstanceIndex)
			{
				// Flush batch for non-sequential instances.
				int32 RenderIndex = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
				if (RenderIndex != (StartIndex + BatchCount))
				{
					ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
					StartIndex = RenderIndex;
					BatchTransforms.SetNum(0, false);
					BatchCount = 0;
				}

				BatchTransforms.Add(NewInstancesTransforms[TransformIndex++]);
				BatchCount++;
			}

			return ISM.ISMComponent->BatchUpdateInstancesTransforms(StartIndex, BatchTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
		}
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid ISM Id (%d) when updating the transform "), MeshInfo.ISMIndex);
	return false;
}

void FGeometryCollectionISMPool::RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		
		TArray<int32> InstancesToRemove;
		InstancesToRemove.SetNum(InstanceGroup.Count);
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceGroup.Count; ++InstanceIndex)
		{
			// We need render index to pass to the ISMComponent.
			InstancesToRemove[InstanceIndex] = ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex];
			// Clear the stored render index since we're about to remove it.
			ISM.InstanceIndexToRenderIndex[InstanceGroup.Start + InstanceIndex] = -1;
		}

		// we sort the array on the spot because we use it after calling RemoveInstances to fix up our own indices
		InstancesToRemove.Sort(TGreater<int32>());
		constexpr bool bArrayAlreadySorted = true;
		ISM.ISMComponent->RemoveInstances(InstancesToRemove, bArrayAlreadySorted);

		// Fix up instance index remapping to match what will have happened in our ISM component in RemoveInstances()
		check(ISM.ISMComponent->SupportsRemoveSwap());
		for (int32 RenderIndex : InstancesToRemove)
		{
			ISM.RenderIndexToInstanceIndex.RemoveAtSwap(RenderIndex, 1, false);
			if (RenderIndex < ISM.RenderIndexToInstanceIndex.Num())
			{
				const int32 MovedInstanceIndex = ISM.RenderIndexToInstanceIndex[RenderIndex];
				ISM.InstanceIndexToRenderIndex[MovedInstanceIndex] = RenderIndex;
			}
		}

		ISM.InstanceGroups.RemoveGroup(MeshInfo.InstanceGroupIndex);
	
		if (ISM.InstanceGroups.IsEmpty())
		{
			// No live instances, so take opportunity to reset indexing.
			ISM.InstanceGroups.Reset();
			ISM.InstanceIndexToRenderIndex.Reset();
			ISM.RenderIndexToInstanceIndex.Reset();
			ISM.ISMComponent->SetVisibility(false);
		}

		if (GUseComponentFreeList && ISM.ISMComponent->PerInstanceSMData.Num() == 0)
		{
			// Remove component and push this ISM slot to the free list.
			// todo: profile if it is better to push component into a free pool and recycle it.
			ISM.ISMComponent->GetOwner()->RemoveInstanceComponent(ISM.ISMComponent);
			ISM.ISMComponent->UnregisterComponent();
			ISM.ISMComponent->DestroyComponent();
			
			MeshToISMIndex.Remove(ISM.MeshInstance);
			FreeList.Add(MeshInfo.ISMIndex);
			ISM.ISMComponent = nullptr;
		}
	}
}

void FGeometryCollectionISMPool::Clear()
{
	MeshToISMIndex.Reset();
	FreeList.Reset();
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

UGeometryCollectionISMPoolComponent::FMeshId UGeometryCollectionISMPoolComponent::AddMeshToGroup(FMeshGroupId MeshGroupId, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		const FGeometryCollectionMeshInfo ISMInstanceInfo = Pool.AddISM(this, MeshInstance, InstanceCount, CustomDataFloats);
		return MeshGroup->AddMesh(MeshInstance, InstanceCount, ISMInstanceInfo);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to add a mesh to a mesh group (%d) that does not exists"), MeshGroupId);
	return INDEX_NONE;
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	return BatchUpdateInstancesTransforms(MeshGroupId, MeshId, StartInstanceIndex, MakeArrayView(NewInstancesTransforms), bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		return MeshGroup->BatchUpdateInstancesTransforms(Pool, MeshId, StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to update instance with mesh group (%d) that not exists"), MeshGroupId);
	return false;
}

void UGeometryCollectionISMPoolComponent::PreallocateMeshInstance(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	// If we are recycling components with a free list then we don't expect to have zero instance components.
	// So don't do preallocation of components either in that case.
	if (!GUseComponentFreeList)
	{
		Pool.AddISM(this, MeshInstance);
	}
}

void UGeometryCollectionISMPoolComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	int32 SizeBytes =
		MeshGroups.GetAllocatedSize()
		+ Pool.MeshToISMIndex.GetAllocatedSize()
		+ Pool.ISMs.GetAllocatedSize()
		+ Pool.FreeList.GetAllocatedSize();
	
	for (FGeometryCollectionISM ISM : Pool.ISMs)
	{
		SizeBytes += ISM.InstanceIndexToRenderIndex.GetAllocatedSize()
			+ ISM.RenderIndexToInstanceIndex.GetAllocatedSize()
			+ ISM.InstanceGroups.GroupRanges.GetAllocatedSize()
			+ ISM.InstanceGroups.FreeList.GetAllocatedSize();
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SizeBytes);
}
