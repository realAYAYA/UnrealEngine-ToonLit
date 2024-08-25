// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "GameFramework/Actor.h"
#include "Misc/Guid.h"

#include "PCGWorldActor.generated.h"

class UPCGLandscapeCache;
namespace EEndPlayReason { enum Type : int; }

/** This record uniquely identifies a partition actor. */
USTRUCT(BlueprintType)
struct FPCGPartitionActorRecord
{
	GENERATED_BODY()

	/** Unique ID for the grid this actor belongs to. */
	UPROPERTY(VisibleAnywhere, Category = Debug)
	FGuid GridGuid;

	/** The grid size this actor lives on. */
	UPROPERTY(VisibleAnywhere, Category = Debug)
	uint32 GridSize = 0;

	/** The specific grid cell this actor lives in. */
	UPROPERTY(VisibleAnywhere, Category = Debug)
	FIntVector GridCoords = FIntVector::ZeroValue;

	bool operator==(const FPCGPartitionActorRecord& InOther) const;
	friend uint32 GetTypeHash(const FPCGPartitionActorRecord& In);
};

UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable)
class APCGWorldActor : public AActor
{
	GENERATED_BODY()

public:
	APCGWorldActor(const FObjectInitializer& ObjectInitializer);

	//~Begin AActor Interface
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	virtual bool IsUserManaged() const override { return false; }
	virtual bool ShouldExport() override { return false; }
	virtual bool ShouldImport(FStringView ActorPropString, bool IsMovingLevel) override { return false; }
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	//~End AActor Interface
#endif

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	/** Creates guids for unused grid sizes. */
	void CreateGridGuidsIfNecessary(const PCGHiGenGrid::FSizeArray& InGridSizes, bool bAreGridsSerialized);

	/** Returns the serialized grid GUIDs used for the partitioned actors, one per grid size. */
	void GetSerializedGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const;

	/** Returns the transient grid GUIDs used for the partitioned actors, one per grid size. */
	void GetTransientGridGuids(PCGHiGenGrid::FSizeToGuidMap& OutSizeToGuidMap) const;

	/** Add a record for tracking loaded and unloaded partition actors. */
	void AddSerializedPartitionActorRecord(const FPCGPartitionActorRecord& PartitionActorRecord) { Modify(); SerializedPartitionActorRecords.Add(PartitionActorRecord); }

	/** Remove a record for tracking loaded and unloaded partition actors. */
	void RemoveSerializedPartitionActorRecord(const FPCGPartitionActorRecord& PartitionActorRecord) { Modify(); SerializedPartitionActorRecords.Remove(PartitionActorRecord); }

	/** Returns true if there is record of a partition actor living in a certain grid cell, regardless of whether or not it is loaded. */
	bool DoesSerializedPartitionActorExist(const FGuid& GridGuid, uint32 GridSize, const FIntVector& GridCoords) const { return SerializedPartitionActorRecords.Contains({ GridGuid, GridSize, GridCoords }); }

	void MergeFrom(APCGWorldActor* OtherWorldActor);

#if WITH_EDITOR
	static APCGWorldActor* CreatePCGWorldActor(UWorld* InWorld);
#endif

	static inline constexpr uint32 DefaultPartitionGridSize = 25600; // 256m

	/** Size of the PCG partition actor grid for non-hierarchical-generation graphs. */
	UPROPERTY(config, EditAnywhere, Category = GenerationSettings)
	uint32 PartitionGridSize;

	/** Contains all the PCG data required to query the landscape complete. Serialized in cooked builds only */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = CachedData, meta = (NoResetToDefault, DisplayName="Landscape Cache"))
	TObjectPtr<UPCGLandscapeCache> LandscapeCacheObject = nullptr;

	/** Disable creation of Partition Actors on the Z axis. Can improve performances if 3D partitioning is not needed. */
	UPROPERTY(config, EditAnywhere, Category = GenerationSettings)
	bool bUse2DGrid = true;

#if WITH_EDITORONLY_DATA
	/** Allows any currently active editor viewport to act as a Runtime Generation Source. */
	UPROPERTY(EditAnywhere, Category = RuntimeGeneration)
	bool bTreatEditorViewportAsGenerationSource = false;
#endif

private:
	void RegisterToSubsystem();
	void UnregisterFromSubsystem();

#if WITH_EDITOR
	void OnPartitionGridSizeChanged();
#endif

	/** GUIDs of the serialized partitioned actor grids, one per grid size. */
	UPROPERTY()
	TMap<uint32, FGuid> GridGuids;
	mutable FRWLock GridGuidsLock;

	/** GUIDs of the transient partitioned actor grids, one per grid size. */
	UPROPERTY(Transient)
	TMap<uint32, FGuid> TransientGridGuids;
	mutable FRWLock TransientGridGuidsLock;

	/** Keeps a record of what grid cells contain a serialized partition actor. Useful for tracking the existence of PAs even when they are not yet loaded. */
	UPROPERTY()
	TSet<FPCGPartitionActorRecord> SerializedPartitionActorRecords;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Grid/PCGLandscapeCache.h"
#endif
