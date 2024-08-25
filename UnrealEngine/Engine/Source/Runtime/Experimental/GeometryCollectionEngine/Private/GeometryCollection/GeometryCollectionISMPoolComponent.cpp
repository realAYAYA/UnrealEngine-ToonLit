// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"

#include "ChaosLog.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionISMPoolComponent)

// Don't release ISM components when they empty, but keep them (and their scene proxy) alive.
// This can remove the high cost associated with repeated registration, scene proxy creation and mesh draw command creation.
static bool GComponentKeepAlive = true;
FAutoConsoleVariableRef CVarISMPoolComponentKeepAlive(
	TEXT("r.ISMPool.ComponentKeepAlive"),
	GComponentKeepAlive,
	TEXT("Keep ISM components alive when all their instances are removed."));

// Use a FreeList to enable recycling of ISM components.
// ISM components aren't unregistered, but their scene proxy is destroyed.
// When recycling a component, a new mesh description can be used.
// This removes the high CPU cost of unregister/register.
// But there is more CPU cost to recycling a component then to simply keeping it alive because scene proxy creation and mesh draw command caching isn't cheap.
// The component memory cost is kept bounded when compared to keeping components alive.
static bool GComponentRecycle = true;
FAutoConsoleVariableRef CVarISMPoolComponentRecycle(
	TEXT("r.ISMPool.ComponentRecycle"),
	GComponentRecycle,
	TEXT("Recycle ISM components to a free list for reuse when all their instances are removed."));

// Target free list size when recycling ISM components.
// We try to maintain a pool of free components for fast allocation, but want to clean up when numbers get too high.
static int32 GComponentFreeListTargetSize = 50;
FAutoConsoleVariableRef CVarISMPoolComponentFreeListTargetSize(
	TEXT("r.ISMPool.ComponentFreeListTargetSize"),
	GComponentFreeListTargetSize,
	TEXT("Target size for number of ISM components in the recycling free list."));

static bool GShadowCopyCustomData = false;
FAutoConsoleVariableRef CVarShadowCopyCustomData(
	TEXT("r.ISMPool.ShadowCopyCustomData"),
	GShadowCopyCustomData,
	TEXT("Keeps a copy of custom instance data so it can be restored if the instance is removed and readded."));


void FGeometryCollectionMeshInfo::ShadowCopyCustomData(int32 InstanceCount, int32 NumCustomDataFloatsPerInstance, TArrayView<const float> CustomDataFloats)
{
	CustomData.SetNum(InstanceCount * NumCustomDataFloatsPerInstance + NumCustomDataFloatsPerInstance);

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		int32 Offset = InstanceIndex * NumCustomDataFloatsPerInstance;
		FMemory::Memcpy(&CustomData[Offset], CustomDataFloats.GetData() + Offset, NumCustomDataFloatsPerInstance * CustomDataFloats.GetTypeSize());
	}
}

TArrayView<const float> FGeometryCollectionMeshInfo::CustomDataSlice(int32 InstanceIndex, int32 NumCustomDataFloatsPerInstance)
{
	TArrayView<const float> DataView = CustomData;
	return DataView.Slice(InstanceIndex * NumCustomDataFloatsPerInstance, NumCustomDataFloatsPerInstance);
}

FGeometryCollectionMeshGroup::FMeshId FGeometryCollectionMeshGroup::AddMesh(const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, const FGeometryCollectionMeshInfo& ISMInstanceInfo, TArrayView<const float> CustomDataFloats)
{
	const FMeshId MeshInfoIndex = MeshInfos.Emplace(ISMInstanceInfo);

	if (bAllowPerInstanceRemoval && GShadowCopyCustomData)
	{
		FGeometryCollectionMeshInfo& MeshInfo = MeshInfos[MeshInfoIndex];
		MeshInfo.ShadowCopyCustomData(InstanceCount, MeshInstance.Desc.NumCustomDataFloats, CustomDataFloats);
	}

	return MeshInfoIndex;
}

bool FGeometryCollectionMeshGroup::BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (MeshInfos.IsValidIndex(MeshId))
	{
		return ISMPool.BatchUpdateInstancesTransforms(MeshInfos[MeshId], StartInstanceIndex, NewInstancesTransforms, bWorldSpace, bMarkRenderStateDirty, bTeleport, bAllowPerInstanceRemoval);
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid mesh Id (%d) for this mesh group"), MeshId);
	return false;
}

