// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playable/IAvaPlayableVisibilityConstraint.h"
#include "UObject/Object.h"
#include "AvaPlaybackTransition.generated.h"

/**
 * Abstract base class for playback transitions that can be queued in
 * the playback manager's commands.
 */
UCLASS()
class UAvaPlaybackTransition : public UObject, public IAvaPlayableVisibilityConstraint
{
	GENERATED_BODY()
	
public:
	/**
	 * @brief Evaluate the status of loading playables to determine if the transition can start.
	 * @param bOutShouldDiscard indicate if the pending start transition command should be discarded.
	 * @return true if the transition can start, false otherwise.
	 */
	virtual bool CanStart(bool& bOutShouldDiscard) const { bOutShouldDiscard = true; return false; }

	virtual void Start() {}

	virtual void Stop() {}

	virtual bool IsRunning() const { return false; }
	
	//~ Begin IAvaPlayableVisibilityConstraint
	virtual bool IsVisibilityConstrained(const UAvaPlayable* InPlayable) const override { return false; }
	//~ End IAvaPlayableVisibilityConstraint
};