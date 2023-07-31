// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneFloatChannel.h"
#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneSlomoSection.generated.h"

class UObject;


/**
 * A single floating point section.
 */
UCLASS(MinimalAPI)
class UMovieSceneSlomoSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneSlomoSection();

public:

	/** Float data */
	UPROPERTY()
	FMovieSceneFloatChannel FloatCurve;
};
