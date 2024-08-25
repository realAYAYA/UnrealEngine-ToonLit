// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Broadcast/Channel/AvaBroadcastMediaOutputInfo.h"

class FAvaBroadcastOutputTreeItem;
class FReply;
class IAvaBroadcastOutputTreeItem;
class SWidget;
class UMediaOutput;
struct FGeometry;
struct FPointerEvent;
struct FSlateBrush;

using FAvaOutputTreeItemPtr = TSharedPtr<IAvaBroadcastOutputTreeItem>;

class IAvaBroadcastOutputTreeItem : public IAvaTypeCastable, public TSharedFromThis<IAvaBroadcastOutputTreeItem>
{
public:
	UE_AVA_INHERITS(IAvaBroadcastOutputTreeItem, IAvaTypeCastable);

	virtual FText GetDisplayName() const = 0;

	virtual const FSlateBrush* GetIconBrush() const = 0;
	
	/** Refreshes what the Children are of this Item. (not recursive!) */
	virtual void RefreshChildren() = 0;

	virtual TSharedPtr<SWidget> GenerateRowWidget() = 0;
	
	virtual const TWeakPtr<FAvaBroadcastOutputTreeItem>& GetParent() const = 0;

	virtual const TArray<FAvaOutputTreeItemPtr>& GetChildren() const = 0;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/** Returns true if it is valid to add this item to the given channel. */
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) = 0;

	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaBroadcastMediaOutputInfo& InOutputInfo) = 0;
};

class FAvaBroadcastOutputTreeItem : public IAvaBroadcastOutputTreeItem
{
public:
	UE_AVA_INHERITS(FAvaBroadcastOutputTreeItem, IAvaBroadcastOutputTreeItem);

	FAvaBroadcastOutputTreeItem(const TSharedPtr<FAvaBroadcastOutputTreeItem>& InParent)
		: ParentWeak(InParent)
	{
	}

	//~ Begin IAvaBroadcastOutputTreeItem
	virtual ~FAvaBroadcastOutputTreeItem() override {};
	virtual const TWeakPtr<FAvaBroadcastOutputTreeItem>& GetParent() const override;
	virtual const TArray<FAvaOutputTreeItemPtr>& GetChildren() const override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) override { return true; }
	//~ End IAvaBroadcastOutputTreeItem

	/** Calls RefreshChildren() on the tree of item. */
	static void RefreshTree(const FAvaOutputTreeItemPtr& InItem);

protected:
	TWeakPtr<FAvaBroadcastOutputTreeItem> ParentWeak;
	TArray<FAvaOutputTreeItemPtr> Children;
};
