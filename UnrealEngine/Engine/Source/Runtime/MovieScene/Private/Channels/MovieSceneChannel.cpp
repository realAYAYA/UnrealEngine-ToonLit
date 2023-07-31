// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneChannel.h"
#include "Curves/KeyHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneChannel)

void FMovieSceneChannel::GetKeyTime(const FKeyHandle InHandle, FFrameNumber& OutKeyTime)
{
	GetKeyTimes(MakeArrayView(&InHandle, 1), MakeArrayView(&OutKeyTime, 1));
}

void FMovieSceneChannel::SetKeyTime(const FKeyHandle InHandle, const FFrameNumber InKeyTime)
{
	SetKeyTimes(MakeArrayView(&InHandle, 1), MakeArrayView(&InKeyTime, 1));
}
