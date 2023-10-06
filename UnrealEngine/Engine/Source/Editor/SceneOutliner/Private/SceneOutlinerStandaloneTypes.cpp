// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerStandaloneTypes.h"

#include "EditorActorFolders.h"
#include "Framework/Application/SlateApplication.h"
#include "ISceneOutlinerTreeItem.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"


#define LOCTEXT_NAMESPACE "SceneOutlinerStandaloneTypes"

uint32 FSceneOutlinerTreeItemType::NextUniqueID = 0;
const FSceneOutlinerTreeItemType ISceneOutlinerTreeItem::Type;

const FLinearColor FSceneOutlinerCommonLabelData::DarkColor(0.15f, 0.15f, 0.15f);

TOptional<FLinearColor> FSceneOutlinerCommonLabelData::GetForegroundColor(const ISceneOutlinerTreeItem& TreeItem) const
{
	if (!TreeItem.IsValid())
	{
		return DarkColor;
	}

	// Darken items that aren't suitable targets for an active drag and drop action
	if (FSlateApplication::Get().IsDragDropping())
	{
		TSharedPtr<FDragDropOperation> DragDropOp = FSlateApplication::Get().GetDragDroppingContent();

		FSceneOutlinerDragDropPayload DraggedObjects(*DragDropOp);
		const auto Outliner = WeakSceneOutliner.Pin();
		if (Outliner->GetMode()->ParseDragDrop(DraggedObjects, *DragDropOp) && !Outliner->GetMode()->ValidateDrop(TreeItem, DraggedObjects).IsValid())
		{
			return DarkColor;
		}
	}

	if (!TreeItem.CanInteract())
	{
		return DarkColor;
	}

	return TOptional<FLinearColor>();
}

bool FSceneOutlinerCommonLabelData::CanExecuteRenameRequest(const ISceneOutlinerTreeItem& Item) const
{
	if (const ISceneOutliner* SceneOutliner = WeakSceneOutliner.Pin().Get())
	{
		return SceneOutliner->CanExecuteRenameRequest(Item);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
