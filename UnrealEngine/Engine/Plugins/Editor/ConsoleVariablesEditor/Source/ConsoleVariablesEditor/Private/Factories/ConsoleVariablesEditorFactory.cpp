// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/ConsoleVariablesEditorFactory.h"

#include "AssetToolsModule.h"

#include "ConsoleVariablesAsset.h"

#define LOCTEXT_NAMESPACE "ConsoleVariablesEditorFactory"

UConsoleVariablesEditorFactory::UConsoleVariablesEditorFactory()
{
	SupportedClass = UConsoleVariablesAsset::StaticClass();

	// This factory manufacture new objects from scratch.
	bCreateNew = true;

	// This factory will open the editor for each new object.
	bEditAfterNew = true;
};

UObject* UConsoleVariablesEditorFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UConsoleVariablesAsset>(InParent, InClass, InName, Flags);
};


bool UConsoleVariablesEditorFactory::ShouldShowInNewMenu() const
{
	return true;
}
uint32 UConsoleVariablesEditorFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("Console Variables Editor", LOCTEXT("AssetCategoryName", "Console Variables Editor"));
}

#undef LOCTEXT_NAMESPACE
