// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttenuatedComponentVisualizer.h"
#include "MediaSoundComponent.h"


/**
 * Visualizer for media sound components.
 */
class FMediaSoundComponentVisualizer
	: public TAttenuatedComponentVisualizer<UMediaSoundComponent>
{
private:

	virtual bool IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const override
	{
		return ShowFlags.AudioRadius;
	}
};
