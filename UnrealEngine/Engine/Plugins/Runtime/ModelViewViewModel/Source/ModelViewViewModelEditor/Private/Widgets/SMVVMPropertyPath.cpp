// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMPropertyPath.h"

#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Styling/MVVMEditorStyle.h"
#include "Widgets/SMVVMFieldIcon.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPropertyPath"

namespace UE::MVVM
{

void SPropertyPath::Construct(const FArguments& InArgs)
{
	WidgetBlueprint = InArgs._WidgetBlueprint;
	check(InArgs._WidgetBlueprint != nullptr);
	PropertyPath = InArgs._PropertyPath;
	check(PropertyPath);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FMVVMEditorStyle::Get(), "PropertyPath.ContextText")
			.Text(this, &SPropertyPath::GetSourceDisplayName)
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(5, 0, 0, 0))
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SFieldIcon)
			.Field(GetLastField())
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(2, 0, 5, 0))
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FMVVMEditorStyle::Get(), "PropertyPath.ContextText")
			.Text(this, &SPropertyPath::GetFieldDisplayName)
		]
	];
}

FText SPropertyPath::GetSourceDisplayName() const
{
	if (PropertyPath->IsFromWidget())
	{
		const UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(PropertyPath->GetWidgetName());
		return FText::FromString(Widget->GetDisplayLabel());
	}
	else if (PropertyPath->IsFromViewModel())
	{
		const UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (const UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprint.Get()))
		{
			const FMVVMBlueprintViewModelContext* ViewModel = View->FindViewModel(PropertyPath->GetViewModelId());
			return ViewModel->GetDisplayName();
		}
	}
	return FText::GetEmpty();
}

FText SPropertyPath::GetFieldDisplayName() const
{
	return FText::FromString(PropertyPath->GetBasePropertyPath());
}

FMVVMConstFieldVariant SPropertyPath::GetLastField() const
{
	return PropertyPath->GetFields().Last();
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
