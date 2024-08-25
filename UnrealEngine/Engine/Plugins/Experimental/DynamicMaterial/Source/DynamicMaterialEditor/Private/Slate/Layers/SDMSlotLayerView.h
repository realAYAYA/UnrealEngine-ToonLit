// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialSlot.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "EditorUndoClient.h"
#include "Slate/Layers/SDMSlotLayerItem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Views/SListView.h"

class SDMLayerEffectsItem;
class SDMSlot;
class UDMMaterialSlot;
class UDMMaterialStage;

DECLARE_DELEGATE_TwoParams(FDMOnLayerViewItemSelected, TSharedPtr<FDMMaterialLayerReference> /** InLayerItem */, const int32 /** InLayerIndex */)

class SDMSlotLayerView : public SListView<TSharedPtr<FDMMaterialLayerReference>>, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SDMSlotLayerView)
	{}
		SLATE_ATTRIBUTE(int32, PreviewSize)
		SLATE_EVENT(FDMOnLayerViewItemSelected, OnLayerSelected)
		SLATE_EVENT(FDMOnStageSelected, OnLayerStageSelected)
	SLATE_END_ARGS()

	virtual ~SDMSlotLayerView() override {}

	void Construct(const FArguments& InArgs, const TSharedRef<SDMSlot>& InSlotWidget);

	TSharedPtr<SDMSlot> GetSlotWidget() const { return SlotWidgetWeak.Pin(); }

	void ScrollItemIntoView(const TSharedPtr<FDMMaterialLayerReference>& InItem);
	void FocusOnItem(const TSharedPtr<FDMMaterialLayerReference>& InItem);

	int32 GetLayerItemIndex(const TSharedPtr<FDMMaterialLayerReference>& InItem) const;

	const TArray<TSharedPtr<FDMMaterialLayerReference>>& GetLayerItems() const { return LayerItems; }

	int32 GetSelectedLayerIndex() const { return SelectedLayerIndex; }
	const TArray<TWeakPtr<SDMStage>>& GetSelectedStageWidgets() const { return SelectedStageWidgets; }

	TSharedPtr<SDMSlotLayerItem> WidgetFromLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InItem);

	TSharedPtr<FDMMaterialLayerReference> FindLayerItem(UDMMaterialStage* const InStage) const;
	TSharedPtr<FDMMaterialLayerReference> FindLayerItem(UDMMaterialLayerObject* const InLayer) const;

	void SelectLayerItem(UDMMaterialStage* const InStage, const bool bInSelected, const ESelectInfo::Type InSelectInfo);
	void SelectLayerItem(UDMMaterialLayerObject* const InLayer, const bool bInMask, const bool bInSelected, const ESelectInfo::Type InSelectInfo);

	bool AddLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InLayerItem);
	bool RemoveLayerItem(const TSharedPtr<FDMMaterialLayerReference>& InLayerItem);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget

	virtual void RequestListRefresh() override;

	//~ Begin FUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FUndoClient

protected:
	TWeakPtr<SDMSlot> SlotWidgetWeak;
	TWeakObjectPtr<UDMMaterialSlot> MaterialSlotWeak;

	TAttribute<int32> PreviewSize;
	FDMOnLayerViewItemSelected LayerSelected;
	FDMOnStageSelected LayerStageSelected;

	TArray<TSharedPtr<FDMMaterialLayerReference>> LayerItems;
	TArray<TMap<EDMMaterialLayerStage, TWeakPtr<SDMStage>>> StageWidgets;

	int32 SelectedLayerIndex = INDEX_NONE;

	TArray<TWeakPtr<SDMStage>> SelectedStageWidgets;

	TArray<TWeakObjectPtr<UDMMaterialStage>> SelectedStages;

	bool bPostRegenSelect = false;
	int32 PostRegenSelectedLayerIndex = INDEX_NONE;
	TArray<TWeakObjectPtr<UDMMaterialStage>> PostRegenSelectedStages;

	TSharedRef<ITableRow> OnGenerateLayerItemWidget(TSharedPtr<FDMMaterialLayerReference> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	void OnLayerItemSelectionChanged(TSharedPtr<FDMMaterialLayerReference> InSelectedItem, ESelectInfo::Type InSelectInfo);

	TSharedPtr<SWidget> CreateLayerItemContextMenu();

	void OnLayerStageSelected(const bool bInSelected, const TSharedRef<SDMStage>& InStageWidget);

	void OnLayerLinkToggled(const bool bIsLinkActive, const TSharedRef<SDMStage>& InStageWidget);

	void Internal_SelectLayers(const TArray<TSharedPtr<FDMMaterialLayerReference>>& InSelectedLayers, UDMMaterialStage* InStage = nullptr);

	void RegenerateItems();

	void BindCommands();

	bool CanMoveLayer(int32 InOffset) const;

	void ExecuteMoveLayer(int32 InOffset);

	bool CanSelectLayerStage(EDMMaterialLayerStage InStageType) const;

	void ExecuteSelectLayerStage(EDMMaterialLayerStage InStageType);

	void OnUndo();
};
