// Copyright Epic Games, Inc. All Rights Reserved.


#include "Harmonix/HarmonixPluginSettings.h"


#define LOCTEXT_NAMESPACE "Harmonix"

UHarmonixPluginSettings::UHarmonixPluginSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Harmonix");
}

#if WITH_EDITOR

FText UHarmonixPluginSettings::GetSectionText() const
{
	return LOCTEXT("PluginSettingsDisplayName", "Harmonix");
}


#endif

#undef LOCTEXT_NAMESPACE