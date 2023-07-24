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

	bool IsStringFilterEnabled() const;
	void Construct(const FArguments& InArgs);

	TSharedRef<ITableRow> MakeRowWidget(const TSharedPtr<int32> RowIndexPtr, const TSharedRef<STableViewBase>& OwnerTable) const;

	// Update existing or generate new columns and apply any filters.
	// @param bReset Weather the columns need a full reset. If true this will destroy existing columns and rebuild new ones. 
	void UpdateColumns(const bool bReset);

	void UpdateRows(const bool bRefresh);

	void UpdateBufferSelectionList();
	
	TSharedRef<SWidget> BufferSelectionGenerateWidget(TSharedPtr<FBufferSelectionInfo> InItem);

	void BufferSelectionChanged(TSharedPtr<FBufferSelectionInfo> NewSelection, ESelectInfo::Type SelectInfo);

	FText GetBufferSelectionText() const;

	void OnComponentFilterChange(const FText& InFilter);

	void OnSimCacheChanged();

	void OnViewDataChanged(const bool bFullRefresh);
	void OnBufferChanged();

private:
	void GenerateColumns();

	TArray<TSharedPtr<int32>>					RowItems;
	TSharedPtr<FNiagaraSimCacheViewModel>		SimCacheViewModel;

	TArray<TSharedPtr<FBufferSelectionInfo>>	BufferSelectionList;
	
	TSharedPtr<SHeaderRow>						HeaderRowWidget;
	TSharedPtr<SListView<TSharedPtr<int32>>>	ListViewWidget;

	TArray<FString>								StringFilterArray;
};