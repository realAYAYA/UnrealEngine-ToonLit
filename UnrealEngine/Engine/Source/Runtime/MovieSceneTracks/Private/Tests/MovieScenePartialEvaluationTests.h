// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "MovieScenePartialEvaluationTests.generated.h"

UCLASS(MinimalAPI)
class UMovieScenePartialEvaluationTestObject : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	float FloatProperty = 0.f;

	UPROPERTY()
	FVector VectorProperty;
};

