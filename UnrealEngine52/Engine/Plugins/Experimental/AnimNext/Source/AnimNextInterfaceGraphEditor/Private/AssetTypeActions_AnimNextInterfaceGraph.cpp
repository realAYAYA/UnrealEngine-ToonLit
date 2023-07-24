// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AnimNextInterfaceGraph.h"
#include "AnimNextInterfaceGraphEditor.h"
#include "AnimNextInterfaceGraph.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_AnimNextInterfaceGraph" 

namespace UE::AnimNext::InterfaceGraphEditor
{

void FAssetTypeActions_AnimNextInterfaceGraph::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if(UAnimNextInterfaceGraph* AnimNextInterfaceGraph = Cast<UAnimNextInterfaceGraph>(Object))
		{
			TSharedRef<FGraphEditor> GraphEditor = MakeShared<FGraphEditor>();
			GraphEditor->InitEditor(Mode, EditWithinLevelEditor, AnimNextInterfaceGraph);
		}
	}
}

const TArray<FText>& FAssetTypeActions_AnimNextInterfaceGraph::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AnimNextInterfacesSubMenu", "AnimNext")
	};
	return SubMenus;
}

}

#undef LOCTEXT_NAMESPACE