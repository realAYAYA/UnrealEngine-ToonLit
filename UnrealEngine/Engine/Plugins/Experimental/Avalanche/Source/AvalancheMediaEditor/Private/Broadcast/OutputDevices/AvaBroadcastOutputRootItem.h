// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaBroadcastOutputTreeItem.h"

class FAvaBroadcastOutputRootItem : public FAvaBroadcastOutputTreeItem
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaBroadcastOutputRootItem, FAvaBroadcastOutputTreeItem);

	FAvaBroadcastOutputRootItem()
		: Super(nullptr)
	{}

	//~ Begin IAvaBroadcastOutputTreeItem
	virtual void RefreshChildren() override;
private:
	virtual FText GetDisplayName() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual TSharedPtr<SWidget> GenerateRowWidget() override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaBroadcastMediaOutputInfo& InOutputInfo) override;
	//~ End IAvaBroadcastOutputTreeItem
};
