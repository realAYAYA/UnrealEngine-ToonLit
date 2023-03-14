// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_Chooser.h"
#include "ChooserTableEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_ChooserTable" 

namespace UE::ChooserEditor
{

void FAssetTypeActions_ChooserTable::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	FChooserTableEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

const TArray<FText>& FAssetTypeActions_ChooserTable::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("DataInterfacesSubMenu", "Data Interfaces")
	};
	return SubMenus;
}

}

#undef LOCTEXT_NAMESPACE