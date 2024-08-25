// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameTime.h"

class UMovieScene;
class UMovieSceneSection;

struct PROPERTYANIMATOR_API FPropertyAnimatorMovieSceneUtils
{
	static FFrameTime GetBaseTime(const UMovieSceneSection& InSection, const UMovieScene& InMovieScene);

	static double GetBaseSeconds(const UMovieSceneSection& InSection);
};
