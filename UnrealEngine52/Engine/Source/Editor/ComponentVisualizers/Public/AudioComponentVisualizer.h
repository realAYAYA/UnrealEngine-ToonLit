// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttenuatedComponentVisualizer.h"
#include "Components/AudioComponent.h"

class COMPONENTVISUALIZERS_API FAudioComponentVisualizer : public TAttenuatedComponentVisualizer<UAudioComponent>
{
private:
	virtual bool IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const override
	{
		return ShowFlags.AudioRadius;
	}
};
