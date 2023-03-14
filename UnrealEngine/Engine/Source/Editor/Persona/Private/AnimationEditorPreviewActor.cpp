// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationEditorPreviewActor.h"
#include "AnimationRuntime.h"

void AAnimationEditorPreviewActor::K2_DestroyActor()
{
	// Override this to do nothing and warn the user
	UE_LOG(LogAnimation, Warning, TEXT("Attempting to destroy an animation preview actor, skipping."));
}