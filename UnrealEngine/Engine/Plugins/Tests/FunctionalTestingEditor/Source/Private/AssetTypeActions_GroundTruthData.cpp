// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GroundTruthData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_GroundTruthData::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

#undef LOCTEXT_NAMESPACE
