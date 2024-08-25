// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/IMovieScenePlaybackCapability.h"
#include <limits.h>

namespace UE::MovieScene
{

FPlaybackCapabilityID FPlaybackCapabilityID::Register()
{
	static int32 StaticID = 0;
	FPlaybackCapabilityID NewID{ StaticID++ };
	// The ID is a bit index inside FPlaybackCapabilitiesImpl's AllCapabilities uint32 bitmask,
	// so we don't want to overflow this.
	checkf((size_t)StaticID < sizeof(uint32) * CHAR_BIT, TEXT("Exceeded the maximum possible amount of playback capabilities!"));
	return NewID;
}

} // namespace UE::MovieScene

