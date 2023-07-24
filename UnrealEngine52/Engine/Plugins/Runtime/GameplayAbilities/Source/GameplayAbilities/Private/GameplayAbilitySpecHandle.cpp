// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilitySpecHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbilitySpecHandle)

// ----------------------------------------------------

void FGameplayAbilitySpecHandle::GenerateNewHandle()
{
	// Must be in C++ to avoid duplicate statics accross execution units
	static int32 GHandle = 1;
	Handle = GHandle++;
}
