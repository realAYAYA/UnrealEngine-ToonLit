// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLMediaSource.h"

#include "Templates/SharedPointer.h"

namespace HLMediaSource
{
	/** Name of the Adaptive Streaming media option. */
	static const FName AdaptiveStreamingOption("IsAdaptiveSource");
}

/* HLMediaSource structors
 *****************************************************************************/
UHLMediaSource::UHLMediaSource()
{
}

/* UMediaSource interface
 *****************************************************************************/
FString UHLMediaSource::GetUrl() const
{
	return StreamUrl;
}

bool UHLMediaSource::Validate() const
{
	return SUCCEEDED(HLMediaLibrary::ValidateSourceUrl(IsAdaptiveSource, *StreamUrl));
}

/* IMediaSource overrides
 *****************************************************************************/
FName UHLMediaSource::GetDesiredPlayerName() const
{
	return FName(TEXT("HLMediaPlayer"));
}

bool UHLMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == HLMediaSource::AdaptiveStreamingOption)
	{
		return IsAdaptiveSource;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool UHLMediaSource::HasMediaOption(const FName& Key) const
{
	if (Key == HLMediaSource::AdaptiveStreamingOption)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}
