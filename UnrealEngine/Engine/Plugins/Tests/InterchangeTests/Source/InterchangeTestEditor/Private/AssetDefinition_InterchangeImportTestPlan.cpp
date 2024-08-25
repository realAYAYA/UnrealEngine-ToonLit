// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_InterchangeImportTestPlan.h"

TConstArrayView<FAssetCategoryPath> UAssetDefinition_InterchangeImportTestPlan::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(NSLOCTEXT("AssetDefinition_InterchangeImportTestPlan_Category", "Name", "Interchange Import Test Plan")) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_InterchangeImportTestPlan::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UInterchangeImportTestPlan* TestPlan : OpenArgs.LoadObjects<UInterchangeImportTestPlan>())
	{
		FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, TestPlan);
	}

	return EAssetCommandResult::Handled;
}