void FGeometryCollectionMeshGroup::BatchUpdateInstanceCustomData(FGeometryCollectionISMPool& ISMPool, int32 CustomFloatIndex, float CustomFloatValue)
{
	for (const FGeometryCollectionMeshInfo& MeshInfo : MeshInfos)
	{
		ISMPool.BatchUpdateInstanceCustomData(MeshInfo, CustomFloatIndex, CustomFloatValue);
	}
}

void FGeometryCollectionMeshGroup::RemoveAllMeshes(FGeometryCollectionISMPool& ISMPool)
{
	for (const FGeometryCollectionMeshInfo& MeshInfo: MeshInfos)
	{
		ISMPool.RemoveInstancesFromISM(MeshInfo);
	}
	MeshInfos.Empty();
}

void FGeometryCollectionISM::CreateISM(AActor* InOwningActor)
{
	check(InOwningActor);

	ISMComponent = NewObject<UInstancedStaticMeshComponent>(InOwningActor, NAME_None, RF_Transient | RF_DuplicateTransient);

	ISMComponent->SetRemoveSwap();
	ISMComponent->SetCanEverAffectNavigation(false);
	ISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMComponent->SetupAttachment(InOwningActor->GetRootComponent());
	
	InOwningActor->AddInstanceComponent(ISMComponent);
	ISMComponent->RegisterComponent();
}

