// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneStringChannel.h"
#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneStringSection.generated.h"

class UObject;

/**
 * A single string section
 */
UCLASS(MinimalAPI)
class UMovieSceneStringSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Public access to this section's internal data function
	 */
	const FMovieSceneStringChannel& GetChannel() const { return StringCurve; }

private:

	/** Curve data */
	UPROPERTY()
	FMovieSceneStringChannel StringCurve;
};
