// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "MassObserverProcessor.h"
#include "MassZoneGraphAnnotationProcessors.generated.h"

class UMassSignalSubsystem;
class UZoneGraphAnnotationSubsystem;
struct FMassZoneGraphAnnotationFragment;
struct FMassZoneGraphLaneLocationFragment;
struct FMassEntityHandle;

/** 
 * Processor for initializing ZoneGraph annotation tags.
 */
UCLASS()
class MASSAIBEHAVIOR_API UMassZoneGraphAnnotationTagsInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UMassZoneGraphAnnotationTagsInitializer();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** 
 * Processor for update ZoneGraph annotation tags periodically and on lane changed signal.
 */
UCLASS()
class MASSAIBEHAVIOR_API UMassZoneGraphAnnotationTagUpdateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UMassZoneGraphAnnotationTagUpdateProcessor();
	
protected:
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void UpdateAnnotationTags(UZoneGraphAnnotationSubsystem& ZoneGraphAnnotationSubsystem, FMassZoneGraphAnnotationFragment& AnnotationTags, const FMassZoneGraphLaneLocationFragment& LaneLocation, FMassEntityHandle Entity);

	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

	// Frame buffer, it gets reset every frame.
	TArray<FMassEntityHandle> TransientEntitiesToSignal;
};
