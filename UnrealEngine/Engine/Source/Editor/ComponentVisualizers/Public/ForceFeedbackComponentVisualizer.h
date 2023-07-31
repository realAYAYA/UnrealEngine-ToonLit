// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttenuatedComponentVisualizer.h"
#include "Components/ForceFeedbackComponent.h"

class COMPONENTVISUALIZERS_API FForceFeedbackComponentVisualizer : public TAttenuatedComponentVisualizer<UForceFeedbackComponent>
{
private:
	virtual bool IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const override
	{
		return ShowFlags.ForceFeedbackRadius;
	}
};
