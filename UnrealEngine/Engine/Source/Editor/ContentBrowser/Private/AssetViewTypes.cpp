// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetViewTypes.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserItemData.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

FAssetViewItem::FAssetViewItem(FContentBrowserItem&& InItem)
	: Item(MoveTemp(InItem))
{
	checkf(Item.IsValid(), TEXT("FAssetViewItem was constructed from an invalid item!"));
}

FAssetViewItem::FAssetViewItem(const FContentBrowserItem& InItem)
	: Item(InItem)
{
	checkf(Item.IsValid(), TEXT("FAssetViewItem was constructed from an invalid item!"));
}

FAssetViewItem::FAssetViewItem(FContentBrowserItemData&& InItemData)
	: Item(MoveTemp(InItemData))
{
	checkf(Item.IsValid(), TEXT("FAssetViewItem was constructed from an invalid item!"));
}

FAssetViewItem::FAssetViewItem(const FContentBrowserItemData& InItemData)
	: Item(InItemData)
{
	checkf(Item.IsValid(), TEXT("FAssetViewItem was constructed from an invalid item!"));
}

void FAssetViewItem::AppendItemData(const FContentBrowserItem& InItem)
{
	Item.Append(InItem);
	ItemDataChangedEvent.Broadcast();
}

void FAssetViewItem::AppendItemData(const FContentBrowserItemData& InItemData)
{
	Item.Append(InItemData);
	ItemDataChangedEvent.Broadcast();
}

void FAssetViewItem::RemoveItemData(const FContentBrowserItem& InItem)
{
	Item.Remove(InItem);
	if (Item.IsValid())
	{
		ItemDataChangedEvent.Broadcast();
	}
}

void FAssetViewItem::RemoveItemData(const FContentBrowserItemData& InItemData)
{
	Item.Remove(InItemData);
	if (Item.IsValid())
	{
		ItemDataChangedEvent.Broadcast();
	}
}

void FAssetViewItem::ClearCachedCustomColumns()
{
	CachedCustomColumnData.Reset();
	CachedCustomColumnDisplayText.Reset();
}

void FAssetViewItem::CacheCustomColumns(TArrayView<const FAssetViewCustomColumn> CustomColumns, const bool bUpdateSortData, const bool bUpdateDisplayText, const bool bUpdateExisting)
{
	for (const FAssetViewCustomColumn& Column : CustomColumns)
	{
		FAssetData ItemAssetData;
		if (Item.Legacy_TryGetAssetData(ItemAssetData))
		{
			if (bUpdateSortData)
			{
				if (bUpdateExisting ? CachedCustomColumnData.Contains(Column.ColumnName) : !CachedCustomColumnData.Contains(Column.ColumnName))
				{
					CachedCustomColumnData.Add(Column.ColumnName, MakeTuple(Column.OnGetColumnData.Execute(ItemAssetData, Column.ColumnName), Column.DataType));
				}
			}

			if (bUpdateDisplayText)
			{
				if (bUpdateExisting ? CachedCustomColumnDisplayText.Contains(Column.ColumnName) : !CachedCustomColumnDisplayText.Contains(Column.ColumnName))
				{
					if (Column.OnGetColumnDisplayText.IsBound())
					{
						CachedCustomColumnDisplayText.Add(Column.ColumnName, Column.OnGetColumnDisplayText.Execute(ItemAssetData, Column.ColumnName));
					}
					else
					{
						CachedCustomColumnDisplayText.Add(Column.ColumnName, FText::AsCultureInvariant(Column.OnGetColumnData.Execute(ItemAssetData, Column.ColumnName)));
					}
				}
			}
		}
	}
}

bool FAssetViewItem::GetCustomColumnDisplayValue(const FName ColumnName, FText& OutText) const
{
	if (const FText* DisplayValue = CachedCustomColumnDisplayText.Find(ColumnName))
	{
		OutText = *DisplayValue;
		return true;
	}

	return false;
}

bool FAssetViewItem::GetCustomColumnValue(const FName ColumnName, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType) const
{
	if (const auto* ColumnDataPair = CachedCustomColumnData.Find(ColumnName))
	{
		OutString = ColumnDataPair->Key;
		if (OutType)
		{
			*OutType = ColumnDataPair->Value;
		}
		return true;
	}

	return false;
}

bool FAssetViewItem::GetTagValue(const FName Tag, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType) const
{
	if (GetCustomColumnValue(Tag, OutString, OutType))
	{
		return true;
	}

	FContentBrowserItemDataAttributeValue TagValue = Item.GetItemAttribute(Tag, true);
	if (TagValue.IsValid())
	{
		OutString = TagValue.GetValue<FString>();
		if (OutType)
		{
			*OutType = TagValue.GetMetaData().AttributeType;
		}
		return true;
	}

	return false;
}

const FContentBrowserItem& FAssetViewItem::GetItem() const
{
	return Item;
}

bool FAssetViewItem::IsFolder() const
{
	return Item.IsFolder();
}

bool FAssetViewItem::IsFile() const
{
	return Item.IsFile();
}

bool FAssetViewItem::IsTemporary() const
{
	return Item.IsTemporary();
}

FSimpleMulticastDelegate& FAssetViewItem::OnItemDataChanged()
{
	return ItemDataChangedEvent;
}

FSimpleDelegate& FAssetViewItem::OnRenameRequested()
{
	return RenameRequestedEvent;
}

FSimpleDelegate& FAssetViewItem::OnRenameCanceled()
{
	return RenameCanceledEvent;
}

bool FAssetViewItem::ShouldRenameWhenScrolledIntoView() const
{
	return bRenameWhenScrolledIntoView;
}

void FAssetViewItem::RenameWhenScrolledIntoView()
{
	bRenameWhenScrolledIntoView = true;
}

void FAssetViewItem::ClearRenameWhenScrolledIntoView()
{
	bRenameWhenScrolledIntoView = false;
}
