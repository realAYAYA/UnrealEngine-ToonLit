// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneFloatChannel.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Math/Color.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneFadeSection.generated.h"

class UObject;


/**
 * A single floating point section.
 */
UCLASS(MinimalAPI)
class UMovieSceneFadeSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneFadeSection();

public:

	/** Float data */
	UPROPERTY()
	FMovieSceneFloatChannel FloatCurve;

	/** Fade color. */
	UPROPERTY(EditAnywhere, Category="Fade", meta=(InlineColorPicker))
	FLinearColor FadeColor;

	/** Fade audio. */
	UPROPERTY(EditAnywhere, Category="Fade")
	uint32 bFadeAudio:1;
};
