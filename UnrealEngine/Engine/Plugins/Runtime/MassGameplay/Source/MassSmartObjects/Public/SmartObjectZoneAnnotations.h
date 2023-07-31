// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "ZoneGraphAnnotationComponent.h"
#include "ZoneGraphTypes.h"
#include "SmartObjectZoneAnnotations.generated.h"

class AZoneGraphData;
class USmartObjectSubsystem;

/** Struct to keep track of a SmartObject entry point on a given lane. */
USTRUCT()
struct FSmartObjectLaneLocation
{
	GENERATED_BODY()

	FSmartObjectLaneLocation() = default;
	FSmartObjectLaneLocation(const FSmartObjectHandle InObjectHandle, const int32 InLaneIndex, const float InDistanceAlongLane)
        : ObjectHandle(InObjectHandle)
        , LaneIndex(InLaneIndex)
        , DistanceAlongLane(InDistanceAlongLane)
	{
	}

	UPROPERTY()
	FSmartObjectHandle ObjectHandle;

	UPROPERTY()
	int32 LaneIndex = INDEX_NONE;

	UPROPERTY()
	float DistanceAlongLane = 0.0f;
};

/**
 * Struct to store indices to all entry points on a given lane.
 * Used as a container wrapper to be able to use in a TMap.
 */
USTRUCT()
struct FSmartObjectLaneLocationIndices
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<int32> SmartObjectLaneLocationIndices;
};

/** Per ZoneGraphData smart object look up data. */
USTRUCT()
struct FSmartObjectAnnotationData
{
	GENERATED_BODY()

	/** @return True if this entry is valid (associated to a valid zone graph data), false otherwise. */
	bool IsValid() const { return DataHandle.IsValid(); }

	/** Reset all internal data. */
	void Reset()
	{
		DataHandle = {};
		AffectedLanes.Reset();
		SmartObjectLaneLocations.Reset();
		SmartObjectToLaneLocationIndexLookup.Reset();
		LaneToLaneLocationIndicesLookup.Reset();
	}

	/** Handle of the ZoneGraphData that this smart object annotation data is associated to */
	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FZoneGraphDataHandle DataHandle;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<int32> AffectedLanes;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectLaneLocation> SmartObjectLaneLocations;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TMap<FSmartObjectHandle, int32> SmartObjectToLaneLocationIndexLookup;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TMap<int32, FSmartObjectLaneLocationIndices> LaneToLaneLocationIndicesLookup;

	bool bInitialTaggingCompleted = false;
};

/**
 * ZoneGraph annotations for smart objects
 */
UCLASS(ClassGroup = AI, BlueprintType, meta = (BlueprintSpawnableComponent))
class MASSSMARTOBJECTS_API USmartObjectZoneAnnotations : public UZoneGraphAnnotationComponent
{
	GENERATED_BODY()

public:
	const FSmartObjectAnnotationData* GetAnnotationData(FZoneGraphDataHandle DataHandle) const;
	TOptional<FSmartObjectLaneLocation> GetSmartObjectLaneLocation(const FZoneGraphDataHandle DataHandle, const FSmartObjectHandle SmartObjectHandle) const;

protected:
	virtual void PostSubsystemsInitialized() override;
	virtual FZoneGraphTagMask GetAnnotationTags() const override;
	virtual void TickAnnotation(const float DeltaTime, FZoneGraphAnnotationTagContainer& BehaviorTagContainer) override;

	virtual void PostZoneGraphDataAdded(const AZoneGraphData& ZoneGraphData) override;
	virtual void PreZoneGraphDataRemoved(const AZoneGraphData& ZoneGraphData) override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy) override;
#endif // UE_ENABLE_DEBUG_DRAWING

	/** Filter specifying which lanes the behavior is applied to. */
	UPROPERTY(EditAnywhere, Category = SmartObject)
	FZoneGraphTagFilter AffectedLaneTags;

	/** Entry points graph for each ZoneGraphData. */
	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectAnnotationData> SmartObjectAnnotationDataArray;

	/** Tag to mark the lanes that offers smart objects. */
	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FZoneGraphTag BehaviorTag;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void OnUnregister() override;

	void RebuildForSingleGraph(FSmartObjectAnnotationData& Data, const FZoneGraphStorage& Storage);
	void RebuildForAllGraphs();

	FDelegateHandle OnAnnotationSettingsChangedHandle;
	FDelegateHandle OnGraphDataChangedHandle;
	FDelegateHandle OnMainCollectionChangedHandle;
	FDelegateHandle OnMainCollectionDirtiedHandle;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
	bool bRebuildAllGraphsRequested = false;
#endif

	/** Cached SmartObjectSubsystem */
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> SmartObjectSubsystem = nullptr;
};
