// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FNiagaraSimCacheViewModel;
class ITableRow;
class STableViewBase;
class SHeaderRow;
class SWidgetSwitcher;
class SScrollBar;

class SNiagaraSimCacheView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheView) {}
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheViewModel>, SimCacheViewModel)
	SLATE_END_ARGS()

	using FBufferSelectionInfo = TPair<int32, FText>;

	void Construct(const FArguments& InArgs);

	TSharedRef<ITableRow> MakeRowWidget(const TSharedPtr<int32> RowIndexPtr, const TSharedRef<STableViewBase>& OwnerTable) const;

	// Update existing or generate new columns and apply any filters.
	// @param bReset Weather the columns need a full reset. If true this will destroy existing columns and rebuild new ones. 
	void UpdateColumns(const bool bReset);

	void UpdateRows(const bool bRefresh);
	
	void OnSimCacheChanged();

	void OnViewDataChanged(const bool bFullRefresh);
	void OnBufferChanged();

private:
	bool GetShouldGenerateWidget(FName Name);
	void GenerateColumns();
	void UpdateDIWidget();

	TArray<TSharedPtr<int32>>					RowItems;
	TSharedPtr<FNiagaraSimCacheViewModel>		SimCacheViewModel;
	
	TSharedPtr<SHeaderRow>						HeaderRowWidget;
	TSharedPtr<SListView<TSharedPtr<int32>>>	ListViewWidget;
	TSharedPtr<SWidgetSwitcher>					SwitchWidget;
	TArray<TSharedPtr<SWidget>>					DIVisualizerWidgets;
	TSharedPtr<SScrollBar>						DataInterfaceScrollBar;
};