// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEEditorSettings.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ConfigUtilities.h"

void UNNEEditorSettings::PostInitProperties()
{
	if (IsTemplate())
	{
		// We want the .ini file to have precedence over the CVar constructor, so we apply the ini to the CVar before following the regular UDeveloperSettingsBackedByCVars flow
		UE::ConfigUtilities::ApplyCVarSettingsFromIni(TEXT("/Script/NNEEditor.NNEEditorSettings"), *GEditorIni, ECVF_SetByProjectSetting);
	}

	Super::PostInitProperties();
}