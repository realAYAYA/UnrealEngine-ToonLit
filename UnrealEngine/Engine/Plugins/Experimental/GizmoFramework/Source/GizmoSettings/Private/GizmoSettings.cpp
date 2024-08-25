// Copyright Epic Games, Inc. All Rights Reserved.

#include "GizmoSettings.h"

UGizmoSettings::UGizmoSettings(const FObjectInitializer& ObjectInitlaizer)
	: UDeveloperSettings(ObjectInitlaizer)
{}

UGizmoSettings::FOnUpdateSettings UGizmoSettings::OnSettingsChange;

void UGizmoSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChange.Broadcast(this);
}
