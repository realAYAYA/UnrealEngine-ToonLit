// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayBehaviorConfig.h"
#include "GameplayBehavior.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayBehaviorConfig)


UGameplayBehavior* UGameplayBehaviorConfig::GetBehavior(UWorld& World) const
{
	if (!BehaviorClass)
	{
		return nullptr;
	}

	UGameplayBehavior* BehaviorCDO = GetMutableDefault<UGameplayBehavior>(BehaviorClass);

	return (BehaviorCDO && BehaviorCDO->IsInstanced(this))
		? NewObject<UGameplayBehavior>(&World, BehaviorClass)
		: BehaviorCDO;
}

