// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_ChooserTable.h"
#include "ChooserTableEditor.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ChooserTable" 

EAssetCommandResult UAssetDefinition_ChooserTable::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	
	for (UObject* Object : Objects)
	{
		UE::ChooserEditor::FChooserTableEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Object);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE