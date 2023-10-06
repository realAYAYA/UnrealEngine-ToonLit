// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorSettings.h"

UStateTreeEditorSettings::UStateTreeEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

FText UStateTreeEditorSettings::GetSectionText() const
{
	return NSLOCTEXT("StateTreeEditor", "StateTreeEditorSettingsName", "StateTree Editor");
}

FText UStateTreeEditorSettings::GetSectionDescription() const
{
	return NSLOCTEXT("StateTreeEditor", "StateTreeEditorSettingsDescription", "Configure options for the StateTree Editor.");
}

#endif // WITH_EDITOR

FName UStateTreeEditorSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}