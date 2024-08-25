// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#include "WorldPartitionRuntimeHashSet.generated.h"

class UHLODLayer;
class URuntimePartitionPersistent;
struct FPropertyChangedChainEvent;

using FStaticSpatialIndexType = TStaticSpatialIndexRTree<TObjectPtr<UWorldPartitionRuntimeCell>>;

/** Holds an HLOD setup for a particular partition class. */
USTRUCT()
struct FRuntimePartitionHLODSetup
{
	GENERATED_USTRUCT_BODY()

	/** Name for this HLOD layer setup */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	FName Name;

	/** Associated HLOD Layer objects */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	TArray<TObjectPtr<const UHLODLayer>> HLODLayers;

	/** whether this HLOD setup is spatially loaded or not */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	bool bIsSpatiallyLoaded = true;

	UPROPERTY(VisibleAnywhere, Category = RuntimeSettings, Instanced, Meta = (EditCondition = "bIsSpatiallyLoaded", HideEditConditionToggle, NoResetToDefault, TitleProperty = "Name"))
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

	/** HLOD setups used by this partition, one for each layers in the hierarchy */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "Class != nullptr", HideEditConditionToggle, ForceInlineRow))
	TArray<FRuntimePartitionHLODSetup> HLODSetups;
#endif

#if WITH_EDITOR
	void UpdateHLODPartitionLayers();
#endif
};

USTRUCT()
struct FRuntimePartitionStreamingData
{
	GENERATED_USTRUCT_BODY()

	void CreatePartitionsSpatialIndex() const;
	void DestroyPartitionsSpatialIndex() const;

	/** Name of the runtime partition, currently maps to target grids. */
	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 LoadingRange = 0;

	UPROPERTY()
	TArray<TObjectPtr<UWorldPartitionRuntimeCell>> StreamingCells;

	UPROPERTY()
	TArray<TObjectPtr<UWorldPartitionRuntimeCell>> NonStreamingCells;

	// Transient
	mutable TUniquePtr<FStaticSpatialIndexType> SpatialIndex;
};

template<>
struct TStructOpsTypeTraits<FRuntimePartitionStreamingData> : public TStructOpsTypeTraitsBase2<FRuntimePartitionStreamingData>
{
	enum
	{
		WithCopy = false
	};
};

UCLASS()
class ENGINE_API URuntimeHashSetExternalStreamingObject : public URuntimeHashExternalStreamingObjectBase
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

	void CreatePartitionsSpatialIndex() const;
	void DestroyPartitionsSpatialIndex() const;

	UPROPERTY()
	TArray<FRuntimePartitionStreamingData> RuntimeStreamingData;
};

UCLASS(MinimalAPI)
class UWorldPartitionRuntimeHashSet : public UWorldPartitionRuntimeHash
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

public:
	ENGINE_API virtual bool Draw2D(FWorldPartitionDraw2DContext& DrawContext) const override;
	ENGINE_API virtual void Draw3D(const TArray<FWorldPartitionStreamingSource>& Sources) const override;

#if WITH_EDITOR
	// Streaming generation interface
	ENGINE_API virtual void SetDefaultValues() override;
	ENGINE_API virtual bool SupportsHLODs() const override;
	ENGINE_API virtual bool SetupHLODActors(const IStreamingGenerationContext* StreamingGenerationContext, const UWorldPartition::FSetupHLODActorsParams& Params) const override;
	ENGINE_API virtual bool GenerateStreaming(class UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate) override;
	ENGINE_API virtual bool IsValidGrid(FName GridName, const UClass* ActorClass) const;
	ENGINE_API virtual bool IsValidHLODLayer(FName GridName, const FSoftObjectPath& HLODLayerPath) const;
	ENGINE_API virtual void DumpStateLog(FHierarchicalLogArchive& Ar) const override;

	// Helpers
	static ENGINE_API bool ParseGridName(FName GridName, TArray<FName>& MainPartitionTokens, TArray<FName>& HLODPartitionTokens);

	// Conversions
	static ENGINE_API UWorldPartitionRuntimeHashSet* CreateFrom(const UWorldPartitionRuntimeHash* SrcHash);
#endif

	// External streaming object interface
#if WITH_EDITOR
	ENGINE_API virtual TSubclassOf<URuntimeHashExternalStreamingObjectBase> GetExternalStreamingObjectClass() const override { return URuntimeHashSetExternalStreamingObject::StaticClass(); }
#endif
	ENGINE_API virtual bool InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) override;
	ENGINE_API virtual bool RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) override;

	// Streaming interface
	ENGINE_API virtual void ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const;
	ENGINE_API virtual void ForEachStreamingCellsQuery(const FWorldPartitionStreamingQuerySource& QuerySource, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func, FWorldPartitionQueryCache* QueryCache) const override;
	ENGINE_API virtual void ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const override;

protected:
#if WITH_EDITOR
	ENGINE_API virtual bool HasStreamingContent() const override;
	ENGINE_API virtual void StoreStreamingContentToExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* OutExternalStreamingObject) override;
	ENGINE_API virtual void FlushStreamingContent() override;
#endif

private:
#if WITH_EDITOR
	/** Generate the runtime partitions streaming descs. */
	bool GenerateRuntimePartitionsStreamingDescs(const IStreamingGenerationContext* StreamingGenerationContext, TMap<URuntimePartition*, TArray<URuntimePartition::FCellDescInstance>>& OutRuntimeCellDescs) const;

	struct FCellUniqueId
	{
		FString Name;
		FGuid Guid;
	};

	FCellUniqueId GetCellUniqueId(const URuntimePartition::FCellDescInstance& InCellDescInstance) const;
#endif

	ENGINE_API void ForEachStreamingData(TFunctionRef<bool(const FRuntimePartitionStreamingData&)> Func) const;

public:
#if WITH_EDITORONLY_DATA
	/** Persistent partition */
	UPROPERTY()
	FRuntimePartitionDesc PersistentPartitionDesc;
#endif

	/** Array of runtime partition descriptors */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (TitleProperty = "Name"))
	TArray<FRuntimePartitionDesc> RuntimePartitions;

	UPROPERTY()
	TArray<FRuntimePartitionStreamingData> RuntimeStreamingData;
};
