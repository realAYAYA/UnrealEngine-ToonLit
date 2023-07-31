// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneCameraShakeSection.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneCameraShakeSourceShakeSection.generated.h"

class UObject;

UCLASS(MinimalAPI)
class UMovieSceneCameraShakeSourceShakeSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneCameraShakeSourceShakeSection(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(EditAnywhere, Category="Camera Shake", meta=(ShowOnlyInnerProperties))
	FMovieSceneCameraShakeSectionData ShakeData;
};

