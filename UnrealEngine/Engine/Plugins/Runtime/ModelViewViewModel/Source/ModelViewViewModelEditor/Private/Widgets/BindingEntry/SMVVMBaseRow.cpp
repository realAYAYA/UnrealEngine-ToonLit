// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/BindingEntry/SMVVMBaseRow.h"

#include "Styling/MVVMEditorStyle.h"

#include "Widgets/SNullWidget.h"

namespace UE::MVVM::BindingEntry
{

void SBaseRow::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FWidgetBlueprintEditor>& InBlueprintEditor, UWidgetBlueprint* InBlueprint, const TSharedPtr<FBindingEntry>& InEntry)
{
	BindingEntry = InEntry;
	WeakBlueprint = InBlueprint;
	WeakBlueprintEditor = InBlueprintEditor;

	STableRow<TSharedPtr<FBindingEntry>>::Construct(
		STableRow<TSharedPtr<FBindingEntry>>::FArguments()
		.Padding(2.0f)
		.Style(FMVVMEditorStyle::Get(), GetTableRowStyle())
		[
			BuildRowWidget()
		],
		OwnerTableView
	);
}

TSharedRef<SWidget> SBaseRow::BuildRowWidget()
{
	return SNullWidget::NullWidget;
}
	
UWidgetBlueprint* SBaseRow::GetBlueprint() const
{
	return WeakBlueprint.Get();
}

UMVVMBlueprintView* SBaseRow::GetBlueprintView() const
{
	return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(GetBlueprint());
}

UMVVMEditorSubsystem* SBaseRow::GetEditorSubsystem() const
{
	return GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
}


} // namespace UE::MVVM::BindingEntry
