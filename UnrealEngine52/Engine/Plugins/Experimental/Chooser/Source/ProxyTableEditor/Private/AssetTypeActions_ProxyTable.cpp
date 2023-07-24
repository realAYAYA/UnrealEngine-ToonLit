// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_ProxyTable.h"
#include "ProxyTableEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_ProxyTable" 

namespace UE::ProxyTableEditor
{

void FAssetTypeActions_ProxyTable::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	FProxyTableEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

const TArray<FText>& FAssetTypeActions_ProxyTable::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("DataInterfacesSubMenu", "Data Interfaces")
	};
	return SubMenus;
}

}

#undef LOCTEXT_NAMESPACE