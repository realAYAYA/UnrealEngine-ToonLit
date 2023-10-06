// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SoundGeneratorOutput.generated.h"

/**
 * Base class for generators that have outputs that can be exposed to other game code.
 *
 * NOTE: This is not widely supported and should be considered experimental.
 */
USTRUCT(BlueprintType)
struct FSoundGeneratorOutput
{
	GENERATED_BODY()

	/** The output's name */
	UPROPERTY(BlueprintReadOnly, Category = SoundGeneratorOutput)
	FName Name;
};
