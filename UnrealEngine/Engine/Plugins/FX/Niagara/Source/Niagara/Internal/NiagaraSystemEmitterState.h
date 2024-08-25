// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "Stateless/NiagaraStatelessDistribution.h"

#include "NiagaraSystemEmitterState.generated.h"

UENUM()
enum class ENiagaraSystemInactiveResponse
{
	/** Let Emitters Finish Then Kill Emitter */
	Complete,
	/** Emitter & Particles Die Immediatly */
	Kill,
};

UENUM()
enum class ENiagaraEmitterInactiveResponse
{
	/** Let Particles Finish Then Kill Emitter */
	Complete,
	/** Emitter & Particles Die Immediatly */
	Kill,
	/** Emitter deactivates but doesn't die until the system does */
	//Continue,
};

UENUM()
enum class ENiagaraLoopBehavior
{
	Infinite,
	Multiple,
	Once,
};

USTRUCT()
struct FNiagaraSystemStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "System State")
	bool bIgnoreSystemState = true;

	UPROPERTY(EditAnywhere, Category = "System State")
	ENiagaraSystemInactiveResponse InactiveResponse = ENiagaraSystemInactiveResponse::Complete;

	UPROPERTY(EditAnywhere, Category = "System State")
	ENiagaraLoopBehavior LoopBehavior = ENiagaraLoopBehavior::Once;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "0.0"))
	FNiagaraDistributionRangeFloat LoopDuration = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "1", EditCondition = "LoopBehavior == ENiagaraLoopBehavior::Multiple", EditConditionHides))
	int LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	bool bRecalculateDurationEachLoop = false;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (ClampMin = "0.0"))
	FNiagaraDistributionRangeFloat LoopDelay = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "System State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	bool bDelayFirstLoopOnly = false;

	UPROPERTY(EditAnywhere, Category = "System State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once && !bDelayFirstLoopOnly", EditConditionHides))
	bool bRecalculateDelayEachLoop = false;
};

USTRUCT()
struct FNiagaraEmitterStateData
{
	GENERATED_BODY()
		
	//UPROPERTY(EditAnywhere, Category="Emitter State")
	//ENiagaraStatelessEmitterState_SelfSystem LifeCycleMode = ENiagaraStatelessEmitterState_SelfSystem::Self;

	UPROPERTY(EditAnywhere, Category="Emitter State")
	ENiagaraEmitterInactiveResponse InactiveResponse = ENiagaraEmitterInactiveResponse::Complete;

	UPROPERTY(EditAnywhere, Category="Emitter State")
	ENiagaraLoopBehavior LoopBehavior = ENiagaraLoopBehavior::Infinite;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "1", EditCondition = "LoopBehavior == ENiagaraLoopBehavior::Multiple", EditConditionHides))
	int32 LoopCount = 1;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "0.0"))
	FNiagaraDistributionRangeFloat LoopDuration = FNiagaraDistributionRangeFloat(1.0f);

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	bool bRecalculateDurationEachLoop = false;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (ClampMin = "0.0"))
	FNiagaraDistributionRangeFloat LoopDelay = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once", EditConditionHides))
	bool bDelayFirstLoopOnly = false;

	UPROPERTY(EditAnywhere, Category = "Emitter State", meta = (EditCondition = "LoopBehavior != ENiagaraLoopBehavior::Once && !bDelayFirstLoopOnly", EditConditionHides))
	bool bRecalculateDelayEachLoop = false;

	//UPROPERTY(EditAnywhere, Category="Emitter State")
	//ENiagaraStatelessEmitterState_SelfSystem Scalability = ENiagaraStatelessEmitterState_SelfSystem::Self;
	//Enable Distance Culling
	//Enable Visibility Culling
	//Reset Age On Awaken
};
