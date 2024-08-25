// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/EventTriggerControlPlaybackCapability.h"

namespace UE::MovieScene
{

TPlaybackCapabilityID<FEventTriggerControlPlaybackCapability> FEventTriggerControlPlaybackCapability::ID = TPlaybackCapabilityID<FEventTriggerControlPlaybackCapability>::Register();

bool FEventTriggerControlPlaybackCapability::IsDisablingEventTriggers(FFrameTime& OutDisabledUntilTime) const
{
	if (DisableEventTriggersUntilTime.IsSet())
	{
		OutDisabledUntilTime = DisableEventTriggersUntilTime.GetValue();
		return true;
	}
	return false;
}

}  // namespace UE::MovieScene