void FGeometryCollectionISM::InitISM(const FGeometryCollectionStaticMeshInstance& InMeshInstance, bool bKeepAlive, bool bOverrideTransformUpdates)
{
	MeshInstance = InMeshInstance;
	check(ISMComponent != nullptr);

	UStaticMesh* StaticMesh = MeshInstance.StaticMesh.Get();
	// We should only get here for valid static mesh objects.
	check(StaticMesh != nullptr);

#if WITH_EDITOR
	const FName ISMName = MakeUniqueObjectName(ISMComponent->GetOwner(), UInstancedStaticMeshComponent::StaticClass(), StaticMesh->GetFName());
	const FString ISMNameString = ISMName.ToString();
	ISMComponent->Rename(*ISMNameString);
#endif

	ISMComponent->bUseAttachParentBound = bOverrideTransformUpdates;
	ISMComponent->SetAbsolute(bOverrideTransformUpdates, bOverrideTransformUpdates, bOverrideTransformUpdates);

	ISMComponent->EmptyOverrideMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MeshInstance.MaterialsOverrides.Num(); MaterialIndex++)
	{
		UMaterialInterface* Material = MeshInstance.MaterialsOverrides[MaterialIndex].Get();
		// We should only get here for valid material objects.
		check(Material != nullptr);
		ISMComponent->SetMaterial(MaterialIndex, Material);
	}

	ISMComponent->SetStaticMesh(StaticMesh);
	ISMComponent->SetMobility((MeshInstance.Desc.Flags & FISMComponentDescription::StaticMobility) != 0 ? EComponentMobility::Static : EComponentMobility::Movable);

	ISMComponent->NumCustomDataFloats = MeshInstance.Desc.NumCustomDataFloats;
	for (int32 DataIndex = 0; DataIndex < MeshInstance.CustomPrimitiveData.Num(); DataIndex++)
	{
		ISMComponent->SetDefaultCustomPrimitiveDataFloat(DataIndex, MeshInstance.CustomPrimitiveData[DataIndex]);
	}

	const bool bReverseCulling = (MeshInstance.Desc.Flags & FISMComponentDescription::ReverseCulling) != 0;
	// Instead of reverse culling we put the mirror in the component transform so that PRIMITIVE_SCENE_DATA_FLAG_DETERMINANT_SIGN will be set for use by materials.
	//ISMComponent->SetReverseCulling(bReverseCulling);
	const FVector Scale = bReverseCulling ? FVector(-1, 1, 1) : FVector(1, 1, 1);

	if(bOverrideTransformUpdates)
	{
		FTransform TempTm = ISMComponent->GetAttachParent() ?
			ISMComponent->GetAttachParent()->GetComponentToWorld() :
			FTransform::Identity;

		// Apply above identified scale to the transform directly
		TempTm.SetScale3D(TempTm.GetScale3D() * Scale);

		ISMComponent->SetComponentToWorld(TempTm);
		ISMComponent->UpdateComponentTransform(EUpdateTransformFlags::None, ETeleportType::None);
		ISMComponent->MarkRenderTransformDirty();
	}
	else
	{
		const FTransform NewRelativeTransform(FQuat::Identity, MeshInstance.Desc.Position, Scale);

		if(!ISMComponent->GetRelativeTransform().Equals(NewRelativeTransform))
		{
			// If we're not overriding the transform and need a relative offset, apply that here
			ISMComponent->SetRelativeTransform(FTransform(FQuat::Identity, MeshInstance.Desc.Position, Scale));
		}
	}

	if ((MeshInstance.Desc.Flags & FISMComponentDescription::DistanceCullPrimitive) != 0)
	{
		ISMComponent->SetCachedMaxDrawDistance(MeshInstance.Desc.EndCullDistance);
	}

	ISMComponent->SetCullDistances(MeshInstance.Desc.StartCullDistance, MeshInstance.Desc.EndCullDistance);
	ISMComponent->SetCastShadow((MeshInstance.Desc.Flags & FISMComponentDescription::AffectShadow) != 0);
	ISMComponent->bAffectDynamicIndirectLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDynamicIndirectLighting) != 0;
	ISMComponent->bAffectDistanceFieldLighting = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectDistanceFieldLighting) != 0;
	ISMComponent->bCastFarShadow = (MeshInstance.Desc.Flags & FISMComponentDescription::AffectFarShadow) != 0;
	ISMComponent->bWorldPositionOffsetWritesVelocity = (MeshInstance.Desc.Flags & FISMComponentDescription::WorldPositionOffsetWritesVelocity) != 0;
	ISMComponent->bEvaluateWorldPositionOffset = (MeshInstance.Desc.Flags & FISMComponentDescription::EvaluateWorldPositionOffset) != 0;
	ISMComponent->bUseGpuLodSelection = (MeshInstance.Desc.Flags & FISMComponentDescription::GpuLodSelection) != 0;
	ISMComponent->bOverrideMinLOD = MeshInstance.Desc.MinLod > 0;
	ISMComponent->MinLOD = MeshInstance.Desc.MinLod;
	ISMComponent->SetLODDistanceScale(MeshInstance.Desc.LodScale);
	ISMComponent->SetUseConservativeBounds(true);
	ISMComponent->bComputeFastLocalBounds = true;
	ISMComponent->SetMeshDrawCommandStatsCategory(MeshInstance.Desc.StatsCategory);
	ISMComponent->ComponentTags = MeshInstance.Desc.Tags;
}

FInstanceGroups::FInstanceGroupId FGeometryCollectionISM::AddInstanceGroup(int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	// When adding new group it will always have a single range
	const FInstanceGroups::FInstanceGroupId InstanceGroupIndex = InstanceGroups.AddGroup(InstanceCount);
	const FInstanceGroups::FInstanceGroupRange& NewInstanceGroup = InstanceGroups.GroupRanges[InstanceGroupIndex];

	// Ensure that remapping arrays are big enough to hold any new items.
	InstanceIds.SetNum(InstanceGroups.GetMaxInstanceIndex(), EAllowShrinking::No);

	FTransform ZeroScaleTransform;
	ZeroScaleTransform.SetIdentityZeroScale();
	TArray<FTransform> ZeroScaleTransforms;
	ZeroScaleTransforms.Init(ZeroScaleTransform, InstanceCount);

	ISMComponent->PreAllocateInstancesMemory(InstanceCount);
	TArray<FPrimitiveInstanceId> AddedInstanceIds = ISMComponent->AddInstancesById(ZeroScaleTransforms, true, true);
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		InstanceIds[NewInstanceGroup.Start + InstanceIndex] = AddedInstanceIds[InstanceIndex];
	}

	// Set any custom data.
	if (CustomDataFloats.Num())
	{
		const int32 NumCustomDataFloats = ISMComponent->NumCustomDataFloats;
		if (ensure(NumCustomDataFloats * InstanceCount == CustomDataFloats.Num()))
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				ISMComponent->SetCustomDataById(AddedInstanceIds[InstanceIndex], CustomDataFloats.Slice(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats));
			}
		}
	}

	return InstanceGroupIndex;
}

