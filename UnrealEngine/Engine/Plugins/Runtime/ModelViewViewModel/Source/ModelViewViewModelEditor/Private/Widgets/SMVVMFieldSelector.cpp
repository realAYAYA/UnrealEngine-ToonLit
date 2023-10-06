// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMFieldSelector.h"

#include "WidgetBlueprint.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Styling/MVVMEditorStyle.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

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
	
	TextStyle = InArgs._TextStyle;
	OnGetPropertyPath = InArgs._OnGetPropertyPath;
	OnGetConversionFunction = InArgs._OnGetConversionFunction;
	OnGetSelectionContext = InArgs._OnGetSelectionContext;
	OnFieldSelectionChanged = InArgs._OnFieldSelectionChanged;
	OnDragEnterEvent = InArgs._OnDragEnter;
	OnDropEvent = InArgs._OnDrop;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(200)
		[
			SAssignNew(ComboButton, SComboButton)
			.ComboButtonStyle(FMVVMEditorStyle::Get(), "FieldSelector.ComboButton")
			.OnGetMenuContent(this, &SFieldSelector::HandleGetMenuContent)
			.ContentPadding(FMargin(4.0f, 2.0f))
			.ButtonContent()
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex(this, &SFieldSelector::GetCurrentDisplayIndex)
				//0-Property/Function (from Widget or Viewmodel).
				//0-Property/Function argument of conversion function
				+ SWidgetSwitcher::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCachedViewBindingPropertyPath, WidgetBlueprint.Get())
					.TextStyle(TextStyle)
					.ShowContext(InArgs._ShowContext)
					.OnGetPropertyPath(InArgs._OnGetPropertyPath)
				]

				//1-Conversion Function
				+ SWidgetSwitcher::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCachedViewBindingConversionFunction, WidgetBlueprint.Get())
					.TextStyle(TextStyle)
					.OnGetConversionFunction(InArgs._OnGetConversionFunction)
				]

				//2-Nothing selected.
				+ SWidgetSwitcher::Slot()
				[
					SNew(SBox)
					.Padding(FMargin(8.0f, 0.0f, 8.0f, 0.0f))
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "HintText")
						.Text(LOCTEXT("None", "No field selected"))
					]
				]
			]
		]
	];
}

int32 SFieldSelector::GetCurrentDisplayIndex() const
{
	if (OnGetConversionFunction.IsBound())
	{
		if (OnGetConversionFunction.Execute() != nullptr)
		{
			return 1;
		}
	}
	if (OnGetPropertyPath.IsBound())
	{
		if (!OnGetPropertyPath.Execute().IsEmpty())
		{
			return 0;
		}
	}
	return 2;
}

TSharedRef<SWidget> SFieldSelector::HandleGetMenuContent()
{
	const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
	if (!WidgetBlueprintPtr)
	{
		return SNullWidget::NullWidget;
	}

	const UFunction* CurrentSelectedFunction = nullptr;
	if (OnGetConversionFunction.IsBound())
	{
		CurrentSelectedFunction = OnGetConversionFunction.Execute();
	}
	TOptional<FMVVMBlueprintPropertyPath> CurrentSelectedPropertyPath;
	if (OnGetPropertyPath.IsBound())
	{
		CurrentSelectedPropertyPath = OnGetPropertyPath.Execute();
	}

	FFieldSelectionContext SelectionContext;
	if (OnGetSelectionContext.IsBound())
	{
		SelectionContext = OnGetSelectionContext.Execute();
	}

	TSharedRef<SFieldSelectorMenu> Menu = SNew(SFieldSelectorMenu, WidgetBlueprintPtr)
		.OnFieldSelectionChanged(this, &SFieldSelector::HandleFieldSelectionChanged)
		.OnMenuCloseRequested(this, &SFieldSelector::HandleMenuClosed)
		.SelectionContext(SelectionContext)
		.CurrentPropertyPathSelected(CurrentSelectedPropertyPath)
		.CurrentFunctionSelected(CurrentSelectedFunction)
		;

	ComboButton->SetMenuContentWidgetToFocus(Menu->GetWidgetToFocus());

	return Menu;
}

void SFieldSelector::HandleFieldSelectionChanged(FMVVMBlueprintPropertyPath PropertyPath, const UFunction* Function)
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	if (OnFieldSelectionChanged.IsBound())
	{
		OnFieldSelectionChanged.Execute(PropertyPath, Function);
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
