//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononProbeComponentVisualizer.h"
#include "PhononProbeComponent.h"
#include "SceneView.h"
#include "SceneManagement.h"

namespace SteamAudio
{
	void FPhononProbeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
	{
		UPhononProbeComponent* ProbeComponent = const_cast<UPhononProbeComponent*>(Cast<UPhononProbeComponent>(Component));
		if (ProbeComponent)
		{
			FScopeLock ProbeLocationsLock(&ProbeComponent->ProbeLocationsCriticalSection);
			const FColor Color(0, 153, 255);
			for (const auto& ProbeLocation : ProbeComponent->ProbeLocations)
			{
				PDI->DrawPoint(ProbeLocation, Color, 5, SDPG_World);
			}
		}
	}
}