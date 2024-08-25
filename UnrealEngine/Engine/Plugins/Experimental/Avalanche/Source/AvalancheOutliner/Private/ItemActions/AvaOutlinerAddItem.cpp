// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemActions/AvaOutlinerAddItem.h"
#include "AvaOutliner.h"
#include "Item/AvaOutlinerObject.h"
#include "Item/AvaOutlinerTreeRoot.h"
#include "Item/IAvaOutlinerItem.h"
#include "Selection/AvaOutlinerScopedSelection.h"
#include "Widgets/Views/STableRow.h"

FAvaOutlinerAddItem::FAvaOutlinerAddItem(const FAvaOutlinerAddItemParams& InAddItemParams)
	: AddParams(InAddItemParams)
{
}

bool FAvaOutlinerAddItem::ShouldTransact() const
{
	return EnumHasAnyFlags(AddParams.Flags, EAvaOutlinerAddItemFlags::Transact);
}

void FAvaOutlinerAddItem::Execute(FAvaOutliner& InOutliner)
{
	if (AddParams.Item.IsValid())
	{
		//Try to Create Children on Find
		if (EnumHasAnyFlags(AddParams.Flags, EAvaOutlinerAddItemFlags::AddChildren))
		{
			constexpr bool bRecursiveFind = true;
			TArray<FAvaOutlinerItemPtr> Children;
			AddParams.Item->FindValidChildren(Children, bRecursiveFind);
		}

		const FAvaOutlinerItemPtr ParentItem = AddParams.Item->GetParent();

		//If this Array has elements in it, then we need to stop a Circular Dependency from forming
		{
			TArray<FAvaOutlinerItemPtr> PathToRelativeItem = AddParams.Item->FindPath({AddParams.RelativeItem});
			if (PathToRelativeItem.Num() > 0 && ParentItem.IsValid())
			{
				FAvaOutlinerAddItemParams CircularSolverParams;
				CircularSolverParams.Item                     = PathToRelativeItem[0];
				CircularSolverParams.RelativeItem             = AddParams.Item;
				CircularSolverParams.RelativeDropZone         = EItemDropZone::AboveItem;
				CircularSolverParams.Flags                    = AddParams.Flags;
				CircularSolverParams.AttachmentTransformRules = AddParams.AttachmentTransformRules;

				ParentItem->AddChild(CircularSolverParams);
			}
		}

		if (AddParams.RelativeItem.IsValid())
		{
			const TSharedPtr<IAvaOutlinerItem> RelativeItemParent = AddParams.RelativeItem->GetParent();

			//If it's onto item, the Relative Item is going to be the Parent
			if (!AddParams.RelativeDropZone.IsSet() || AddParams.RelativeDropZone == EItemDropZone::OntoItem)
			{
				//If the Relative Item is Onto and it's the same as the Current Parent, shift Item up in the Hierarchy
				//(as long as the parent is a valid one)
				if (AddParams.RelativeItem == ParentItem && RelativeItemParent)
				{
					AddParams.RelativeDropZone = EItemDropZone::BelowItem;
					RelativeItemParent->AddChild(AddParams);
				}
				else
				{
					AddParams.RelativeItem->AddChild(AddParams);
				}
			}
			//else we place it as a Sibling to the Relative Item
			else if (RelativeItemParent)
			{
				RelativeItemParent->AddChild(AddParams);
			}
			//if no parent, then add it to the tree root
			else
			{
				InOutliner.GetTreeRoot()->AddChild(AddParams);
			}
		}
		else
		{
			//If no Relative Item, add to tree root
			InOutliner.GetTreeRoot()->AddChild(AddParams);
		}

		if (const FEditorModeTools* const ModeTools = InOutliner.GetModeTools())
		{
			const FAvaOutlinerScopedSelection ScopedSelection(*ModeTools, EAvaOutlinerScopedSelectionPurpose::Read);

			//Automatically Select Item if it's Selected in Mode Tools
			if (AddParams.Item->IsSelected(ScopedSelection))
			{
				// Select in Outliner but don't signal selection as we already have it selected in Mode Tools
				AddParams.Flags = EAvaOutlinerAddItemFlags::Select;
				AddParams.SelectionFlags &= ~EAvaOutlinerItemSelectionFlags::SignalSelectionChange;
			}
			//Signal Selection Change when we attempt to select this item in the Outliner but it isn't selected in Mode Tools
			else if (EnumHasAnyFlags(AddParams.Flags, EAvaOutlinerAddItemFlags::Select))
			{
				AddParams.SelectionFlags |= EAvaOutlinerItemSelectionFlags::SignalSelectionChange;
			}
		}

		if (EnumHasAnyFlags(AddParams.Flags, EAvaOutlinerAddItemFlags::Select))
		{
			InOutliner.SelectItems({AddParams.Item}, AddParams.SelectionFlags);
		}

		InOutliner.SetOutlinerModified();
	}
}

void FAvaOutlinerAddItem::OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bRecursive)
{
	if (AddParams.Item.IsValid())
	{
		AddParams.Item->OnObjectsReplaced(InReplacementMap, bRecursive);
	}
	if (AddParams.RelativeItem.IsValid())
	{
		AddParams.RelativeItem->OnObjectsReplaced(InReplacementMap, bRecursive);
	}
}
