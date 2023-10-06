// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Containers/Map.h"
#include "InstancedStaticMeshDelegates.h"
#include "Materials/MaterialInterface.h"

#include "GeometryCollectionISMPoolComponent.generated.h"

class AActor;
class UGeometryCollectionISMPoolComponent;
class UInstancedStaticMeshComponent;

/** 
 * Structure containing a set of allocated instance ranges in an FGeometryCollectionISM which is the manager for a single ISM component.
 * The instance ranges don't change once allocated, and aren't the same as the actual render indices in the ISM.
 * The reason that we don't store the the actual ISM render indices is that ISM component is free to reorder its instances whenever it likes.
 */
struct FInstanceGroups
{
	using FInstanceGroupId = int32;

	/** A single continuous range associated with an FInstanceGroupId. */
	struct FInstanceGroupRange
	{
		FInstanceGroupRange(int32 InStart, int32 InCount)
			: Start(InStart)
			, Count(InCount)
		{
		}

		int32 Start = 0;
		int32 Count = 0;
	};

	/** Reset all contents. */
	void Reset()
	{
		TotalInstanceCount = 0;
		TotalFreeInstanceCount = 0;
		GroupRanges.Empty();
		FreeList.Empty();
	}

	/** Returns true if no groups ranges are in use. */
	bool IsEmpty() const
	{
		return GroupRanges.Num() == FreeList.Num();
	}

	/** Returns the maximum instance index that we have allocated in a group. */
	int32 GetMaxInstanceIndex() const
	{
		return TotalInstanceCount;
	}

	/** Add a new group range. */
	FInstanceGroupId AddGroup(int32 Count)
	{
		// First check to see if we can recycle a group from the free list.
		for (int32 Index = 0; Index < FreeList.Num(); ++Index)
		{
			const FInstanceGroupId GroupId = FreeList[Index];
			// todo: Could also allow allocating a subrange if Count is less than the group range count.
			if (Count == GroupRanges[GroupId].Count)
			{
				TotalFreeInstanceCount -= Count;
				FreeList.RemoveAtSwap(Index, 1, false);
				return GroupId;
			}
		}
		
		// Create a new group.
		const int32 StartIndex = TotalInstanceCount;
		TotalInstanceCount += Count;
		const FInstanceGroupId GroupId = GroupRanges.Add(FInstanceGroupRange(StartIndex, Count));
		return GroupId;
	}

	/** Remove a group range. */
	void RemoveGroup(FInstanceGroupId GroupId)
	{
		// Remove the group by putting on free list for reuse.
		// Actually removing it would require too much shuffling of the render instance index remapping.
		TotalFreeInstanceCount += GroupRanges[GroupId].Count;
		FreeList.Add(GroupId);
	}

	int32 TotalInstanceCount = 0;
	int32 TotalFreeInstanceCount = 0;
	TArray<FInstanceGroupRange> GroupRanges;
	TArray<FInstanceGroupId> FreeList;
};

/** 
 * A description for an ISM component.
 */
struct FISMComponentDescription
{
	bool bUseHISM = false;
	bool bReverseCulling = false;
	bool bIsStaticMobility = false;
	bool bAffectShadow = true;
	bool bAffectDistanceFieldLighting = false;
	bool bAffectDynamicIndirectLighting = false;
	int32 NumCustomDataFloats = 0;
	int32 StartCullDistance = 0;
	int32 EndCullDistance = 0;
	int32 MinLod = 0;
	float LodScale = 1.f;
	TArray<FName> Tags;

	bool operator==(const FISMComponentDescription& Other) const
	{
		return bUseHISM == Other.bUseHISM &&
			bReverseCulling == Other.bReverseCulling &&
			bIsStaticMobility == Other.bIsStaticMobility &&
			bAffectShadow == Other.bAffectShadow &&
			bAffectDistanceFieldLighting == Other.bAffectDistanceFieldLighting &&
			bAffectDynamicIndirectLighting == Other.bAffectDistanceFieldLighting &&
			NumCustomDataFloats == Other.NumCustomDataFloats &&
			StartCullDistance == Other.StartCullDistance && 
			EndCullDistance == Other.EndCullDistance &&
			MinLod == Other.MinLod &&
			LodScale == Other.LodScale &&
			Tags == Other.Tags;
	}
};

