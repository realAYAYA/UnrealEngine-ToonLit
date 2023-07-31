// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MassStateTreeTypes.h"
#include "SmartObjectSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectRequest.h"
#include "MassFindSmartObjectTask.generated.h"

struct FMassSmartObjectUserFragment;
struct FMassZoneGraphLaneLocationFragment;
struct FTransformFragment;

USTRUCT()
struct MASSAIBEHAVIOR_API FMassFindSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	/** Result of the candidates search request */
	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots FoundCandidateSlots;		// @todo: Should turn this in a StateTree result/value.

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasCandidateSlots = false;

	/** The identifier of the search request send by the task to find candidates */
	UPROPERTY()
	FMassSmartObjectRequestID SearchRequestID;

	/** Next update time; task will not do anything when Tick gets called before that time */
	UPROPERTY()
	float NextUpdate = 0.f;

	/** Last lane where the smart objects were searched. */
	UPROPERTY()
	FZoneGraphLaneHandle LastLane;
};

USTRUCT(meta = (DisplayName = "Find Smart Object"))
struct MASSAIBEHAVIOR_API FMassFindSmartObjectTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassFindSmartObjectTaskInstanceData;

	FMassFindSmartObjectTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FTransformFragment> EntityTransformHandle;
	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment, EStateTreeExternalDataRequirement::Optional> LocationHandle;

	/** Gameplay tag query for finding matching smart objects. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery ActivityRequirements;

	/** How frequently to search for new candidates. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float SearchInterval = 10.0f;
};