FGeometryCollectionISMPool::FGeometryCollectionISMPool()
	: bCachedKeepAlive(GComponentKeepAlive)
	, bCachedRecycle(GComponentRecycle)
{
}

FGeometryCollectionISMPool::FISMIndex FGeometryCollectionISMPool::GetOrAddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, bool& bOutISMCreated)
{
	FISMIndex* ISMIndexPtr = MeshToISMIndex.Find(MeshInstance);
	if (ISMIndexPtr != nullptr)
	{
		bOutISMCreated = false;
		return *ISMIndexPtr;
	}

	// Take an ISM from the current FreeLists if available instead of allocating a new slot.
	FISMIndex ISMIndex = INDEX_NONE;
	if (FreeListISM.Num())
	{
		ISMIndex = FreeListISM.Last();
		FreeListISM.RemoveAt(FreeListISM.Num() - 1);
	}
	else if (FreeList.Num())
	{
		ISMIndex = FreeList.Last();
		FreeList.RemoveAt(FreeList.Num() - 1);
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner());
	}
	else
	{
		ISMIndex = ISMs.AddDefaulted();
		ISMs[ISMIndex].CreateISM(OwningComponent->GetOwner());
	}
	
	ISMs[ISMIndex].InitISM(MeshInstance, bCachedKeepAlive, bDisableBoundsAndTransformUpdate);
	
	bOutISMCreated = true;
	MeshToISMIndex.Add(MeshInstance, ISMIndex);
	return ISMIndex;
}

FGeometryCollectionMeshInfo FGeometryCollectionISMPool::AddInstancesToISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats)
{
	bool bISMCreated = false;
	FGeometryCollectionMeshInfo Info;
	Info.ISMIndex = GetOrAddISM(OwningComponent, MeshInstance, bISMCreated);
	Info.InstanceGroupIndex = ISMs[Info.ISMIndex].AddInstanceGroup(InstanceCount, CustomDataFloats);
	return Info;
}

bool FGeometryCollectionISMPool::BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport, bool bAllowPerInstanceRemoval)
{
	if (!ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Invalid ISM Id (%d) when updating the transform "), MeshInfo.ISMIndex);
		return false;
	}
		
	FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
	const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
	// If ISM component has identity transform (the common case) then we can skip world space to component space maths inside BatchUpdateInstancesTransforms()
	bWorldSpace &= !ISM.ISMComponent->GetComponentTransform().Equals(FTransform::Identity, 0.f);

	// The transform count should fit within the instance group.
	// Clamp it if it doesn't, but if we hit this ensure we need to investigate why.
	ensure(StartInstanceIndex + NewInstancesTransforms.Num() <= InstanceGroup.Count);
	const int32 NumTransforms = FMath::Min(NewInstancesTransforms.Num(), InstanceGroup.Count - StartInstanceIndex);

	// Loop over transforms.
	// todo: There may be some value in batching InstanceIds and caling one function for each of Add/Remove/Update.
	// However the ISM batched calls themselves seem to be just simple loops over the single instance calls, so probably no benefit.
	for (int InstanceIndex = StartInstanceIndex; InstanceIndex < StartInstanceIndex + NumTransforms; ++InstanceIndex)
	{
		FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + InstanceIndex];
		FTransform const& Transform = NewInstancesTransforms[InstanceIndex];

		if (bAllowPerInstanceRemoval)
		{
			if (Transform.GetScale3D().IsZero() && InstanceId.IsValid())
			{
				// Zero scale is used to indicate that we should remove the instance from the ISM.
				ISM.ISMComponent->RemoveInstanceById(InstanceId);
				ISM.InstanceIds[InstanceGroup.Start + InstanceIndex] = FPrimitiveInstanceId();
				continue;
			}
			else if (!Transform.GetScale3D().IsZero() && !InstanceId.IsValid())
			{
				// Re-add the instance to the ISM if the scale becomes non-zero.
				FPrimitiveInstanceId Id = ISM.ISMComponent->AddInstanceById(Transform, bWorldSpace);
				ISM.InstanceIds[InstanceGroup.Start + InstanceIndex] = Id;

				if (MeshInfo.CustomData.Num())
				{
					ISM.ISMComponent->SetCustomDataById(Id, MeshInfo.CustomDataSlice(InstanceIndex, ISM.ISMComponent->NumCustomDataFloats));
				}
				continue;
			}
		}

		if (InstanceId.IsValid())
		{
			ISM.ISMComponent->UpdateInstanceTransformById(InstanceId, Transform, bWorldSpace, bTeleport);
		}
	}

	return true;
}

void FGeometryCollectionISMPool::BatchUpdateInstanceCustomData(FGeometryCollectionMeshInfo const& MeshInfo, int32 CustomFloatIndex, float CustomFloatValue)
{
	if (!ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		return;
	}
	
	FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
	if (!ensure(CustomFloatIndex < ISM.MeshInstance.Desc.NumCustomDataFloats))
	{
		return;
	}
		
	const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceGroup.Count; ++InstanceIndex)
	{
		const FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + InstanceIndex];
		if (InstanceId.IsValid())
		{
			ISM.ISMComponent->SetCustomDataValueById(InstanceId, CustomFloatIndex, CustomFloatValue);
		}
	}
}

void FGeometryCollectionISMPool::RemoveInstancesFromISM(const FGeometryCollectionMeshInfo& MeshInfo)
{
	if (ISMs.IsValidIndex(MeshInfo.ISMIndex))
	{
		FGeometryCollectionISM& ISM = ISMs[MeshInfo.ISMIndex];
		const FInstanceGroups::FInstanceGroupRange& InstanceGroup = ISM.InstanceGroups.GroupRanges[MeshInfo.InstanceGroupIndex];
		
		for (int32 Index = 0; Index < InstanceGroup.Count; ++Index)
		{
			FPrimitiveInstanceId InstanceId = ISM.InstanceIds[InstanceGroup.Start + Index];
			if (InstanceId.IsValid())
			{
				// todo: Could RemoveInstanceByIds() instead as long as that function can handle skipping invalid InstanceIds.
				ISM.ISMComponent->RemoveInstanceById(InstanceId);
			}
		}
		
#if DO_CHECK
		// clear the IDs
		for (int32 Index = 0; Index < InstanceGroup.Count; ++Index)
		{
			ISM.InstanceIds[InstanceGroup.Start + Index] = FPrimitiveInstanceId();
		}
#endif
		ISM.InstanceGroups.RemoveGroup(MeshInfo.InstanceGroupIndex);
	
		if (ISM.InstanceGroups.IsEmpty())
		{
			ensure(ISM.ISMComponent->PerInstanceSMData.Num() == 0);

			// No live instances, so take opportunity to reset indexing.
			ISM.InstanceGroups.Reset();
			ISM.InstanceIds.Reset();

			RemoveISM(MeshInfo.ISMIndex, bCachedKeepAlive, bCachedRecycle);

			if (!bCachedKeepAlive)
			{
				MeshToISMIndex.Remove(ISM.MeshInstance);
			}
		}
	}
}

void FGeometryCollectionISMPool::RemoveISM(FISMIndex ISMIndex, bool bKeepAlive, bool bRecycle)
{
	FGeometryCollectionISM& ISM = ISMs[ISMIndex];
	ensure(ISM.InstanceGroups.IsEmpty());
	ensure(ISM.InstanceIds.IsEmpty());

	if (bKeepAlive)
	{
		// Nothing to do.
	}
	else if (bRecycle)
	{
		// Recycle to the free list.
#if WITH_EDITOR
		ISM.ISMComponent->Rename(nullptr);
#endif
		FreeListISM.Add(ISMIndex);
	}
	else
	{
		// Completely unregister and destroy the component and mark the ISM slot as free.
		ISM.ISMComponent->UnregisterComponent();
		ISM.ISMComponent->DestroyComponent();
		ISM.ISMComponent->GetOwner()->RemoveInstanceComponent(ISM.ISMComponent);
		ISM.ISMComponent = nullptr;
		
		FreeList.Add(ISMIndex);
	}
}

void FGeometryCollectionISMPool::Clear()
{
	MeshToISMIndex.Reset();
	PrellocationQueue.Reset();
	FreeList.Reset();
	FreeListISM.Reset();
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

void FGeometryCollectionISMPool::RequestPreallocateMeshInstance(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	// Preallocation only makes sense when we are keeping empty components alive.
	if (bCachedKeepAlive)
	{
		uint32 KeyHash = GetTypeHash(MeshInstance);
		if (MeshToISMIndex.FindByHash(KeyHash, MeshInstance) == nullptr)
		{
			PrellocationQueue.AddByHash(KeyHash, MeshInstance);
		}
	}
}

static bool AreWeakPointersValid(FGeometryCollectionStaticMeshInstance& InMeshInstance)
{
	if (!InMeshInstance.StaticMesh.IsValid())
	{
		return false;
	}

	for (TWeakObjectPtr<UMaterialInterface> Material : InMeshInstance.MaterialsOverrides)
	{
		if (!Material.IsValid())
		{
			return false;
		}
	}

	return true;
}

void FGeometryCollectionISMPool::ProcessPreallocationRequests(UGeometryCollectionISMPoolComponent* OwningComponent, int32 MaxPreallocations)
{
	int32 NumAdded = 0;
	for (TSet<FGeometryCollectionStaticMeshInstance>::TIterator It(PrellocationQueue); It; ++It)
	{
		bool bISMCreated = false;

		// Objects in the entries of the preallocation queue may no longer be loaded.
		if (AreWeakPointersValid(*It))
		{
			GetOrAddISM(OwningComponent, *It, bISMCreated);
		}

		It.RemoveCurrent();

		if (bISMCreated)
		{
			if (++NumAdded >= MaxPreallocations)
			{
				break;
			}
		}
	}
}

void FGeometryCollectionISMPool::UpdateAbsoluteTransforms(const FTransform& BaseTransform, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	for(const FGeometryCollectionISM& GcIsm : ISMs)
	{
		const bool bReverseCulling = (GcIsm.MeshInstance.Desc.Flags & FISMComponentDescription::ReverseCulling) != 0;
		check(GcIsm.MeshInstance.Desc.Position == FVector::ZeroVector);
		
		if(UInstancedStaticMeshComponent* Ism = GcIsm.ISMComponent)
		{
			if(bReverseCulling)
			{
				// As in InitISM we need to apply the inverted X scale for reverse culling.
				// Just copy the transform and set an inverted scale to apply to the ISM
				FVector BaseScale = BaseTransform.GetScale3D();
				BaseScale.X = -BaseScale.X;
				FTransform Flipped = BaseTransform;
				Flipped.SetScale3D(BaseScale);

				Ism->SetComponentToWorld(Flipped);
			}
			else
			{
				Ism->SetComponentToWorld(BaseTransform);
			}

			Ism->UpdateComponentTransform(UpdateTransformFlags | EUpdateTransformFlags::SkipPhysicsUpdate, Teleport);
			Ism->MarkRenderTransformDirty();
		}
	}
}

void FGeometryCollectionISMPool::Tick(UGeometryCollectionISMPoolComponent* OwningComponent)
{
	// Recache component lifecycle state from cvar.
	const bool bRemovedKeepAlive = bCachedKeepAlive && !GComponentKeepAlive;
	const bool bRemovedReycle = bCachedRecycle && !GComponentRecycle;
	bCachedKeepAlive = GComponentKeepAlive;
	bCachedRecycle = GComponentRecycle;

	// If we disabled keep alive behavior since last update then deal with the zombie components.
	if (bRemovedKeepAlive)
	{
		for (int32 ISMIndex = 0; ISMIndex < ISMs.Num(); ++ISMIndex)
		{
			FGeometryCollectionISM& ISM = ISMs[ISMIndex];
			if (ISM.ISMComponent && ISM.InstanceGroups.IsEmpty())
			{
				// Actually release the ISM.
				RemoveISM(ISMIndex, false, bCachedRecycle);
				MeshToISMIndex.Remove(ISM.MeshInstance);
			}
		}
	}

	// Process preallocation queue.
	if (!bCachedKeepAlive)
	{
		PrellocationQueue.Reset();
	}
	else if (!PrellocationQueue.IsEmpty())
	{
		// Preallocate components per tick until the queue is empty.
		const int32 PreallocateCountPerTick = 2;
		ProcessPreallocationRequests(OwningComponent, PreallocateCountPerTick);
	}

	if (FreeListISM.Num() > 0)
	{
		// Release components per tick until we reach minimum pool size.
		const int32 RemoveCountPerTick = 1;
		const int32 FreeListTargetSize = bRemovedReycle ? 0 : FMath::Max(FMath::Max(FreeListISM.Num() - RemoveCountPerTick, GComponentFreeListTargetSize), 0);
		while (FreeListISM.Num() > FreeListTargetSize)
		{
			const int32 ISMIndex = FreeListISM.Pop(EAllowShrinking::No);
			RemoveISM(ISMIndex, false, false);
		}
	}
}


UGeometryCollectionISMPoolComponent::UGeometryCollectionISMPoolComponent(const FObjectInitializer& ObjectInitializer)
	: NextMeshGroupId(0)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UGeometryCollectionISMPoolComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Pool.Tick(this);
}

