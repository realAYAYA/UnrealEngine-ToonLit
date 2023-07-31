// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MaterialInstanceDynamic.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MaterialInstanceDynamic::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	TSharedRef<FSimpleAssetEditor> Editor = FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
	Editor->SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([]() -> bool
	{
		return true;
	}));
}

#undef LOCTEXT_NAMESPACE
