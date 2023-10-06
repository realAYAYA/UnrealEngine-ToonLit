// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#include "WorldPartitionRuntimeHashSet.generated.h"

class UHLODLayer;
class URuntimePartition;
class URuntimePartitionPersistent;
struct FPropertyChangedChainEvent;

using FStaticSpatialIndexType = TStaticSpatialIndexRTree<UWorldPartitionRuntimeCell*>;

/** Holds settings for an HLOD layer for a particular partition class. */
USTRUCT()
struct FRuntimePartitionHLODSetup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = RuntimeSettings, Meta = (DisplayThumbnail = false))
	TObjectPtr<const UHLODLayer> HLODLayer;

	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Instanced)
	TObjectPtr<URuntimePartition> PartitionLayer;
};

/** Holds settings for a runtime partition instance. */
USTRUCT()
struct FRuntimePartitionDesc
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	/** Partition class */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	TSubclassOf<URuntimePartition> Class;

	/** Name for this partition, used to map actors to it through the Actor.RuntimeGrid property  */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "Class != nullptr", HideEditConditionToggle))
	FName Name;

	/** Main partition object */
	UPROPERTY(VisibleAnywhere, Category = RuntimeSettings, Instanced, Meta = (EditCondition = "Class != nullptr", HideEditConditionToggle, NoResetToDefault, TitleProperty = "Name"))
	TObjectPtr<URuntimePartition> MainLayer;

	/** Associated HLOD Layer object */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "Class != nullptr", HideEditConditionToggle, NoResetToDefault, ForceInlineRow))
	TObjectPtr<const UHLODLayer> HLODLayer;

	/** HLOD setups used by this partition, one for each layers in the hierarchy */
	UPROPERTY(VisibleAnywhere, Category = RuntimeSettings, EditFixedSize, Meta = (EditCondition = "HLODLayer != nullptr", HideEditConditionToggle, ForceInlineRow))
	TArray<FRuntimePartitionHLODSetup> HLODSetups;
#endif

#if WITH_EDITOR
	void UpdateHLODPartitionLayers();
#endif
};

UCLASS()
class URuntimeHashSetExternalStreamingObject : public URuntimeHashExternalStreamingObjectBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TObjectPtr<UWorldPartitionRuntimeCell>> NonSpatiallyLoadedRuntimeCells;

	UPROPERTY()
	TArray<TObjectPtr<UWorldPartitionRuntimeCell>> SpatiallyLoadedRuntimeCells;

	// Transient
	TUniquePtr<FStaticSpatialIndexType> SpatialIndex;
};

UCLASS(HideDropdown, MinimalAPI)
class UWorldPartitionRuntimeHashSet : public UWorldPartitionRuntimeHash
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostLoad() override;
	static ENGINE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

public:
#if WITH_EDITOR
	// Streaming generation interface
	ENGINE_API virtual void SetDefaultValues() override;
	ENGINE_API virtual bool SupportsHLODs() const override;
	ENGINE_API virtual bool GenerateStreaming(class UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate) override;
	ENGINE_API virtual void FlushStreaming() override;
	ENGINE_API virtual bool IsValidGrid(FName GridName) const;
	ENGINE_API virtual TArray<UWorldPartitionRuntimeCell*> GetAlwaysLoadedCells() const override;
	ENGINE_API virtual void DumpStateLog(FHierarchicalLogArchive& Ar) const override;

	// Helpers
	static ENGINE_API TArray<FName> ParseGridName(FName GridName);
#endif

	// External streaming object interface
#if WITH_EDITOR
	ENGINE_API virtual URuntimeHashExternalStreamingObjectBase* StoreToExternalStreamingObject(UObject* StreamingObjectOuter, FName StreamingObjectName) override;
#endif
	ENGINE_API virtual bool InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) override;
	ENGINE_API virtual bool RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) override;

	// Streaming interface
	ENGINE_API virtual void ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const;
	ENGINE_API virtual void ForEachStreamingCellsQuery(const FWorldPartitionStreamingQuerySource& QuerySource, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func, FWorldPartitionQueryCache* QueryCache) const override;
	ENGINE_API virtual void ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const override;

private:
#if WITH_EDITOR
	/** Update the partition layers to reflect the curent HLOD setups. */
	void UpdateHLODPartitionLayers();
#endif

public:
#if WITH_EDITORONLY_DATA
	/** Persistent partition */
	UPROPERTY()
	FRuntimePartitionDesc PersistentPartitionDesc;

	/** Array of runtime partition descriptors */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (TitleProperty = "Name"))
	TArray<FRuntimePartitionDesc> RuntimePartitions;
#endif

	UPROPERTY()
	TArray<TObjectPtr<UWorldPartitionRuntimeCell>> NonSpatiallyLoadedRuntimeCells;

	TUniquePtr<FStaticSpatialIndexType> SpatialIndex;

	TSet<URuntimeHashSetExternalStreamingObject*> InjectedExternalStreamingObjects;
};
