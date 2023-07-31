// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoLayerComponentVisualizer.h"

#include "Components/ActorComponent.h"
#include "Components/StereoLayerComponent.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"


void FStereoLayerComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const UStereoLayerComponent* StereoLayerComp = Cast<const UStereoLayerComponent>(Component);
	if(StereoLayerComp != nullptr && StereoLayerComp->Shape != nullptr)
	{
		StereoLayerComp->Shape->DrawShapeVisualization(View, PDI);
    }
}
