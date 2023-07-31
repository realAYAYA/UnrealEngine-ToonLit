// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FNiagaraSimCacheViewModel;
class ITableRow;
class STableViewBase;
class SHeaderRow;

class SNiagaraSimCacheView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheView) {}
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheViewModel>, SimCacheViewModel)
	SLATE_END_ARGS()

	using FBufferSelectionInfo = TPair<int32, FText>;

	void Construct(const FArguments& InArgs);

	TSharedRef<ITableRow> MakeRowWidget(const TSharedPtr<int32> RowIndexPtr, const TSharedRef<STableViewBase>& OwnerTable) const;

	void UpdateColumns(const bool bRefresh);

	void UpdateRows(const bool bRefresh);

	void UpdateBufferSelectionList();
	
	TSharedRef<SWidget> BufferSelectionGenerateWidget(TSharedPtr<FBufferSelectionInfo> InItem);

	void BufferSelectionChanged(TSharedPtr<FBufferSelectionInfo> NewSelection, ESelectInfo::Type SelectInfo);

	FText GetBufferSelectionText() const;

	void OnComponentFilterChange(const FText& InFilter);

	void OnSimCacheChanged(const FAssetData& InAsset);

	void OnViewDataChanged(const bool bFullRefresh);

	TArray<TSharedPtr<int32>>					RowItems;
	TSharedPtr<FNiagaraSimCacheViewModel>		SimCacheViewModel;

	TArray<TSharedPtr<FBufferSelectionInfo>>	BufferSelectionList;

	int32										FrameIndex = 0;

	TSharedPtr<SHeaderRow>						HeaderRowWidget;
	TSharedPtr<SListView<TSharedPtr<int32>>>	ListViewWidget;

	TArray<FString>								ComponentFilterArray;
};