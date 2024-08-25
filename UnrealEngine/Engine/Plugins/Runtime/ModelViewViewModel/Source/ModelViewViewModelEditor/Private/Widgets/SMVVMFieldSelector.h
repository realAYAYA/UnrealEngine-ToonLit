// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SMVVMFieldDisplay.h"
#include "Widgets/SMVVMFieldSelectorMenu.h"

namespace UE::MVVM { class SCachedViewBindingPropertyPath; }

class SComboButton;

namespace UE::MVVM
{

class SFieldSelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(FFieldSelectionContext, FOnGetSelectionContext);
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnDrop, const FGeometry&, const FDragDropEvent&);
	DECLARE_DELEGATE_TwoParams(FOnDragEnter, const FGeometry&, const FDragDropEvent&);

	SLATE_BEGIN_ARGS(SFieldSelector)
		: _TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		{
		}
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ARGUMENT_DEFAULT(bool, ShowContext) { true };
		SLATE_ARGUMENT_DEFAULT(bool, IsBindingToEvent) { false };
		SLATE_EVENT(SFieldDisplay::FOnGetLinkedPinValue, OnGetLinkedValue)
		SLATE_EVENT(FOnLinkedValueSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnGetSelectionContext, OnGetSelectionContext)
		SLATE_EVENT(FOnDrop, OnDrop)
		SLATE_EVENT(FOnDragEnter, OnDragEnter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint);
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

private:
	TSharedRef<SWidget> CreateSourcePanel();
	TSharedRef<SWidget> HandleGetMenuContent();

	void HandleFieldSelectionChanged(FMVVMLinkedPinValue NewValue);
	void HandleMenuClosed();

private:
	TSharedPtr<SCachedViewBindingPropertyPath> PropertyPathWidget;
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SFieldDisplay> FieldDisplay;
	TWeakObjectPtr<const UWidgetBlueprint> WidgetBlueprint;
	FOnLinkedValueSelectionChanged OnSelectionChanged;
	FOnGetSelectionContext OnGetSelectionContext;
	FOnDrop OnDropEvent;
	FOnDragEnter OnDragEnterEvent;
	bool bIsBindingToEvent = false;
}; 

} // namespace UE::MVVM
