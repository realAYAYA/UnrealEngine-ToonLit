// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaMovieTimeSource.h"

#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/Timespan.h"

FTimespan FMediaMovieTimeSource::GetTimecode()
{
	const double CurrentTime = FPlatformTime::Seconds();
	const FTimespan Timecode = FTimespan::FromSeconds(CurrentTime);

	return Timecode;
}
