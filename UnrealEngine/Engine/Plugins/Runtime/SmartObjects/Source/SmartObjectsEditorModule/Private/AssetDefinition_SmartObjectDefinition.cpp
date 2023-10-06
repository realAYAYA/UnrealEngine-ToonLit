// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_SmartObjectDefinition.h"
#include "IAssetTools.h"
#include "SmartObjectAssetEditor.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectEditorStyle.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText UAssetDefinition_SmartObjectDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_SmartObjectDefinition", "SmartObject Definition");
}

FLinearColor UAssetDefinition_SmartObjectDefinition::GetAssetColor() const
{
	return FSmartObjectEditorStyle::TypeColor;
}

TSoftClassPtr<> UAssetDefinition_SmartObjectDefinition::GetAssetClass() const
{
	return USmartObjectDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_SmartObjectDefinition::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::AI };
	return Categories;
}

EAssetCommandResult UAssetDefinition_SmartObjectDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	for (USmartObjectDefinition* Definition : OpenArgs.LoadObjects<USmartObjectDefinition>())
	{
		USmartObjectAssetEditor* AssetEditor = NewObject<USmartObjectAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
		AssetEditor->SetObjectToEdit(Definition);
		AssetEditor->Initialize();
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
