// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieScenePlaybackCapabilities.h"
#include "Misc/FrameTime.h"
#include "Misc/Optional.h"

namespace UE::MovieScene
{

/** Playback capability for controlling how events are triggered */
struct MOVIESCENE_API FEventTriggerControlPlaybackCapability
{
	/** Playback capability ID */
	static TPlaybackCapabilityID<FEventTriggerControlPlaybackCapability> ID;

	/**
	 * Returns whether triggering events should be temporarily disabled.
	 *
	 * @param OutDisabledUntilTime  The time until which to disable triggering events
	 */
	bool IsDisablingEventTriggers(FFrameTime& OutDisabledUntilTime) const;

	TOptional<FFrameTime> DisableEventTriggersUntilTime;
};

}  // namespace UE::MovieScene

