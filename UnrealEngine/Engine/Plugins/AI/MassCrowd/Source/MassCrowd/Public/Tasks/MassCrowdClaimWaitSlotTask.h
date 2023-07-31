// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ZoneGraphTypes.h"
#include "Tasks/MassZoneGraphPathFollowTask.h"
#include "MassCrowdClaimWaitSlotTask.generated.h"

struct FStateTreeExecutionContext;
class UMassCrowdSubsystem;

/**
* Claim wait slot and expose slot position for path follow.
*/
USTRUCT()
struct MASSCROWD_API FMassCrowdClaimWaitSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	FMassZoneGraphTargetLocation WaitSlotLocation;

	UPROPERTY()
	int32 WaitingSlotIndex = INDEX_NONE;
	
	UPROPERTY()
	FZoneGraphLaneHandle AcquiredLane;
};

USTRUCT(meta = (DisplayName = "Crowd Claim Wait Slot"))
struct MASSCROWD_API FMassCrowdClaimWaitSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassCrowdClaimWaitSlotTaskInstanceData;
	
	FMassCrowdClaimWaitSlotTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment> LocationHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<UMassCrowdSubsystem> CrowdSubsystemHandle;
};
