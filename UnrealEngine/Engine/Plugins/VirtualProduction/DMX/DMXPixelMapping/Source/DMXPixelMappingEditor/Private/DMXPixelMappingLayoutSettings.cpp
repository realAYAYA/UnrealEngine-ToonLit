// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingLayoutSettings.h"


FSimpleMulticastDelegate UDMXPixelMappingLayoutSettings::OnLayoutSettingsChanged;

void UDMXPixelMappingLayoutSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		SaveConfig();
		OnLayoutSettingsChanged.Broadcast();
	}
}
