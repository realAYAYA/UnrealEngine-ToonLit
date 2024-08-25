// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEEditorModelDataActions.h"

#include "NNEEditorModelDataEditorToolkit.h"
#include "NNEModelData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace UE::NNEEditor::Private
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

} // UE::NNEEditor::Private

#undef LOCTEXT_NAMESPACE