// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilitySpecHandle.h"

// ----------------------------------------------------

void FGameplayAbilitySpecHandle::GenerateNewHandle()
{
	// Must be in C++ to avoid duplicate statics accross execution units
	static int32 GHandle = 1;
	Handle = GHandle++;
}