UGeometryCollectionISMPoolComponent::FMeshGroupId  UGeometryCollectionISMPoolComponent::CreateMeshGroup(bool bAllowPerInstanceRemoval)
{
	FGeometryCollectionMeshGroup Group;
	Group.bAllowPerInstanceRemoval = bAllowPerInstanceRemoval;
	MeshGroups.Add(NextMeshGroupId, Group);
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
		const FGeometryCollectionMeshInfo ISMInstanceInfo = Pool.AddInstancesToISM(this, MeshInstance, InstanceCount, CustomDataFloats);
		return MeshGroup->AddMesh(MeshInstance, InstanceCount, ISMInstanceInfo, CustomDataFloats);
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

bool UGeometryCollectionISMPoolComponent::BatchUpdateInstanceCustomData(FMeshGroupId MeshGroupId, int32 CustomFloatIndex, float CustomFloatValue)
{
	if (FGeometryCollectionMeshGroup* MeshGroup = MeshGroups.Find(MeshGroupId))
	{
		MeshGroup->BatchUpdateInstanceCustomData(Pool, CustomFloatIndex, CustomFloatValue);
		return true;
	}
	UE_LOG(LogChaos, Warning, TEXT("UGeometryCollectionISMPoolComponent : Trying to update instance with mesh group (%d) that not exists"), MeshGroupId);
	return false;
}

void UGeometryCollectionISMPoolComponent::PreallocateMeshInstance(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	Pool.RequestPreallocateMeshInstance(MeshInstance);
}

void UGeometryCollectionISMPoolComponent::SetTickablePoolManagement(bool bEnablePoolManagement)
{
	if (!bEnablePoolManagement)
	{
		// Disable the keep alive and recycle pool management systems.
		// This also disables preallocation for this pool.
		Pool.bCachedKeepAlive = false;
		Pool.bCachedRecycle = false;
	}
	// Disable the Tick that is used to manage the pool.
	PrimaryComponentTick.SetTickFunctionEnable(bEnablePoolManagement);
}

void UGeometryCollectionISMPoolComponent::SetOverrideTransformUpdates(bool bOverrideUpdates)
{
	Pool.bDisableBoundsAndTransformUpdate = bOverrideUpdates;
}

void UGeometryCollectionISMPoolComponent::UpdateAbsoluteTransforms(const FTransform& BaseTransform, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Pool.UpdateAbsoluteTransforms(BaseTransform, UpdateTransformFlags, Teleport);
}

void UGeometryCollectionISMPoolComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	int32 SizeBytes =
		MeshGroups.GetAllocatedSize()
		+ Pool.MeshToISMIndex.GetAllocatedSize()
		+ Pool.ISMs.GetAllocatedSize()
		+ Pool.FreeList.GetAllocatedSize()
		+ Pool.FreeListISM.GetAllocatedSize();
	
	for (FGeometryCollectionISM ISM : Pool.ISMs)
	{
		SizeBytes += ISM.InstanceIds.GetAllocatedSize()
			+ ISM.InstanceGroups.GroupRanges.GetAllocatedSize()
			+ ISM.InstanceGroups.FreeList.GetAllocatedSize();
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SizeBytes);
}
