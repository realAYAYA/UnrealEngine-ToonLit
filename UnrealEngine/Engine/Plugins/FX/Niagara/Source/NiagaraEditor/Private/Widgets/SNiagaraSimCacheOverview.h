// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h"
#include "Templates/SharedPointer.h"

class FNiagaraSimCacheViewModel;
class SNiagaraSimCacheTreeView;

enum class ENiagaraSimCacheOverviewItemType
{
	System,
	Emitter,
	Component,
	MAX,
};

struct FNiagaraSimCacheOverviewItem : public TSharedFromThis<FNiagaraSimCacheOverviewItem>
{
	virtual ~FNiagaraSimCacheOverviewItem() = default;

	FNiagaraSimCacheOverviewItem()
	{
		
	}

	FText GetDisplayNameText()
	{
		return DisplayName;
	}

	void SetDisplayName(FText NewName)
	{
		DisplayName = NewName;
	}

	virtual int32 GetBufferIndex()
	{
		return BufferIndex;
	}

	void SetBufferIndex(int32 NewIndex)
	{
		BufferIndex = NewIndex;
	}

	virtual ENiagaraSimCacheOverviewItemType GetType () { return ENiagaraSimCacheOverviewItemType::MAX; }

	virtual TSharedRef<SWidget> GetRowWidget()
	{
		return SNew(STextBlock).Text(GetDisplayNameText());
	}

	int32 BufferIndex = INDEX_NONE;
	FText DisplayName;
};

struct FNiagaraSimCacheOverviewSystemItem : public FNiagaraSimCacheOverviewItem
{
	virtual ENiagaraSimCacheOverviewItemType GetType () override { return ENiagaraSimCacheOverviewItemType::System; }
};

struct FNiagaraSimCacheOverviewEmitterItem : public FNiagaraSimCacheOverviewItem
{
	virtual ENiagaraSimCacheOverviewItemType GetType() override {return ENiagaraSimCacheOverviewItemType::Emitter; }
};

class SNiagaraSimCacheOverview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheOverview) {}
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheViewModel>, SimCacheViewModel)
	SLATE_END_ARGS()

	void OnSimCacheChanged();
	void Construct(const FArguments& InArgs);

	TSharedRef<ITableRow> OnGenerateRowForItem(TSharedRef<FNiagaraSimCacheOverviewItem> Item, const TSharedRef<STableViewBase>& Owner);
	void OnListSelectionChanged(TSharedPtr<FNiagaraSimCacheOverviewItem> Item, ESelectInfo::Type SelectInfo);

	TSharedPtr<SListView<TSharedRef<FNiagaraSimCacheOverviewItem>>> BufferListView;

	TSharedPtr<SNiagaraSimCacheTreeView> TreeViewWidget;

	TSharedPtr<FNiagaraSimCacheViewModel> ViewModel;
};
