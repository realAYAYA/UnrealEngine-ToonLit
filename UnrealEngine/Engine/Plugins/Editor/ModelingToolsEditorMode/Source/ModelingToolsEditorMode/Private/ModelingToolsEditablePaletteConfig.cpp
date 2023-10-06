// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsEditablePaletteConfig.h"

TObjectPtr<UModelingModeEditableToolPaletteConfig> UModelingModeEditableToolPaletteConfig::Instance = nullptr;

void UModelingModeEditableToolPaletteConfig::Initialize()
{
	if(!Instance)
	{
		Instance = NewObject<UModelingModeEditableToolPaletteConfig>(); 
		Instance->AddToRoot();
	}
}

FEditableToolPaletteSettings* UModelingModeEditableToolPaletteConfig::GetMutablePaletteConfig(const FName& InstanceName)
{
	if (InstanceName.IsNone())
	{
		return nullptr;
	}

	return &EditableToolPalettes.FindOrAdd(InstanceName);
}

const FEditableToolPaletteSettings* UModelingModeEditableToolPaletteConfig::GetConstPaletteConfig(const FName& InstanceName)
{
	if (InstanceName.IsNone())
	{
		return nullptr;
	}

	return EditableToolPalettes.Find(InstanceName);
}

void UModelingModeEditableToolPaletteConfig::SavePaletteConfig(const FName&)
{
	SaveEditorConfig();
}