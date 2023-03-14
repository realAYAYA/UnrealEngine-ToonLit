// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "InternalGameplayTagContainerNetSerializer.generated.h"

USTRUCT()
struct FGameplayTagContainerNetSerializerSerializationHelper
{
	GENERATED_BODY()

	UPROPERTY();
	TArray<FGameplayTag> GameplayTags;
};
