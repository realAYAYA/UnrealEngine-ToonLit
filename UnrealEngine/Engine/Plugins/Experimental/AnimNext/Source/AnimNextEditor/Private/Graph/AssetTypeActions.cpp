// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions.h"
#include "AnimNextGraphEditor.h"
#include "Graph/AnimNextGraph.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_AnimNextGraph" 

namespace UE::AnimNext::Editor
{

void FAssetTypeActions_AnimNextGraph::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if(UAnimNextGraph* AnimNextGraph = Cast<UAnimNextGraph>(Object))
		{
			TSharedRef<FGraphEditor> GraphEditor = MakeShared<FGraphEditor>();
			GraphEditor->InitEditor(Mode, EditWithinLevelEditor, AnimNextGraph);
		}
	}
}

const TArray<FText>& FAssetTypeActions_AnimNextGraph::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AnimNextSubMenu", "AnimNext")
	};
	return SubMenus;
}

}

#undef LOCTEXT_NAMESPACE