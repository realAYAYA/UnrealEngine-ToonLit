// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_ProxyTable.h"
#include "ProxyTableEditor.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ProxyTable" 

EAssetCommandResult UAssetDefinition_ProxyTable::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UObject*> Objects = OpenArgs.LoadObjects<UObject>();
	
	for (UObject* Object : Objects)
	{
		UE::ProxyTableEditor::FProxyTableEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Object);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE