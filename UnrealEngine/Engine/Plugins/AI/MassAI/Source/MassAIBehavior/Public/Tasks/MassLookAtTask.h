// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassStateTreeTypes.h"
#include "MassLookAtFragments.h"
#include "MassLookAtTask.generated.h"

class UMassSignalSubsystem;

/**
 * Task to assign a LookAt target for mass processing
 */
USTRUCT()
struct MASSAIBEHAVIOR_API FMassLookAtTaskInstanceData
{
	GENERATED_BODY()

	/** Entity to set as the target for the LookAt behavior. */
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	FMassEntityHandle TargetEntity;
 
	/** Delay before the task ends. Default (0 or any negative) will run indefinitely so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	/** Accumulated time used to stop task if duration is set */
	UPROPERTY()
	float Time = 0.f;
};

USTRUCT(meta = (DisplayName = "Mass LookAt Task"))
struct MASSAIBEHAVIOR_API FMassLookAtTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassLookAtTaskInstanceData;
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FMassLookAtFragment> LookAtHandle;

	/** Look At Mode */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassLookAtMode LookAtMode = EMassLookAtMode::LookForward; 

	/** Random gaze Mode */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassLookAtGazeMode RandomGazeMode = EMassLookAtGazeMode::None;
	
	/** Random gaze yaw angle added to the look direction determined by the look at mode. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 180.0, ClampMax = 180.0))
	uint8 RandomGazeYawVariation = 0;

	/** Random gaze pitch angle added to the look direction determined by the look at mode. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 180.0, ClampMax = 180.0))
	uint8 RandomGazePitchVariation = 0;

	/** If true, allow random gaze to look at other entities too. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRandomGazeEntities = false;
};
