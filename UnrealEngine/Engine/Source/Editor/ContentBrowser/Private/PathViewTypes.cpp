// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathViewTypes.h"

#include "Containers/UnrealString.h"
#include "ContentBrowserItemData.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

FTreeItem::FTreeItem(FContentBrowserItem&& InItem)
	: Item(MoveTemp(InItem))
{
	checkf(Item.IsFolder(), TEXT("FTreeItem must be constructed from a folder item!"));
}

FTreeItem::FTreeItem(const FContentBrowserItem& InItem)
	: Item(InItem)
{
	checkf(Item.IsFolder(), TEXT("FTreeItem must be constructed from a folder item!"));
}

FTreeItem::FTreeItem(FContentBrowserItemData&& InItemData)
	: Item(MoveTemp(InItemData))
{
	checkf(Item.IsFolder(), TEXT("FTreeItem must be constructed from a folder item!"));
}

FTreeItem::FTreeItem(const FContentBrowserItemData& InItemData)
	: Item(InItemData)
{
	checkf(Item.IsFolder(), TEXT("FTreeItem must be constructed from a folder item!"));
}

void FTreeItem::AppendItemData(const FContentBrowserItem& InItem)
{
	checkf(InItem.IsFolder(), TEXT("FTreeItem can only contain folder items!"));
	Item.Append(InItem);
}

void FTreeItem::AppendItemData(const FContentBrowserItemData& InItemData)
{
	checkf(InItemData.IsFolder(), TEXT("FTreeItem can only contain folder items!"));
	Item.Append(InItemData);
}

void FTreeItem::RemoveItemData(const FContentBrowserItem& InItem)
{
	checkf(InItem.IsFolder(), TEXT("FTreeItem can only contain folder items!"));
	Item.Remove(InItem);
}

void FTreeItem::RemoveItemData(const FContentBrowserItemData& InItemData)
{
	checkf(InItemData.IsFolder(), TEXT("FTreeItem can only contain folder items!"));
	Item.Remove(InItemData);
}

const FContentBrowserItem& FTreeItem::GetItem() const
{
	return Item;
}

FSimpleMulticastDelegate& FTreeItem::OnRenameRequested()
{
	return RenameRequestedEvent;
}

bool FTreeItem::IsNamingFolder() const
{
	return bNamingFolder;
}

void FTreeItem::SetNamingFolder(const bool InNamingFolder)
{
	bNamingFolder = InNamingFolder;
}

bool FTreeItem::IsChildOf(const FTreeItem& InParent)
{
	TSharedPtr<FTreeItem> CurrentParent = Parent.Pin();
	while (CurrentParent.IsValid())
	{
		if (CurrentParent.Get() == &InParent)
		{
			return true;
		}

		CurrentParent = CurrentParent->Parent.Pin();
	}

	return false;
}

TSharedPtr<FTreeItem> FTreeItem::GetChild(const FName InChildFolderName) const
{
	for (const TSharedPtr<FTreeItem>& Child : Children)
	{
		if (Child->Item.GetItemName() == InChildFolderName)
		{
			return Child;
		}
	}

	return nullptr;
}

TSharedPtr<FTreeItem> FTreeItem::FindItemRecursive(const FName InFullPath)
{
	if (InFullPath == Item.GetVirtualPath())
	{
		return SharedThis(this);
	}

	for (const TSharedPtr<FTreeItem>& Child : Children)
	{
		if (TSharedPtr<FTreeItem> ChildItem = Child->FindItemRecursive(InFullPath))
		{
			return ChildItem;
		}
	}

	return nullptr;
}

void FTreeItem::RequestSortChildren()
{
	bChildrenRequireSort = true;
}

void FTreeItem::SortChildrenIfNeeded()
{
	if (bChildrenRequireSort)
	{
		if (SortOverride.IsBound())
		{
			SortOverride.Execute(this, Children);
		}
		else
		{
			Children.Sort([](TSharedPtr<FTreeItem> A, TSharedPtr<FTreeItem> B) -> bool
			{
				return A->Item.GetDisplayName().ToString() < B->Item.GetDisplayName().ToString();
			});
		}

		bChildrenRequireSort = false;
	}
}

bool FTreeItem::IsDisplayOnlyFolder() const
{
	return GetItem().IsDisplayOnlyFolder();
}

void FTreeItem::SetSortOverride(FSortTreeItemChildrenDelegate& InSortOverride)
{
	SortOverride = InSortOverride;
}

