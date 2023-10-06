// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEEditorToolsModelDataActions.h"
#include "EditorFramework/AssetImportData.h"
#include "NNEModelData.h"
#include "NNEEditorToolsModelDataEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace UE::NNEEditorTools::Private
{

	UClass* FModelDataAssetTypeActions::GetSupportedClass() const
	{
		return UNNEModelData::StaticClass();
	}

	FText FModelDataAssetTypeActions::GetName() const
	{
		return LOCTEXT("FModelDataAssetTypeActionsName", "NNE Model Data");
	}

	FColor FModelDataAssetTypeActions::GetTypeColor() const
	{
		return FColor::Cyan;
	}

	uint32 FModelDataAssetTypeActions::GetCategories()
	{
		return EAssetTypeCategories::Misc;
	}

	void FModelDataAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
	{
		for (UObject* Obj : InObjects)
		{
			UNNEModelData* ModelData = Cast<UNNEModelData>(Obj);
			if (ModelData != nullptr)
			{
				MakeShared<FModelDataEditorToolkit>()->InitEditor(ModelData);
			}
		}
	}

	void FModelDataAssetTypeActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
	{
		for (UObject* Asset : TypeAssets)
		{
			const UNNEModelData* ModelData = CastChecked<UNNEModelData>(Asset);
			if (ModelData->AssetImportData)
			{
				ModelData->AssetImportData->ExtractFilenames(OutSourceFilePaths);
			}
		}
	}

} // UE::NNEEditorTools::Private

#undef LOCTEXT_NAMESPACE