// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Layers/SDMLayerEffectsItem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Views/SListView.h"

class SDMLayerEffectsItem;
class UDMMaterialEffectStack;

DECLARE_DELEGATE_TwoParams(FDMOnEffectsViewItemSelected, TSharedPtr<FDMEffectsLayerItem> /** InLayerItem */, const int32 /** InLayerIndex */)

class SDMLayerEffectsView : public SListView<TSharedPtr<FDMEffectsLayerItem>>
{
public:
	SLATE_BEGIN_ARGS(SDMLayerEffectsView)
	{}
		SLATE_EVENT(FDMOnEffectsViewItemSelected, OnEffectSelected)
	SLATE_END_ARGS()

	virtual ~SDMLayerEffectsView() override {}

	void Construct(const FArguments& InArgs, const TSharedPtr<SDMSlot>& InSlotWidget, UDMMaterialEffectStack* InMaterialEffectStack);

	void ScrollItemIntoView(const TSharedPtr<FDMEffectsLayerItem>& InItem);
	void FocusOnItem(const TSharedPtr<FDMEffectsLayerItem>& InItem);

	int32 GetLayerItemIndex(const TSharedPtr<FDMEffectsLayerItem>& InItem) const;

	const TArray<TSharedPtr<FDMEffectsLayerItem>>& GetSelectedEffectItems() const { return SelectedEffectItems; }
	const TArray<TSharedPtr<SDMLayerEffectsItem>>& GetSelectedEffectWidgets() const { return SelectedEffectWidgets; }

	TSharedPtr<SDMLayerEffectsItem> WidgetFromLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InItem);

	bool AddLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InLayerItem);
	bool RemoveLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InLayerItem);

	int32 GetLayerItemCount() const { return EffectItems.Num(); }

	//~ Begin SListView
	virtual void RebuildList() override;
	//~ End SListView

	//~ Begin SWidget
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget

protected:
	TWeakPtr<SDMSlot> SlotWidgetWeak;
	TWeakObjectPtr<UDMMaterialEffectStack> MaterialEffectStackWeak;

	FDMOnEffectsViewItemSelected OnEffectSelected;
	FOnContextMenuOpening OnContextMenuOpening;

	TArray<TSharedPtr<FDMEffectsLayerItem>> EffectItems;

	int32 SelectedEffectIndex = INDEX_NONE;

	TArray<TSharedPtr<FDMEffectsLayerItem>> SelectedEffectItems;
	TArray<TSharedPtr<SDMLayerEffectsItem>> SelectedEffectWidgets;

	TSharedRef<ITableRow> OnGenerateLayerItemWidget(TSharedPtr<FDMEffectsLayerItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	void OnLayerItemSelectionChanged(TSharedPtr<FDMEffectsLayerItem> InSelectedItem, ESelectInfo::Type InSelectInfo);

	void OnLayerEffectSelected(const bool bInSelected, const TSharedRef<SDMLayerEffectsItem>& InEffectsItemWidget);

	void OnEffectStackUpdate(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
};
