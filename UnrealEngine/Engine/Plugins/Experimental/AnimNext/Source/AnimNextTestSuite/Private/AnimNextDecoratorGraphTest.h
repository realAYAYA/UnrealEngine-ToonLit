// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorSharedData.h"
#include "Graph/AnimNextGraph.h"

#include "AnimNextDecoratorGraphTest.generated.h"

USTRUCT()
struct FTestDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, Inline))
	int32 SomeInt32 = 3;

	UPROPERTY(meta = (Input, Inline))
	float SomeFloat = 34.0f;

	UPROPERTY(meta = (Input))
	int32 SomeLatentInt32 = 3;			// MathAdd with constants, latent

	UPROPERTY(meta = (Input))
	int32 SomeOtherLatentInt32 = 3;		// GetParameter, latent

	UPROPERTY(meta = (Input))
	float SomeLatentFloat = 34.0f;		// Inline value, not latent

	#define DECORATOR_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(SomeLatentInt32) \
		GeneratorMacro(SomeOtherLatentInt32) \
		GeneratorMacro(SomeLatentFloat) \

	GENERATE_DECORATOR_LATENT_PROPERTIES(FTestDecoratorSharedData, DECORATOR_LATENT_PROPERTIES_ENUMERATOR)
	#undef DECORATOR_LATENT_PROPERTIES_ENUMERATOR
};
