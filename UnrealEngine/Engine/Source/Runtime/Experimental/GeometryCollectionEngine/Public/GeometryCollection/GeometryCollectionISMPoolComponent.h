// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if 1

#include "Components/SceneComponent.h"
#include "Containers/Map.h"
#include "Materials/MaterialInterface.h"
#include "GeometryCollectionISMPoolComponent.generated.h"

class AActor;
class UGeometryComponent;
class UInstancedStaticMeshComponent;
class UGeometryCollectionISMPoolComponent;


struct FGeometryCollectionISMInstance
{
	int32 StartIndex;
	int32 Count;
};

struct FInstanceGroups
{
public:
	using FInstanceGroupId = int32;

	struct FInstanceGroupRange
	{
		FInstanceGroupRange(int32 InStart, int32 InCount)
			: Start(InStart)
			, Count(InCount)
		{}
		int32 Start;
		int32 Count;
	};

	//FInstanceGroupId AddGroup(int32 Count)
	//{
	//	int32 StartIndex = 0;
	//	if (GroupRanges.Num())
	//	{
	//		const FInstanceGroupRange& LastGroupRange = GroupRanges.Last();
	//		StartIndex = LastGroupRange.Start + LastGroupRange.Count;
	//	}
	//	return GroupRanges.Emplace(FInstanceGroupRange(StartIndex, Count));
	//}

	//void RemoveGroup(FInstanceGroupId GroupId)
	//{
	//	check(GroupRanges.IsValidIndex(BlockId));
	//	const int32 StartOffset = GroupRanges[GroupId].Count;
	//	FreeList.Add(GroupId);
	//	// 
	//	for (int32 Index = (GroupId + 1); Index < GroupRanges.Num(); Index++)
	//	{
	//		const FInstanceGroupRange& Curr = GroupRanges[Index];
	//		FInstanceGroupRange& Prev = GroupRanges[Index - 1];
	//		Prev.Start = Curr.Start - StartOffset;
	//		Prev.Count = Curr.Count;
	//	}
	//}

	FInstanceGroupId AddGroup(int32 Count)
	{
		const int32 StartIndex = InstancesCount;
		InstancesCount += Count;
		GroupRanges.Emplace(NextGroupId, FInstanceGroupRange(StartIndex, Count));
		return NextGroupId++;
	}

	void RemoveGroup(FInstanceGroupId GroupId)
	{
		check(GroupRanges.Contains(GroupId));
		const FInstanceGroupRange& GroupRangeToRemove = GroupRanges[GroupId];

		// we need now to shift all the groups above the remove one
		for (TPair<FInstanceGroupId, FInstanceGroupRange>& GroupRange: GroupRanges)
		{
			if (GroupRange.Value.Start > GroupRangeToRemove.Start)
			{
				GroupRange.Value.Start -= GroupRangeToRemove.Count;
			}
		}
		// now remove the range safely
		InstancesCount -= GroupRangeToRemove.Count;
		GroupRanges.Remove(GroupId);
	}

	const FInstanceGroupRange& GetGroup(int32 GroupIndex) const { return GroupRanges[GroupIndex]; };

private:
	int32 InstancesCount = 0;
	int32 NextGroupId = 0;
	TMap<FInstanceGroupId, FInstanceGroupRange> GroupRanges;
};

/**
* This represent a unique mesh with potentially overriden materials
* if the array is empty , there's no overrides
*/
struct FGeometryCollectionStaticMeshInstance
{
	UStaticMesh* StaticMesh = nullptr;
	TArray<UMaterialInterface*> MaterialsOverrides;

	bool operator==(const FGeometryCollectionStaticMeshInstance& Other) const 
	{
		if (StaticMesh == Other.StaticMesh)
		{
			if (MaterialsOverrides.Num() == Other.MaterialsOverrides.Num())
			{
				for (int32 MatIndex = 0; MatIndex < MaterialsOverrides.Num(); MatIndex++)
				{
					const FName MatName = MaterialsOverrides[MatIndex] ? MaterialsOverrides[MatIndex]->GetFName() : NAME_None;
					const FName OtherName = Other.MaterialsOverrides[MatIndex] ? Other.MaterialsOverrides[MatIndex]->GetFName() : NAME_None;
					if (MatName != OtherName)
					{
						return false;
					}
				}
				return true;
			}
		}
		return false;
	}
};


