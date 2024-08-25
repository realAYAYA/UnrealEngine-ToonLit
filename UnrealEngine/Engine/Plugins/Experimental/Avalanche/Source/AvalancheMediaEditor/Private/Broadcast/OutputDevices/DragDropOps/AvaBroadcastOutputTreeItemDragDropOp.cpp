// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBroadcastOutputTreeItemDragDropOp.h"

#include "Broadcast/AvaBroadcast.h"
#include "Broadcast/OutputDevices/AvaBroadcastOutputClassItem.h"
#include "MediaOutput.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AvaBroadcastOutputTreeItemDragDropOp"

TSharedRef<FAvaBroadcastOutputTreeItemDragDropOp> FAvaBroadcastOutputTreeItemDragDropOp::New(const TSharedPtr<FAvaBroadcastOutputTreeItem>& InOutputTreeItem)
{
	TSharedRef<FAvaBroadcastOutputTreeItemDragDropOp> DragDropOp = MakeShared<FAvaBroadcastOutputTreeItemDragDropOp>();
	DragDropOp->Init(InOutputTreeItem);
	return DragDropOp;
}

bool FAvaBroadcastOutputTreeItemDragDropOp::IsValidToDropInChannel(FName InTargetChannelName) const
{
	return OutputTreeItem.IsValid() && OutputTreeItem->IsValidToDropInChannel(InTargetChannelName);
}

FReply FAvaBroadcastOutputTreeItemDragDropOp::OnChannelDrop(FName InTargetChannelName)
{
	if (const TSharedPtr<FAvaBroadcastOutputTreeItem> Item = GetOutputTreeItem())
	{
		UAvaBroadcast& Broadcast = UAvaBroadcast::Get();
		
		FScopedTransaction Transaction(LOCTEXT("OnChannelDrop", "Drop Media Output"));
		Broadcast.Modify();
		
		const UMediaOutput* const MediaOutput = Item->AddMediaOutputToChannel(InTargetChannelName, FAvaBroadcastMediaOutputInfo());
		if (IsValid(MediaOutput))
		{
			return FReply::Handled();
		}
		else
		{
			Transaction.Cancel();
		}
	}
	return FReply::Unhandled();
}

void FAvaBroadcastOutputTreeItemDragDropOp::Init(const TSharedPtr<FAvaBroadcastOutputTreeItem>& InOutputTreeItem)
{
	OutputTreeItem = InOutputTreeItem;
	CurrentHoverText = InOutputTreeItem->GetDisplayName();
	CurrentIconBrush = InOutputTreeItem->GetIconBrush();
	CurrentIconColorAndOpacity = FSlateColor::UseForeground();
	MouseCursor = EMouseCursor::GrabHandClosed;

	SetupDefaults();
	Construct();
}

#undef LOCTEXT_NAMESPACE
