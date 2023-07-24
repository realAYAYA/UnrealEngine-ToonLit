// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "Async/AsyncWork.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "HierarchicalInstancedStaticMeshComponent.generated.h"

class FClusterBuilder;
class FStaticLightingTextureMapping_InstancedStaticMesh;


UENUM()
enum class EHISMViewRelevanceType : uint8
{
	Grass,
	Foliage,
	HISM
};

// Due to BulkSerialize we can't edit the struct, so we must deprecated this one and create a new one
USTRUCT()
struct FClusterNode_DEPRECATED
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector3f BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector3f BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	FClusterNode_DEPRECATED()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode_DEPRECATED& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;

		return Ar;
	}
};


USTRUCT()
struct FClusterNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector3f BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector3f BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	UPROPERTY()
	FVector3f MinInstanceScale;
	UPROPERTY()
	FVector3f MaxInstanceScale;

	FClusterNode()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
		, MinInstanceScale(MAX_flt)
		, MaxInstanceScale(-MAX_flt)
	{
	}

	FClusterNode(const FClusterNode_DEPRECATED& OldNode)
		: BoundMin(OldNode.BoundMin)
		, FirstChild(OldNode.FirstChild)
		, BoundMax(OldNode.BoundMax)
		, LastChild(OldNode.LastChild)
		, FirstInstance(OldNode.FirstChild)
		, LastInstance(OldNode.LastInstance)
		, MinInstanceScale(MAX_flt)
		, MaxInstanceScale(-MAX_flt)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;
		Ar << NodeData.MinInstanceScale;
		Ar << NodeData.MaxInstanceScale;
		
		return Ar;
	}
};

UCLASS(ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	friend class ALightWeightInstanceStaticMeshManager;

	~UHierarchicalInstancedStaticMeshComponent();

	TSharedPtr<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;

	// If true then we allow a translated space when building the cluster tree.
	// This can help for impementations (foliage) where we can have instances with offsets to large for single float precision.
	UPROPERTY()
	uint32 bUseTranslatedInstanceSpace : 1;

	// Origin of the translated space used when building the cluster tree.
	UPROPERTY()
	FVector TranslatedInstanceSpaceOrigin;

	// Table for remapping instances from cluster tree to PerInstanceSMData order
	UPROPERTY()
	TArray<int32> SortedInstances;

	// The number of instances in the ClusterTree. Subsequent instances will always be rendered.
	UPROPERTY()
	int32 NumBuiltInstances;

	// Normally equal to NumBuiltInstances, but can be lower if density scaling is in effect
	int32 NumBuiltRenderInstances;

	// Bounding box of any built instances (cached from the ClusterTree)
	UPROPERTY()
	FBox BuiltInstanceBounds;

	// Bounding box of any unbuilt instances
	UPROPERTY()
	FBox UnbuiltInstanceBounds;

	// Bounds of each individual unbuilt instance, used for LOD calculation
	UPROPERTY()
	TArray<FBox> UnbuiltInstanceBoundsList;

	// Enable for detail meshes that don't really affect the game. Disable for anything important.
	// Typically, this will be enabled for small meshes without collision (e.g. grass) and disabled for large meshes with collision (e.g. trees)
	UPROPERTY()
	uint32 bEnableDensityScaling:1;

	// Current value of density scaling applied to this component
	float CurrentDensityScaling;

	// The number of nodes in the occlusion layer
	UPROPERTY()
	int32 OcclusionLayerNumNodes;

	// The last mesh bounds that was cache
	UPROPERTY()
	FBoxSphereBounds CacheMeshExtendedBounds;

	UPROPERTY()
	bool bDisableCollision;

	// Instances to render (including removed one until the build is complete)
	UPROPERTY()
	int32 InstanceCountToRender;

	bool bIsAsyncBuilding : 1;
	bool bIsOutOfDate : 1;
	bool bConcurrentChanges : 1;
	bool bAutoRebuildTreeOnInstanceChanges : 1;

#if WITH_EDITOR
	// in Editor mode we might disable the density scaling for edition
	bool bCanEnableDensityScaling : 1;
#endif

	// Apply the results of the async build
	void ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime);

public:

	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
	