FORCEINLINE uint32 GetTypeHash(const FGeometryCollectionStaticMeshInstance& MeshInstance)
{
	uint32 CombinedHash = GetTypeHash(MeshInstance.StaticMesh);
	CombinedHash = HashCombine(CombinedHash, GetTypeHash(MeshInstance.MaterialsOverrides.Num()));
	for (const UMaterialInterface* Material: MeshInstance.MaterialsOverrides)
	{
		CombinedHash = HashCombine(CombinedHash, GetTypeHash(Material));
	}
	return CombinedHash;
}

struct FGeometryCollectionMeshInfo
{
	int32 ISMIndex;
	int32 InstanceGroupIndex;
};

struct FGeometryCollectionISMPool;

/**
* FGeometryCollectionMeshGroup
* a mesh groupo contains various mesh with their instances
*/
struct FGeometryCollectionMeshGroup
{
	using FMeshId = int32;

	FMeshId AddMesh(const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount, const FGeometryCollectionMeshInfo& ISMInstanceInfo);
	bool BatchUpdateInstancesTransforms(FGeometryCollectionISMPool& ISMPool, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);
	void RemoveAllMeshes(FGeometryCollectionISMPool& ISMPool);

	TMap<FGeometryCollectionStaticMeshInstance, FMeshId> Meshes;
	TArray<FGeometryCollectionMeshInfo> MeshInfos;
};

struct FGeometryCollectionISM
{
	FGeometryCollectionISM(AActor* OwmingActor, const FGeometryCollectionStaticMeshInstance& MeshInstance);

	int32 AddInstanceGroup(int32 InstanceCount);

	TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;
	FInstanceGroups InstanceGroups;
};


struct FGeometryCollectionISMPool
{
	using FISMIndex = int32;

	FGeometryCollectionMeshInfo AddISM(UGeometryCollectionISMPoolComponent* OwningComponent, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount);
	bool BatchUpdateInstancesTransforms(FGeometryCollectionMeshInfo& MeshInfo, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport);
	void RemoveISM(const FGeometryCollectionMeshInfo& MeshInfo);

	/** Clear all ISM components and associated data */
	void Clear();

	TMap<FGeometryCollectionStaticMeshInstance, FISMIndex> MeshToISMIndex;
	TArray<FGeometryCollectionISM> ISMs;
	// Todo : since ISMs index cannot change, we'll need a free list 
};


/**
* UGeometryCollectionISMPoolComponent
*   Component that managed a pool of ISM in order to optimize render of geometry collections when not using fracture
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionISMPoolComponent: public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	using FMeshGroupId = int32;
	using FMeshId = int32;

	/** 
	* Create an Mesh group which represent an arbitrary set of mesh with their instance 
	* no resources are created until the meshes are added for this group 
	* return a mesh group Id used to add and update instances
	*/
	FMeshGroupId CreateMeshGroup();

	/** destroy  a mesh group and its associated resources */
	void DestroyMeshGroup(FMeshGroupId MeshGroupId);

	/** Add a static mesh for a nmesh group */
	FMeshId AddMeshToGroup(FMeshGroupId MeshGroupId, const FGeometryCollectionStaticMeshInstance& MeshInstance, int32 InstanceCount);

	/** Add a static mesh for a nmesh group */
	bool BatchUpdateInstancesTransforms(FMeshGroupId MeshGroupId, FMeshId MeshId, int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);

private:
	uint32 NextMeshGroupId = 0;
	TMap<FMeshGroupId, FGeometryCollectionMeshGroup> MeshGroups;
	FGeometryCollectionISMPool Pool;
};
#endif