// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_StringTable.h"
#include "StringTableEditorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

EAssetCommandResult UAssetDefinition_StringTable::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FStringTableEditorModule& StringTableEditorModule = FModuleManager::LoadModuleChecked<FStringTableEditorModule>("StringTableEditor");

	for (UStringTable* StringTable : OpenArgs.LoadObjects<UStringTable>())
	{
		StringTableEditorModule.CreateStringTableEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, StringTable);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
