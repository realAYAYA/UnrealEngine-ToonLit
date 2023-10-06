// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalHeightFogComponentVisualizer.h"

#include "Components/ActorComponent.h"
#include "Components/LocalHeightFogComponent.h"
#include "SceneView.h"
#include "SceneManagement.h"

void FLocalHeightFogComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const ULocalHeightFogComponent* LocalHeightFog = Cast<const ULocalHeightFogComponent>(Component);
		if(LocalHeightFog != NULL)
		{
			FTransform LocalHeightFogTransform = LocalHeightFog->GetComponentTransform();

			// Draw local fog volume spherical shape radius
			DrawWireSphereAutoSides(PDI, LocalHeightFogTransform, FColor(200, 255, 255), 1.0f, SDPG_World);
		}
	}
}


