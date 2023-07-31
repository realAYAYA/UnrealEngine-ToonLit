// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/LevelSnapshotEditorFactory.h"

#include "AssetToolsModule.h"
#include "AssetTypeCategories.h"

#include "LevelSnapshot.h"

#define LOCTEXT_NAMESPACE "LevelSnapshotEditorFactory"

ULevelSnapshotEditorFactory::ULevelSnapshotEditorFactory()
{
	SupportedClass = ULevelSnapshot::StaticClass();

	// This factory manufacture new objects from scratch.
	bCreateNew = true;

	// This factory will open the editor for each new object.
	bEditAfterNew = true;
};

UObject* ULevelSnapshotEditorFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<ULevelSnapshot>(InParent, InClass, InName, Flags);
};


bool ULevelSnapshotEditorFactory::ShouldShowInNewMenu() const
{
	return true;
}
uint32 ULevelSnapshotEditorFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("Level Snapshots", LOCTEXT("AssetCategoryName", "Level Snapshots"));
}

#undef LOCTEXT_NAMESPACE
