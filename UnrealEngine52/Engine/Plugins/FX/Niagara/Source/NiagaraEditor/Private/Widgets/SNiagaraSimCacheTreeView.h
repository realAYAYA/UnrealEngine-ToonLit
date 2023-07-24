﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSimCacheViewModel.h"

#include "NiagaraEditorStyle.h"
#include "Widgets/SNiagaraPinTypeSelector.h"
#include "EdGraphSchema_Niagara.h"
#include "SNiagaraSimCacheOverview.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SScaleBox.h"

class SNiagaraSimCacheTreeView;
class FNiagaraSimCacheViewModel;
struct FNiagaraSimCacheTreeItem;
struct FNiagaraSimCacheOverviewItem;

///// Widget for controlling filters on the tree view
class SNiagaraSimCacheTreeViewFilterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheTreeViewFilterWidget) {}
	SLATE_END_ARGS()

	
	void Construct(const FArguments& InArgs, TWeakPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> InTreeView);

protected:
	
	void OnCheckStateChanged(ECheckBoxState InState);
	FReply OnClearAllReleased();
	FReply OnSelectAllReleased();
	ECheckBoxState GetFilterState() const;
	bool IsFilterActive() const;

private:
	TWeakPtr<FNiagaraSimCacheTreeItem> WeakTreeItem;
	TWeakPtr<SNiagaraSimCacheTreeView> WeakTreeView;

};

///// Widget for managing visibility of a component in the tree view.
class SSimCacheTreeViewVisibilityWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCacheTreeViewVisibilityWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> InTreeView);

protected:
	void OnCheckStateChanged(ECheckBoxState NewState);
	ECheckBoxState GetCheckedState() const;
	
	bool IsInFilter() const;
	bool IsFilterActive() const;
	bool IsItemSelected() const;
private:
	TWeakPtr<FNiagaraSimCacheTreeItem> WeakTreeItem;
	TWeakPtr<SNiagaraSimCacheTreeView> WeakTreeView;
};

class SNiagaraSimCacheTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheTreeView) {}
	SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheViewModel>, SimCacheViewModel)
SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	bool HasSelectionFilter() const {return !SelectionForFilter.IsEmpty();}
	bool IsFilterActive() const;
	bool IsItemInFilter(TSharedPtr<FNiagaraSimCacheTreeItem> InItem) const;
	void SetFilterActive(bool bNewActive);
	void VisibilityButtonClicked(TSharedRef<FNiagaraSimCacheTreeItem> InItem);
	bool IsItemSelected(TSharedRef<FNiagaraSimCacheTreeItem> InItem);
	void ClearFilterSelection();
	void UpdateStringFilters();
	void SelectAll();
private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedRef<FNiagaraSimCacheTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	void OnGetChildren(TSharedRef<FNiagaraSimCacheTreeItem> InItem, TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& OutChildren);

	void OnBufferChanged();

	void RecursiveAddToSelectionFilter(TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& ArrayToAdd);
	void RecursiveRemoveFromSelectionFilter(TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& ArrayToRemove);
	
	void UpdateSelectionFilter(TSharedRef<FNiagaraSimCacheTreeItem> ClickedItem);
	
	
	TSharedPtr<STreeView<TSharedRef<FNiagaraSimCacheTreeItem>>> TreeView;

	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> ActiveRootEntries;

	TSharedPtr<FNiagaraSimCacheViewModel> ViewModel;

	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> SelectionForFilter;
};

struct FNiagaraSimCacheTreeItem : public FNiagaraSimCacheOverviewItem
{
	FNiagaraSimCacheTreeItem(TWeakPtr<SNiagaraSimCacheTreeView> InOwner)
	{
		Owner = InOwner;
	}

	virtual ~FNiagaraSimCacheTreeItem() override
	{
		Children.Empty();
	}

	FString GetFilterName()
	{
		return FilterName;
	}

	void SetFilterName(FString NewName)
	{
		FilterName = NewName;
	}

	virtual FNiagaraSimCacheTreeItem* GetRootItem()
	{
		return this;
	}

	void AddChild(TSharedRef<FNiagaraSimCacheTreeItem> NewChild)
	{
		Children.AddUnique(NewChild);
	}

	virtual ENiagaraSimCacheOverviewItemType GetType () override { return ENiagaraSimCacheOverviewItemType::System; }

	virtual TSharedRef<SWidget> GetRowWidget() override
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(GetDisplayNameText())
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SNiagaraSimCacheTreeViewFilterWidget, SharedThis(this), Owner)
			];
	}

	FText DisplayName;
	FString FilterName;
	TSharedPtr<SSimCacheTreeViewVisibilityWidget> VisibilityWidget;
	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> Children;
	TWeakPtr<SNiagaraSimCacheTreeView> Owner;
	
};

struct FNiagaraSimCacheEmitterTreeItem : public FNiagaraSimCacheTreeItem
{
	
	FNiagaraSimCacheEmitterTreeItem(TWeakPtr<SNiagaraSimCacheTreeView> InOwner): FNiagaraSimCacheTreeItem(InOwner)
	{
	}

	virtual ~FNiagaraSimCacheEmitterTreeItem() override {}
	
	virtual ENiagaraSimCacheOverviewItemType GetType () override { return ENiagaraSimCacheOverviewItemType::Emitter; }
};

struct FNiagaraSimCacheComponentTreeItem : public FNiagaraSimCacheTreeItem
{
	
	FNiagaraSimCacheComponentTreeItem(TWeakPtr<SNiagaraSimCacheTreeView> InOwner): FNiagaraSimCacheTreeItem(InOwner)
	{
	}

	virtual ~FNiagaraSimCacheComponentTreeItem() override {}

	virtual ENiagaraSimCacheOverviewItemType GetType () override { return ENiagaraSimCacheOverviewItemType::Component; }

	virtual TSharedRef<SWidget> GetRowWidget() override
	{
		
		TSharedRef<SHorizontalBox> Contents = SNew(SHorizontalBox);

		if(TypeDef.IsSet())
		{
			FSlateBrush const* IconBrush = TypeDef.GetValue().IsStatic() ? FNiagaraEditorStyle::Get().GetBrush(TEXT("NiagaraEditor.StaticIcon")) : FAppStyle::GetBrush(TEXT("Kismet.AllClasses.VariableIcon"));
			const FLinearColor TypeColor = UEdGraphSchema_Niagara::GetTypeColor(TypeDef.GetValue());
			
			Contents->AddSlot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SScaleBox)
				[
					SNew(SNiagaraIconWidget)
					.IconToolTip(TypeDef.GetValue().GetNameText())
					.IconBrush(IconBrush)
					.IconColor(TypeColor)
					.SecondaryIconBrush(FAppStyle::GetBrush(TEXT("NoBrush")))
					.SecondaryIconColor(TypeColor)
				]
			];
		}
		
		
		Contents->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(5, 0)
		[
			SNew(STextBlock)
			.Text(GetDisplayNameText())
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		];

		Contents->AddSlot()
		.HAlign(HAlign_Right)
		.FillWidth(1.0f)
		[
			SAssignNew(VisibilityWidget, SSimCacheTreeViewVisibilityWidget, SharedThis(this), Owner)
		];
		
		return Contents;
	}

	TOptional<FNiagaraTypeDefinition> TypeDef;
};



