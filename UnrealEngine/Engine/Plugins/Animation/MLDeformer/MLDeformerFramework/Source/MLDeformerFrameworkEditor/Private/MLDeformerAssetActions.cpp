// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerAssetActions.h"
#include "MLDeformerAsset.h"
#include "MLDeformerEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace UE::MLDeformer
{
	UClass* FMLDeformerAssetActions::GetSupportedClass() const
	{
		return UMLDeformerAsset::StaticClass();
	}

	void FMLDeformerAssetActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
	{
		FAssetTypeActions_Base::GetActions(InObjects, Section);
	}

	void FMLDeformerAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
	{
		const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			if (UMLDeformerAsset* Asset = Cast<UMLDeformerAsset>(*ObjIt))
			{
				TSharedRef<FMLDeformerEditorToolkit> NewEditor(new FMLDeformerEditorToolkit());
				NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, Asset);
			}
		}
	}

	const TArray<FText>& FMLDeformerAssetActions::GetSubMenus() const
	{
		static const TArray<FText> SubMenus
		{
			LOCTEXT("AnimDeformersSubMenu", "Deformers")
		};
		return SubMenus;
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
