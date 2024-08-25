// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FGMEResourceListViewModel;
class IGMETreeNodeViewModel;
class ITableRow;
class STableViewBase;
template <typename ItemType> class SListView;

class SGMEResourceList
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGMEResourceList)
	{ }
	SLATE_END_ARGS()

	virtual ~SGMEResourceList() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<FGMEResourceListViewModel> InViewModel);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<IGMETreeNodeViewModel> InViewModel
		, const TSharedRef<STableViewBase>& InOwnerTable);

	void Refresh();

private:
	TSharedPtr<FGMEResourceListViewModel> ViewModel;

	TSharedPtr<SListView<TSharedPtr<IGMETreeNodeViewModel>>> ResourceListWidget;

	TArray<TSharedPtr<IGMETreeNodeViewModel>> ResourceItems;
};
