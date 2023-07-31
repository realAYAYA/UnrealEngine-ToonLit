// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "Tasks/MassZoneGraphPathFollowTask.h"
#include "MassZoneGraphFindWanderTarget.generated.h"

struct FStateTreeExecutionContext;
struct FMassZoneGraphLaneLocationFragment;
class UZoneGraphSubsystem;
class UZoneGraphAnnotationSubsystem;
class UMassCrowdSubsystem;

/**
 * Updates TargetLocation to a wander target based on the agents current location on ZoneGraph.
 */
USTRUCT()
struct MASSCROWD_API FMassZoneGraphFindWanderTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FMassZoneGraphTargetLocation WanderTargetLocation;
};

USTRUCT(meta = (DisplayName = "ZG Find Wander Target"))
struct MASSCROWD_API FMassZoneGraphFindWanderTarget : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassZoneGraphFindWanderTargetInstanceData;
	
	FMassZoneGraphFindWanderTarget();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment> LocationHandle;
	TStateTreeExternalDataHandle<UZoneGraphSubsystem> ZoneGraphSubsystemHandle;
	TStateTreeExternalDataHandle<UZoneGraphAnnotationSubsystem> ZoneGraphAnnotationSubsystemHandle;
	TStateTreeExternalDataHandle<UMassCrowdSubsystem> MassCrowdSubsystemHandle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FZoneGraphTagFilter AllowedAnnotationTags;
};
