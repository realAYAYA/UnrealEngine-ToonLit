// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_StateTree.h"
#include "StateTreeEditorModule.h"
#include "StateTree.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText UAssetDefinition_StateTree::GetAssetDisplayName() const
{
	return LOCTEXT("FAssetTypeActions_StateTree", "StateTree");
}

FLinearColor UAssetDefinition_StateTree::GetAssetColor() const
{
	return FColor(201, 185, 29);
}

TSoftClassPtr<> UAssetDefinition_StateTree::GetAssetClass() const
{
	return UStateTree::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_StateTree::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] =
		{
		EAssetCategoryPaths::AI,
		EAssetCategoryPaths::Gameplay
		};
	return Categories;
}

EAssetCommandResult UAssetDefinition_StateTree::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FStateTreeEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FStateTreeEditorModule>("StateTreeEditorModule");
	for (UStateTree* StateTree : OpenArgs.LoadObjects<UStateTree>())
	{
		EditorModule.CreateStateTreeEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, StateTree);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
