// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorTransformationsSettings.h"


FName UWaveformEditorTransformationsSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR

FText UWaveformEditorTransformationsSettings::GetSectionText() const
{
	return NSLOCTEXT("WaveformEditorTransformations", "WaveformEditorTransformationsSettingsSection", "Waveform Editor Transformations");
}

FName UWaveformEditorTransformationsSettings::GetSectionName() const
{
	return TEXT("Waveform Editor Transformations");
}
#endif