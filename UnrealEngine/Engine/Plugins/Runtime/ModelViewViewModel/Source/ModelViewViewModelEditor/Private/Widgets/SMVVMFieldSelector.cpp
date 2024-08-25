// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMFieldSelector.h"

#include "WidgetBlueprint.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Styling/MVVMEditorStyle.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "MVVMFieldSelector"

namespace UE::MVVM
{

namespace Private
{

FBindingSource GetSourceFromPath(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Path)
{
	return FBindingSource::CreateFromPropertyPath(WidgetBlueprint, Path);
}

} // namespace Private

void SFieldSelector::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	check(InWidgetBlueprint);

	OnSelectionChanged = InArgs._OnSelectionChanged;
	OnGetSelectionContext = InArgs._OnGetSelectionContext;
	OnDragEnterEvent = InArgs._OnDragEnter;
	OnDropEvent = InArgs._OnDrop;
	bIsBindingToEvent = InArgs._IsBindingToEvent;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(200.0f)
		[
			SAssignNew(ComboButton, SComboButton)
			.ComboButtonStyle(FMVVMEditorStyle::Get(), "FieldSelector.ComboButton")
			.OnGetMenuContent(this, &SFieldSelector::HandleGetMenuContent)
			.ContentPadding(FMargin(4.0f, 2.0f))
			.ButtonContent()
			[
				SAssignNew(FieldDisplay, SFieldDisplay, InWidgetBlueprint)
				.TextStyle(InArgs._TextStyle)
				.OnGetLinkedValue(InArgs._OnGetLinkedValue)
			]
		]
	];
}

TSharedRef<SWidget> SFieldSelector::HandleGetMenuContent()
{
	const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
	if (!WidgetBlueprintPtr || !FieldDisplay)
	{
		return SNullWidget::NullWidget;
	}

	TOptional<FMVVMLinkedPinValue> CurrentSelected;
	if (FieldDisplay->OnGetLinkedValue.IsBound())
	{
		CurrentSelected = FieldDisplay->OnGetLinkedValue.Execute();
	}

	FFieldSelectionContext SelectionContext;
	if (OnGetSelectionContext.IsBound())
	{
		SelectionContext = OnGetSelectionContext.Execute();
	}

	TSharedRef<SFieldSelectorMenu> Menu = SNew(SFieldSelectorMenu, WidgetBlueprintPtr)
		.CurrentSelected(CurrentSelected)
		.OnSelectionChanged(this, &SFieldSelector::HandleFieldSelectionChanged)
		.OnMenuCloseRequested(this, &SFieldSelector::HandleMenuClosed)
		.SelectionContext(SelectionContext)
		.IsBindingToEvent(bIsBindingToEvent)
		;

	ComboButton->SetMenuContentWidgetToFocus(Menu->GetWidgetToFocus());

	return Menu;
}

void SFieldSelector::HandleFieldSelectionChanged(FMVVMLinkedPinValue LinkedValue)
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(LinkedValue);
	}
}

void SFieldSelector::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnDragEnterEvent.IsBound())
	{
		OnDragEnterEvent.Execute(MyGeometry, DragDropEvent);
	}
}

void SFieldSelector::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>())
	{
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
}

FReply SFieldSelector::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnDropEvent.IsBound())
	{
		return OnDropEvent.Execute(MyGeometry, DragDropEvent);
	}
	return FReply::Unhandled();
}

void SFieldSelector::HandleMenuClosed()
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
