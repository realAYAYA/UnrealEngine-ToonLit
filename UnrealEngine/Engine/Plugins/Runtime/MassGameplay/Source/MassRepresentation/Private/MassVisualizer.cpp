// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassVisualizer.h"
#include "MassVisualizationComponent.h"

AMassVisualizer::AMassVisualizer()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->SetMobility(EComponentMobility::Static);
	VisComponent = CreateDefaultSubobject<UMassVisualizationComponent>(TEXT("VisualizerComponent"));
}

