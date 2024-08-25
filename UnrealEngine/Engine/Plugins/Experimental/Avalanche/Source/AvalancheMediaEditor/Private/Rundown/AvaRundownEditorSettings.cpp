// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownEditorSettings.h"

UAvaRundownEditorSettings::UAvaRundownEditorSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName  = TEXT("Rundown Editor");
}

const UAvaRundownEditorSettings* UAvaRundownEditorSettings::Get()
{
	return GetMutable();
}

UAvaRundownEditorSettings* UAvaRundownEditorSettings::GetMutable()
{
	UAvaRundownEditorSettings* const DefaultSettings = GetMutableDefault<UAvaRundownEditorSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}