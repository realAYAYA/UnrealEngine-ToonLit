// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_InterchangeImportTestPlan.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


void FAssetTypeActions_InterchangeImportTestPlan::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Object : InObjects)
	{
		if (UInterchangeImportTestPlan* TestPlan = Cast<UInterchangeImportTestPlan>(Object))
		{
			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Object);
		}
	}
}

#undef LOCTEXT_NAMESPACE
