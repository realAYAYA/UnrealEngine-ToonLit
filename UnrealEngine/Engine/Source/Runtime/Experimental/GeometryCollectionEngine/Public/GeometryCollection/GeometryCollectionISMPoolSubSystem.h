// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "GeometryCollectionISMPoolSubSystem.generated.h"

class AGeometryCollectionISMPoolActor;
class ULevel;
/**
 * A subsystem managing ISMPool actors.
 * Used by geometry collection now but repurposed for more general use.
 */
UCLASS(MinimalAPI)
class UGeometryCollectionISMPoolSubSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionISMPoolSubSystem();

	// USubsystem BEGIN
	GEOMETRYCOLLECTIONENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	GEOMETRYCOLLECTIONENGINE_API virtual void Deinitialize() override;
	// USubsystem END

	/** Finds or creates an actor. */
	GEOMETRYCOLLECTIONENGINE_API AGeometryCollectionISMPoolActor* FindISMPoolActor(ULevel* Level);
	
	/** Get all actors managed by the subsystem. */
	GEOMETRYCOLLECTIONENGINE_API void GetISMPoolActors(TArray<AGeometryCollectionISMPoolActor*>& OutActors) const;

protected:
	UFUNCTION()
	void OnActorEndPlay(AActor* InSource, EEndPlayReason::Type Reason);

	/** ISMPool are per level **/
	TMap<TObjectPtr<ULevel>, TObjectPtr<AGeometryCollectionISMPoolActor> > PerLevelISMPoolActors;
};
