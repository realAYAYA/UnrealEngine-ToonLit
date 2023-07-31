// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppMediaTimeSource.h"
#include "MediaUtilsPrivate.h"

#include "Misc/App.h"
#include "Misc/Timespan.h"

/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_CLASS(LogMediaTimeSource, Log, All);

/* IMediaTimeSource interface 
 *****************************************************************************/

FTimespan FAppMediaTimeSource::GetTimecode()
{
	const double CurrentTime = FApp::GetGameTime();
	const FTimespan Timecode = FTimespan::FromSeconds(CurrentTime);

	UE_LOG(LogMediaTimeSource, VeryVerbose, TEXT("AppMediaTimeSource: Time %.10f, Delta %.10f, Timecode %s"), CurrentTime, FApp::GetDeltaTime(), *Timecode.ToString(TEXT("%h:%m:%s.%t")));

	return Timecode;
}
