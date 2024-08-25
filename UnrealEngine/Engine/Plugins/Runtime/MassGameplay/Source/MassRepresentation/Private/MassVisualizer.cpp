// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassVisualizer.h"
#include "MassVisualizationComponent.h"

AMassVisualizer::AMassVisualizer()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->SetMobility(EComponentMobility::Static);
	VisComponent = CreateDefaultSubobject<UMassVisualizationComponent>(TEXT("VisualizerComponent"));

	// MassVisualizers are created by the subsystems and should not be exposed to users or allowed to be deleted.
#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = false;
#endif
}

