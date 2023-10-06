// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MaterialInstanceDynamic.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

EAssetCommandResult UAssetDefinition_MaterialInstanceDynamic::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TSharedRef<FSimpleAssetEditor> Editor = FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, OpenArgs.LoadObjects<UObject>());
	Editor->SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([]() -> bool
	{
		return true;
	}));

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
