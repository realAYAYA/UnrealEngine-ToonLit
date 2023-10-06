// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorSharedData.generated.h"

/**
 * FAnimNextDecoratorSharedData
 * Decorator shared data represents a unique instance in the authored static graph.
 * Each instance of a graph will share instances of the read-only shared data.
 * Shared data typically contains hardcoded properties, RigVM latent pin information,
 * or pooled properties shared between multiple decorators.
 * 
 * @see FNodeDescription
 *
 * A FAnimNextDecoratorSharedData is the base type that decorator shared data derives from.
 */
USTRUCT()
struct FAnimNextDecoratorSharedData
{
	GENERATED_BODY()
};
