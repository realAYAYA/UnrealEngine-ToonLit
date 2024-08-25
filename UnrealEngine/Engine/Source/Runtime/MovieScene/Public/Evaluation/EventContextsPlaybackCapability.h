// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"

class UObject;

namespace UE::MovieScene
{

/** Playback capability for controlling how events are triggered */
struct MOVIESCENE_API IEventContextsPlaybackCapability
{
	/** Playback capability ID */
	static TPlaybackCapabilityID<IEventContextsPlaybackCapability> ID;

	/** Virtual destructor */
	virtual ~IEventContextsPlaybackCapability() {}

	/** Get the contexts used for triggering events */
	virtual TArray<UObject*> GetEventContexts() const = 0;
};

}  // namespace UE::MovieScene

