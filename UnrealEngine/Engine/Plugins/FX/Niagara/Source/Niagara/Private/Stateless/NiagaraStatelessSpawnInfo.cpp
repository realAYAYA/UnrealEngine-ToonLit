// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessSpawnInfo.h"

bool FNiagaraStatelessSpawnInfo::IsValid(TOptional<float> LoopDuration) const
{
	switch (Type)
	{
		case ENiagaraStatelessSpawnInfoType::Burst:
			return (Amount.Min + Amount.Max) > 0 && SpawnTime >= 0.0f && SpawnTime < LoopDuration.Get(SpawnTime + UE_SMALL_NUMBER);

		case ENiagaraStatelessSpawnInfoType::Rate:
			return (Rate.Min + Rate.Max) > 0.0f;

		default:
			checkNoEntry();
			return false;
	}
}
