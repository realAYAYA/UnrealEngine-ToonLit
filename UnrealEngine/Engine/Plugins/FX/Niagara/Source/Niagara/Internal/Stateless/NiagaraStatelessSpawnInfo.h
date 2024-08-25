// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessDistribution.h"

#include "NiagaraStatelessSpawnInfo.generated.h"

UENUM()
enum class ENiagaraStatelessSpawnInfoType
{
	Burst,
	Rate
};

USTRUCT()
struct FNiagaraStatelessSpawnInfo
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid SourceId;
#endif

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ShowInStackItemHeader, StackItemHeaderAlignment = "Left"))
	ENiagaraStatelessSpawnInfoType	Type = ENiagaraStatelessSpawnInfoType::Burst;

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (EditConditionHides, EditCondition = "Type == ENiagaraStatelessSpawnInfoType::Burst"))
	float SpawnTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (EditConditionHides, EditCondition = "Type == ENiagaraStatelessSpawnInfoType::Burst", ClampMin = "0", DisableBindingDistribution))
	FNiagaraDistributionRangeInt Amount = FNiagaraDistributionRangeInt(1);

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (EditConditionHides, EditCondition = "Type == ENiagaraStatelessSpawnInfoType::Rate", ClampMin = "0.0", DisableBindingDistribution))
	FNiagaraDistributionRangeFloat Rate = FNiagaraDistributionRangeFloat(60.0f);

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (InlineEditConditionToggle))
	bool bSpawnProbabilityEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (EditCondition = "bSpawnProbabilityEnabled", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SpawnProbability = 1.0f;

	bool IsValid(TOptional<float> LoopDuration) const;
};

struct FNiagaraStatelessRuntimeSpawnInfo
{
	ENiagaraStatelessSpawnInfoType Type = ENiagaraStatelessSpawnInfoType::Burst;
	uint32	UniqueOffset	= 0;
	float	SpawnTimeStart	= 0.0f;
	float	SpawnTimeEnd	= 0.0f;
	float	Rate			= 0.0f;
	int32	Amount			= 0;
};
