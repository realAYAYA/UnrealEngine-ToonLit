// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "ContentBrowserItem.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Templates/Tuple.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"

class FContentBrowserItemData;
class FString;
class FText;
struct FAssetViewCustomColumn;

/** An item (folder or file) displayed in the asset view */
class FAssetViewItem
{
public:
	FAssetViewItem() = default;

	explicit FAssetViewItem(FContentBrowserItem&& InItem);
	explicit FAssetViewItem(const FContentBrowserItem& InItem);

	explicit FAssetViewItem(FContentBrowserItemData&& InItemData);
	explicit FAssetViewItem(const FContentBrowserItemData& InItemData);

	void AppendItemData(const FContentBrowserItem& InItem);

	void AppendItemData(const FContentBrowserItemData& InItemData);

	void RemoveItemData(const FContentBrowserItem& InItem);

	void RemoveItemData(const FContentBrowserItemData& InItemData);

	/** Clear cached custom column data */
	void ClearCachedCustomColumns();

	/** Updates cached custom column data (only does something for files) */
	void CacheCustomColumns(TArrayView<const FAssetViewCustomColumn> CustomColumns, const bool bUpdateSortData, const bool bUpdateDisplayText, const bool bUpdateExisting);
	
	/** Get the display value of a custom column on this item */
	bool GetCustomColumnDisplayValue(const FName ColumnName, FText& OutText) const;

	/** Get the value (and optionally also the type) of a custom column on this item */
	bool GetCustomColumnValue(const FName ColumnName, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType = nullptr) const;

	/** Get the value (and optionally also the type) of a named tag on this item */
	bool GetTagValue(const FName Tag, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType = nullptr) const;

	/** Get the underlying Content Browser item */
	const FContentBrowserItem& GetItem() const;

	bool IsFolder() const;

	bool IsFile() const;

	bool IsTemporary() const;

	/** Get the event fired when the data for this item changes */
	FSimpleMulticastDelegate& OnItemDataChanged();

	/** Get the event fired whenever a rename is requested */
	FSimpleDelegate& OnRenameRequested();

	/** Get the event fired whenever a rename is canceled */
	FSimpleDelegate& OnRenameCanceled();

	/** True if this item should enter inline renaming on the next scroll into view */
	bool ShouldRenameWhenScrolledIntoView() const;

	/** Set that this item should enter inline renaming on the next scroll into view */
	void RenameWhenScrolledIntoView();

	/** Clear that this item should enter inline renaming on the next scroll into view */
	void ClearRenameWhenScrolledIntoView();

private:
	/** Underlying Content Browser item data */
	FContentBrowserItem Item;

	/** An event to fire when the data for this item changes */
	FSimpleMulticastDelegate ItemDataChangedEvent;

	/** Broadcasts whenever a rename is requested */
	FSimpleDelegate RenameRequestedEvent;

	/** Broadcasts whenever a rename is canceled */
	FSimpleDelegate RenameCanceledEvent;

	/** True if this item should enter inline renaming on the next scroll into view */
	bool bRenameWhenScrolledIntoView = false;

	/** Map of values/types for custom columns */
	TMap<FName, TTuple<FString, UObject::FAssetRegistryTag::ETagType>> CachedCustomColumnData;
	
	/** Map of display text for custom columns */
	TMap<FName, FText> CachedCustomColumnDisplayText;
};
