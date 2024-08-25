// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Reply.h"
#include "Types/MVVMBindingSource.h"
#include "Types/MVVMExecutionMode.h"
#include "Widgets/BindingEntry/SMVVMBaseRow.h"
#include "Widgets/SMVVMFieldSelectorMenu.h"


namespace UE::MVVM::BindingEntry
{

/**
 *
 */
class SEventRow : public SBaseRow
{
public:
	SLATE_BEGIN_ARGS(SEventRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FWidgetBlueprintEditor>& InBlueprintEditor, UWidgetBlueprint* InBlueprint, const TSharedPtr<FBindingEntry>& InEntry);

protected:
	virtual TSharedRef<SWidget> BuildRowWidget() override;
	virtual const ANSICHAR* GetTableRowStyle() const override
	{
		return "BindingView.BindingRow";
	}

private:
	UMVVMBlueprintViewEvent* GetEvent() const;

	FSlateColor GetErrorBorderColor() const;

	EVisibility GetErrorButtonVisibility() const;

	FText GetErrorButtonToolTip() const;

	FReply OnErrorButtonClicked();

	TSharedRef<ITableRow> OnGenerateErrorRow(TSharedPtr<FText> Text, const TSharedRef<STableViewBase>& TableView) const;

	ECheckBoxState IsEventEnabled() const;

	void OnIsEventEnableChanged(ECheckBoxState NewState);

	ECheckBoxState IsEventCompiled() const;

	void OnIsEventCompileChanged(ECheckBoxState NewState);

	FMVVMLinkedPinValue GetFieldSelectedValue(bool bSource) const;

	void HandleFieldSelectionChanged(FMVVMLinkedPinValue Value, bool bSource);

	FFieldSelectionContext GetSelectedSelectionContext(bool bSource) const;

	FReply HandleFieldSelectorDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bSource);

	void HandleFieldSelectorDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bSource);

	TSharedRef<SWidget> HandleContextMenu() const;
	void HandleShowBlueprintGraph() const;

private:
	TArray<TSharedPtr<FText>> ErrorItems;
};

} // namespace UE::MVVM
