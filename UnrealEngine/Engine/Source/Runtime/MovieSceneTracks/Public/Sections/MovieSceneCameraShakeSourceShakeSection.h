// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneCameraShakeSection.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneCameraShakeSourceShakeSection.generated.h"

class UObject;

UCLASS(MinimalAPI)
class UMovieSceneCameraShakeSourceShakeSection 
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
{
	GENERATED_BODY()

public:
	UMovieSceneCameraShakeSourceShakeSection(const FObjectInitializer& ObjectInitializer);

	/** IMovieSceneEntityProvider interface */
	void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;
	
public:
	UPROPERTY(EditAnywhere, Category="Camera Shake", meta=(ShowOnlyInnerProperties))
	FMovieSceneCameraShakeSectionData ShakeData;
};

