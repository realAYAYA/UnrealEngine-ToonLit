// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintEditorSettings.h"

void UAnimationBlueprintEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}
