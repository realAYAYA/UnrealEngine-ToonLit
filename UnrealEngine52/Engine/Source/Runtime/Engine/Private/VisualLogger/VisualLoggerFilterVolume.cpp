// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLoggerFilterVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisualLoggerFilterVolume)

AVisualLoggerFilterVolume::AVisualLoggerFilterVolume(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bIsEditorOnlyActor = true;
}
