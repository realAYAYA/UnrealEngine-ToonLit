// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneObjectPathChannel.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneObjectPropertySection.generated.h"

class UObject;

UCLASS(MinimalAPI)
class UMovieSceneObjectPropertySection : public UMovieSceneSection
{
public:

	GENERATED_BODY()

	UMovieSceneObjectPropertySection(const FObjectInitializer& ObjInit);

	UPROPERTY()
	FMovieSceneObjectPathChannel ObjectChannel;
};
