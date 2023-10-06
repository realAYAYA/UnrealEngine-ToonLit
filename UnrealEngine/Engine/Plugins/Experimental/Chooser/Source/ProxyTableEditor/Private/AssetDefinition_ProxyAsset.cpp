// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_ProxyAsset.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ProxyAsset" 

EAssetCommandResult UAssetDefinition_ProxyAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	FSimpleAssetEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Objects);

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE