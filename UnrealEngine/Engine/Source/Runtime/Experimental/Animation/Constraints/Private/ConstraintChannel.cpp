// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConstraintChannel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConstraintChannel)

bool FMovieSceneConstraintChannel::Evaluate(FFrameTime InTime, bool& OutValue) const
{
	if (Times.IsEmpty())
	{
		return false;
	}

	if (InTime.FrameNumber < Times[0])
	{
		return false;
	}

	return FMovieSceneBoolChannel::Evaluate(InTime, OutValue);
}

