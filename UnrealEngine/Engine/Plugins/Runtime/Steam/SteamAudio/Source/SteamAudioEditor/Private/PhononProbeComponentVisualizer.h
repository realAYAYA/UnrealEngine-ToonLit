//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "ComponentVisualizer.h"

class FPrimitiveDrawInterface;
class FSceneView;

namespace SteamAudio
{
	class FPhononProbeComponentVisualizer : public FComponentVisualizer
	{
	public:
		virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	};
}
