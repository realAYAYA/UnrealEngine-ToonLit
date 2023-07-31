// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassDebugVisualizer.h"

#if WITH_EDITORONLY_DATA
#include "MassDebugVisualizationComponent.h"
#endif // WITH_EDITORONLY_DATA

AMassDebugVisualizer::AMassDebugVisualizer()
{
#if WITH_EDITORONLY_DATA
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	DebugVisComponent = CreateDefaultSubobject<UMassDebugVisualizationComponent>(TEXT("VisualizerComponent"));
#endif
}

