// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructStream.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphAnnotationTypes.h"
#include "Misc/MTAccessDetector.h"
#include "Subsystems/WorldSubsystem.h"

#include "ZoneGraphAnnotationSubsystem.generated.h"

class UZoneGraphAnnotationComponent;
class AZoneGraphData;


/**
 * Struct holding combined tags for a specific ZoneGraphData.
 */
struct ZONEGRAPHANNOTATIONS_API FZoneGraphDataAnnotationTags
{
	TArray<FZoneGraphTagMask> LaneTags;	// Combined array of tags from all Annotations.
	FZoneGraphDataHandle DataHandle;	// Handle of the data
	bool bInUse = false;				// True, if this entry is in use.
};

/**
 * Annotation tags per zone graph data
 */
struct ZONEGRAPHANNOTATIONS_API FZoneGraphAnnotationTagContainer
{
	TArrayView<FZoneGraphTagMask> GetMutableAnnotationTagsForData(const FZoneGraphDataHandle DataHandle)
	{
		check(DataAnnotationTags[DataHandle.Index].DataHandle == DataHandle);
		return DataAnnotationTags[DataHandle.Index].LaneTags;
	}

	TArray<FZoneGraphDataAnnotationTags> DataAnnotationTags;

 	/** Mask combining all static tags used by any of the registered ZoneGraphData. */
	FZoneGraphTagMask CombinedStaticTags;
};


// Struct representing registered ZoneGraph data in the subsystem.
USTRUCT()
struct FRegisteredZoneGraphAnnotation
{
	GENERATED_BODY()

	void Reset()
	{
		AnnotationComponent = nullptr;
		AnnotationTags = FZoneGraphTagMask::None;
	}

	UPROPERTY()
	TObjectPtr<UZoneGraphAnnotationComponent> AnnotationComponent = nullptr;

	FZoneGraphTagMask AnnotationTags = FZoneGraphTagMask::None;	// Combination of all registered Annotation tag masks.
};


/**
* A subsystem managing Zonegraph Annotations.
*/
UCLASS()
class ZONEGRAPHANNOTATIONS_API UZoneGraphAnnotationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UZoneGraphAnnotationSubsystem();

	/** Registers Annotation component */
	void RegisterAnnotationComponent(UZoneGraphAnnotationComponent& Component);
	
	/** Unregisters Annotation component */
	void UnregisterAnnotationComponent(UZoneGraphAnnotationComponent& Component);

	/** Sends an event to the Annotations. */
	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, FZoneGraphAnnotationEventBase>::IsDerived, void>::Type SendEvent(const T& InRequest)
	{
		// This method is thread safe.
		UE_MT_SCOPED_WRITE_ACCESS(EventsDetector);
		Events[CurrentEventStream].Add(InRequest);
	}

	/** @return bitmask of Annotation tags at given lane */
	FZoneGraphTagMask GetAnnotationTags(const FZoneGraphLaneHandle LaneHandle) const
	{
		check(AnnotationTagContainer.DataAnnotationTags.IsValidIndex(LaneHandle.DataHandle.Index));
		const FZoneGraphDataAnnotationTags& AnnotationTags = AnnotationTagContainer.DataAnnotationTags[LaneHandle.DataHandle.Index];
		return AnnotationTags.LaneTags[LaneHandle.Index];
	}

	/** @return First Annotation matching a bit in the bitmask */
	UZoneGraphAnnotationComponent* GetFirstAnnotationForTag(const FZoneGraphTag AnnotationTag) const
	{
		return AnnotationTag.IsValid() ? TagToAnnotationLookup[AnnotationTag.Get()] : nullptr;
	}

	/** Signals the subsystem to re-register all tags. */
#if WITH_EDITOR
	void ReregisterTagsInEditor();
#endif

protected:

	void PostZoneGraphDataAdded(const AZoneGraphData* ZoneGraphData);
	void PreZoneGraphDataRemoved(const AZoneGraphData* ZoneGraphData);

	void AddToAnnotationLookup(UZoneGraphAnnotationComponent& Annotation, const FZoneGraphTagMask AnnotationTags);
	void RemoveFromAnnotationLookup(UZoneGraphAnnotationComponent& Annotation);
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Array of registered components. */
	UPROPERTY(Transient)
	TArray<FRegisteredZoneGraphAnnotation> RegisteredComponents;

	/** Stream of events to be processed, double buffered. */
	UPROPERTY(Transient)
	FInstancedStructStream Events[2];
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(EventsDetector);

	/** Index of the current event stream. */
	int32 CurrentEventStream = 0;

	/** Lookup table from tag index to Annotation */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UZoneGraphAnnotationComponent>> TagToAnnotationLookup;
	
	/** Combined tags for each ZoneGraphData. Each ZoneGraphData is indexed by it's data handle index, so there can be gaps in the array. */
	FZoneGraphAnnotationTagContainer AnnotationTagContainer;

	FDelegateHandle OnPostZoneGraphDataAddedHandle;
	FDelegateHandle OnPreZoneGraphDataRemovedHandle;
};
