// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "MovieSceneDecomposerTests.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneDecomposerTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float FloatProperty = 0.f;
};

