// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FLiveLinkClient;
struct FSlateColorBrush;

struct FLiveLinkDebugUIEntry;
using FLiveLinkDebugUIEntryPtr = TSharedPtr<FLiveLinkDebugUIEntry>;

class SLiveLinkDebugView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkDebugView) {}
	SLATE_END_ARGS()

	virtual ~SLiveLinkDebugView() override;

	void Construct(const FArguments& Args, FLiveLinkClient* InClient);

private:
	TSharedRef<ITableRow> GenerateRow(FLiveLinkDebugUIEntryPtr Data, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleSourcesChanged();
	void RefreshSourceItems();

private:
	FLiveLinkClient* Client;
	TArray<FLiveLinkDebugUIEntryPtr> DebugItemData;

	TSharedPtr<SListView<FLiveLinkDebugUIEntryPtr>> DebugItemView;
	TSharedPtr<FSlateColorBrush> BackgroundBrushSource;
	TSharedPtr<FSlateColorBrush> BackgroundBrushSubject;
};