#if WITH_EDITOR
	virtual void PostStaticMeshCompilation() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// UInstancedStaticMesh interface
	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false) override;
	virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices, bool bWorldSpace = false) override;
	virtual bool RemoveInstance(int32 InstanceIndex) override;
	virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove) override;
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex, float CustomDataValue, bool bMarkRenderStateDirty = false) override;
	virtual bool SetCustomData(int32 InstanceIndex, const TArray<float>& InCustomData, bool bMarkRenderStateDirty = false) override;
	virtual bool BatchUpdateInstancesTransforms(int32 StartInstanceIndex, const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace=false, bool bMarkRenderStateDirty=false, bool bTeleport=false) override;
	virtual bool BatchUpdateInstancesTransform(int32 StartInstanceIndex, int32 NumInstances, const FTransform& NewInstancesTransform, bool bWorldSpace=false, bool bMarkRenderStateDirty=false, bool bTeleport=false) override;
	virtual bool BatchUpdateInstancesData(int32 StartInstanceIndex, int32 NumInstances, FInstancedStaticMeshInstanceData* StartInstanceData, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;

	virtual void ClearInstances() override;
	virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace = true) const override;
	virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box, bool bBoxInWorldSpace = true) const override;
	virtual void PreAllocateInstancesMemory(int32 AddedInstanceCount) override;
	virtual bool SupportsRemoveSwap() const override { return true; }

	/** Get the number of instances that overlap a given sphere */
	int32 GetOverlappingSphereCount(const FSphere& Sphere) const;
	/** Get the number of instances that overlap a given box */
	int32 GetOverlappingBoxCount(const FBox& Box) const;
	/** Get the transforms of instances inside the provided box */
	void GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const;

	virtual bool ShouldCreatePhysicsState() const override;

	bool BuildTreeIfOutdated(bool Async, bool ForceUpdate);
	UE_DEPRECATED(5.1, "The BuildTreeAnyThread method is moving to UGrassInstancedStaticMeshComponent. Please update your project to use the new component and method or your project may not compile in the next udpate.")
	static void BuildTreeAnyThread(TArray<FMatrix>& InstanceTransforms, TArray<float>& InstanceCustomDataFloats, int32 NumCustomDataFloats, const FBox& MeshBox, TArray<FClusterNode>& OutClusterTree, TArray<int32>& OutSortedInstances, TArray<int32>& OutInstanceReorderTable, int32& OutOcclusionLayerNum, int32 MaxInstancesPerLeaf, bool InGenerateInstanceScalingRange);
	UE_DEPRECATED(5.1, "The AcceptPrebuiltTree method is moving to UGrassInstancedStaticMeshComponent. Please update your project to use the new component and method or your project may not compile in the next udpate.")
	void AcceptPrebuiltTree(TArray<FInstancedStaticMeshInstanceData>& InInstanceData, TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances);
	bool IsAsyncBuilding() const { return bIsAsyncBuilding; }
	bool IsTreeFullyBuilt() const { return !bIsOutOfDate; }

	/** Heuristic for the number of leaves in the tree **/
	int32 DesiredInstancesPerLeaf();

	virtual void ApplyComponentInstanceData(struct FInstancedStaticMeshComponentInstanceData* InstancedMeshData) override;
	
	// Number of instances in the render-side instance buffer
	virtual int32 GetNumRenderInstances() const { return SortedInstances.Num(); }

	/** Will apply current density scaling, if enabled **/
	void UpdateDensityScaling();

	virtual void PropagateLightingScenarioChange() override;

	EHISMViewRelevanceType GetViewRelevanceType() const { return ViewRelevanceType; }

protected:
	void BuildTree();
	void BuildTreeAsync();
	void ApplyBuildTree(FClusterBuilder& Builder, const bool bWasAsyncBuild);
	void ApplyEmpty();
	void SetPerInstanceLightMapAndEditorData(FStaticMeshInstanceData& PerInstanceData, const TArray<TRefCountPtr<HHitProxy>>& HitProxies);

	FVector CalcTranslatedInstanceSpaceOrigin() const;
	void GetInstanceTransforms(TArray<FMatrix>& InstanceTransforms, FVector const& Offset) const;
	void InitializeInstancingRandomSeed();

	/** Removes specified instances */ 
	void RemoveInstancesInternal(const int32* InstanceIndices, int32 Num);
	
	/** Gets and approximate number of verts for each LOD to generate heuristics **/
	int32 GetVertsForLOD(int32 LODIndex);
	/** Average number of instances per leaf **/
	float ActualInstancesPerLeaf();
	/** For testing, prints some stats after any kind of build **/
	void PostBuildStats();

	virtual void OnPostLoadPerInstanceData() override;

	virtual FVector GetTranslatedInstanceSpaceOrigin() const override { return TranslatedInstanceSpaceOrigin; }

	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const override;
	virtual void PartialNavigationUpdate(int32 InstanceIdx) override;
	virtual bool SupportsPartialNavigationUpdate() const override { return true; }
	virtual FBox GetNavigationBounds() const override;
	void FlushAccumulatedNavigationUpdates();
	mutable FBox AccumulatedNavigationDirtyArea;

	FGraphEventArray BuildTreeAsyncTasks;
	EHISMViewRelevanceType ViewRelevanceType = EHISMViewRelevanceType::HISM;

protected:
	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
	friend class FClusterBuilder;
};

