// Copyright Epic Games, Inc. All Rights Reserved.

#include "StreamMediaSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StreamMediaSource)


/* UMediaSource overrides
 *****************************************************************************/

FString UStreamMediaSource::GetUrl() const
{
	return StreamUrl;
}


bool UStreamMediaSource::Validate() const
{
	return StreamUrl.Contains(TEXT("://"));
}