FORCEINLINE uint32 GetTypeHash(const FISMComponentDescription& Desc)
{
	const uint32 PackedBools = 
		(Desc.bUseHISM ? 1 : 0) | 
		(Desc.bReverseCulling ? 2 : 0) | 
		(Desc.bIsStaticMobility ? 4 : 0) | 
		(Desc.bAffectShadow ? 8 : 0) | 
		(Desc.bAffectDistanceFieldLighting ? 16 : 0) |
		(Desc.bAffectDynamicIndirectLighting ? 32 : 0);
	uint32 Hash = HashCombineFast(GetTypeHash(PackedBools), GetTypeHash(Desc.NumCustomDataFloats));
	Hash = HashCombineFast(Hash, GetTypeHash(Desc.StartCullDistance));
	Hash = HashCombineFast(Hash, GetTypeHash(Desc.EndCullDistance));
	Hash = HashCombineFast(Hash, GetTypeHash(Desc.MinLod));
	Hash = HashCombineFast(Hash, GetTypeHash(Desc.LodScale));
	return HashCombineFast(Hash, GetArrayHash(Desc.Tags.GetData(), Desc.Tags.Num()));
}

/**
 * A mesh with potentially overriden materials and ISM property description.
 * We batch instances into ISMs that have equivalent values for this structure.
 */
struct FGeometryCollectionStaticMeshInstance
{
	UStaticMesh* StaticMesh = nullptr;
	TArray<UMaterialInterface*> MaterialsOverrides;
	TArray<float> CustomPrimitiveData;
	FISMComponentDescription Desc;

	bool operator==(const FGeometryCollectionStaticMeshInstance& Other) const 
	{
		if (StaticMesh != Other.StaticMesh || !(Desc == Other.Desc))
		{
			return false;
		}
		if (MaterialsOverrides.Num() != Other.MaterialsOverrides.Num())
		{
			return false;
		}
		for (int32 MatIndex = 0; MatIndex < MaterialsOverrides.Num(); MatIndex++)
		{
			const FName MatName = MaterialsOverrides[MatIndex] ? MaterialsOverrides[MatIndex]->GetFName() : NAME_None;
			const FName OtherName = Other.MaterialsOverrides[MatIndex] ? Other.MaterialsOverrides[MatIndex]->GetFName() : NAME_None;
			if (MatName != OtherName)
			{
				return false;
			}
		}
		if (CustomPrimitiveData.Num() != Other.CustomPrimitiveData.Num())
		{
			return false;
		}
		for (int32 DataIndex = 0; DataIndex < CustomPrimitiveData.Num(); DataIndex++)
		{
			if (CustomPrimitiveData[DataIndex] != Other.CustomPrimitiveData[DataIndex])
			{
				return false;
			}
		}

		return true;
	}
};

FORCEINLINE uint32 GetTypeHash(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	uint32 CombinedHash = GetTypeHash(MeshInstance.StaticMesh);
	CombinedHash = HashCombineFast(CombinedHash, GetTypeHash(MeshInstance.MaterialsOverrides.Num()));
	for (const UMaterialInterface* Material: MeshInstance.MaterialsOverrides)
	{
		CombinedHash = HashCombineFast(CombinedHash, GetTypeHash(Material));
	}
	for (const float CustomFloat : MeshInstance.CustomPrimitiveData)
	{
		CombinedHash = HashCombineFast(CombinedHash, GetTypeHash(CustomFloat));
	}
	CombinedHash = HashCombineFast(CombinedHash, GetTypeHash(MeshInstance.Desc));
	return CombinedHash;
}

/** Describes a group of instances within an ISM. */
struct FGeometryCollectionMeshInfo
{
	int32 ISMIndex;
	FInstanceGroups::FInstanceGroupId InstanceGroupIndex;
};

struct FGeometryCollectionISMPool;

/**
 * A mesh group which is a collection of meshes and their related FGeometryCollectionMeshInfo.
 * We group these with a single handle with the expectation that a client will want to own multiple meshs and release them together.
 */
struct FGeometryCollectionMeshGroup
{
	using FMeshId = int32;

