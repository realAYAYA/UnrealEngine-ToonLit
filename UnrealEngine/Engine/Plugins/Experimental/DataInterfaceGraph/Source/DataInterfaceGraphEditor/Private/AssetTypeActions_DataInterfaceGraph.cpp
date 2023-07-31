// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DataInterfaceGraph.h"
#include "DataInterfaceGraphEditor.h"
#include "DataInterfaceGraph.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DataInterfaceGraph" 

namespace UE::DataInterfaceGraphEditor
{

void FAssetTypeActions_DataInterfaceGraph::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if(UDataInterfaceGraph* DataInterfaceGraph = Cast<UDataInterfaceGraph>(Object))
		{
			TSharedRef<FGraphEditor> GraphEditor = MakeShared<FGraphEditor>();
			GraphEditor->InitEditor(Mode, EditWithinLevelEditor, DataInterfaceGraph);
		}
	}
}

const TArray<FText>& FAssetTypeActions_DataInterfaceGraph::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("DataInterfacesSubMenu", "Data Interfaces")
	};
	return SubMenus;
}

}

#undef LOCTEXT_NAMESPACE