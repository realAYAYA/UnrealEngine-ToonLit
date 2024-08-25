// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualScoutingSettings.h"


UVirtualScoutingSettings* UVirtualScoutingSettings::GetVirtualScoutingSettings()
{
	return GetMutableDefault<UVirtualScoutingSettings>();
}

UVirtualScoutingEditorSettings* UVirtualScoutingEditorSettings::GetVirtualScoutingEditorSettings()
{
	return GetMutableDefault<UVirtualScoutingEditorSettings>();
}