	/** Adds a new mesh with instance count. We expect to only add a unique mesh instance once to each group. Returns a ID that can be used to update the instances. */
	FMeshId AddMesh(const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, const FGeometryCollectionMeshInfo& ISMInstanceInfo);
	/** Update instance transforms for a group of instances. */
	bool BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);
	bool BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);

	/** Remove all of our managed meshes and associated instances. */
	void RemoveAllMeshes(FGeometryCollectionISMPool& ISMPool);

	/** Array of allocated mesh infos. */
	TArray<FGeometryCollectionMeshInfo> MeshInfos;
	/** Map from mesh instance description to the its index in the MeshInfo array. */
	TMap<FGeometryCollectionStaticMeshInstance, FMeshId> Meshes;
};

/** Structure containting all info for a single ISM. */
struct FGeometryCollectionISM
{
	FGeometryCollectionISM(AActor* OwmingActor, const FGeometryCollectionStaticMeshInstance& InMeshInstance);

	/** Add a group to the ISM. Returns the group index. */
	FInstanceGroups::FInstanceGroupId AddInstanceGroup(int32 InstanceCount, TArrayView<const float> CustomDataFloats);

	/** Unique description of ISM component settings. */
	FGeometryCollectionStaticMeshInstance MeshInstance;
	/** Created ISM component. Will be nullptr when this slot has been recycled to FGeometryCollectionISMPool FreeList. */
	TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;
	/** Groups of instances allocated in the ISM. */
	FInstanceGroups InstanceGroups;
	/** Mapping from our instance index to the ISM Component index. */
	TArray<int32> InstanceIndexToRenderIndex;
	/** Mapping from the ISM Component index to our instance index . */
	TArray<int32> RenderIndexToInstanceIndex;
};

/** A pool of ISMs. */
struct FGeometryCollectionISMPool
{
	using FISMIndex = int32;

	FISMIndex AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance);
	FGeometryCollectionMeshInfo AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats);

	bool BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);

	bool BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);

	void RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo);
	
	/** Clear all ISM components and associated data */
	void Clear();

	TMap<FGeometryCollectionStaticMeshInstance, FISMIndex> MeshToISMIndex;
	TArray<FGeometryCollectionISM> ISMs;
	TArray<int32> FreeList;
};


/**
* UGeometryCollectionISMPoolComponent.
* Component that manages a pool of ISM components in order to allow multiple client components that use the same meshes to the share ISMs.
*/
UCLASS(meta = (BlueprintSpawnableComponent), MinimalAPI)
class UGeometryCollectionISMPoolComponent: public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	using FMeshGroupId = int32;
	using FMeshId = int32;

	//~ Begin UActorComponent Interface
	GEOMETRYCOLLECTIONENGINE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UActorComponent Interface

	/** 
	* Create an Mesh group which represent an arbitrary set of mesh with their instance 
	* no resources are created until the meshes are added for this group 
	* return a mesh group Id used to add and update instances
	*/
	GEOMETRYCOLLECTIONENGINE_API FMeshGroupId CreateMeshGroup();

	/** destroy  a mesh group and its associated resources */
	GEOMETRYCOLLECTIONENGINE_API void DestroyMeshGroup(FMeshGroupId MeshGroupId);

	/** Add a static mesh for a mesh group */
	GEOMETRYCOLLECTIONENGINE_API FMeshId AddMeshToGroup(FMeshGroupId MeshGroupId, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, TArrayView<const float> CustomDataFloats);

	/** Add a static mesh for a mesh group */
	GEOMETRYCOLLECTIONENGINE_API bool BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, TArrayView<const FTransform> NewInstancesTransforms, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);

	UE_DEPRECATED(5.3, "BatchUpdateInstancesTransforms Array parameter version is deprecated, use the TArrayView version instead")
	GEOMETRYCOLLECTIONENGINE_API bool BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);

	/** 
	 * Preallocate an ISM in the pool. 
	 * Doing this early for known mesh instance descriptions can reduce the component registration cost of AddMeshToGroup() for newly discovered mesh descriptions.
	 */
	GEOMETRYCOLLECTIONENGINE_API void PreallocateMeshInstance(const FGeometryCollectionStaticMeshInstance& MeshInstance);

private:
	uint32 NextMeshGroupId = 0;
	TMap<FMeshGroupId, FGeometryCollectionMeshGroup> MeshGroups;
	FGeometryCollectionISMPool Pool;

	// Expose internals for debug draw support.
	friend class UGeometryCollectionISMPoolDebugDrawComponent;
};
