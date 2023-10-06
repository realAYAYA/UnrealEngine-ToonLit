// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MassStateTreeTypes.h"
#include "SmartObjectRuntime.h"
#include "MassZoneGraphPathFollowTask.h"
#include "MassZoneGraphFindSmartObjectTarget.generated.h"

struct FMassSmartObjectUserFragment;
struct FMassZoneGraphLaneLocationFragment;
class UZoneGraphAnnotationSubsystem;
class USmartObjectSubsystem;

/**
* Computes move target to a smart object based on current location on ZoneGraph.
*/
USTRUCT()
struct MASSAIBEHAVIOR_API FMassZoneGraphFindSmartObjectTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Input)
	FSmartObjectClaimHandle ClaimedSlot;

	UPROPERTY(EditAnywhere, Category = Output)
	FMassZoneGraphTargetLocation SmartObjectLocation;
};

USTRUCT(meta = (DisplayName = "ZG Find Smart Object Target"))
struct MASSAIBEHAVIOR_API FMassZoneGraphFindSmartObjectTarget : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassZoneGraphFindSmartObjectTargetInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment> LocationHandle;
	TStateTreeExternalDataHandle<UZoneGraphAnnotationSubsystem> AnnotationSubsystemHandle;
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};