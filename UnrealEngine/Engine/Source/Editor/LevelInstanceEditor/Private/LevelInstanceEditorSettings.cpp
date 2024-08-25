// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstanceEditorSettings.h"
#include "LevelInstance/LevelInstanceActor.h"

ULevelInstanceEditorSettings::ULevelInstanceEditorSettings()
{
	LevelInstanceClassName = ALevelInstance::StaticClass()->GetPathName();
	bEnableStreaming = false;
	bIsEditInPlaceStreamingEnabled = false;
}

ULevelInstanceEditorPerProjectUserSettings::ULevelInstanceEditorPerProjectUserSettings()
{
	bAlwaysShowDialog = true;
	PivotType = ELevelInstancePivotType::CenterMinZ;
}

void ULevelInstanceEditorPerProjectUserSettings::UpdateFrom(const FNewLevelInstanceParams& Params)
{
	ULevelInstanceEditorPerProjectUserSettings* UserSettings = GetMutableDefault<ULevelInstanceEditorPerProjectUserSettings>();

	if (Params.PivotType != UserSettings->PivotType || Params.bAlwaysShowDialog != UserSettings->bAlwaysShowDialog)
	{
		UserSettings->bAlwaysShowDialog = Params.bAlwaysShowDialog;
		UserSettings->PivotType = Params.PivotType;
		UserSettings->SaveConfig();
	}
}