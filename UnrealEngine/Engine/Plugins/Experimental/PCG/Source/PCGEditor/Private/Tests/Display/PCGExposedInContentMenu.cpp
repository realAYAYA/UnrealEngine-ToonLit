// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"
#include "AssetToolsModule.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGExposedInContentMenu, FPCGTestBaseClass, "Editor.Plugins.Tools.PCG.ExposedInContentMenu", PCGTestsCommon::TestFlags)

bool FPCGExposedInContentMenu::RunTest(const FString& Parameters)
{
	const FName NAME_AssetTools = "AssetTools";
	const FString PluginName = "PCG";

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_AssetTools);
	TArray<FAdvancedAssetCategory> AdvancedAssetCategories;

	// Content Browser queries AdvancedAssetCategory to display in the menu
	AssetToolsModule.Get().GetAllAdvancedAssetCategories(/*out*/ AdvancedAssetCategories);

	FAdvancedAssetCategory* PCGAssetCategory = AdvancedAssetCategories.FindByPredicate([&PluginName](const FAdvancedAssetCategory& AdvancedAssetCategory) {
		return AdvancedAssetCategory.CategoryName.ToString().Contains(PluginName);
		});

	TestNotNull(TEXT("PCG Category not found"), PCGAssetCategory);
	return true;
}

#endif
