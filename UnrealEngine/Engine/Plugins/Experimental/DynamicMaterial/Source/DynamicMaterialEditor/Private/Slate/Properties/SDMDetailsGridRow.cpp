// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMDetailsGridRow.h"
#include "Components/DMMaterialValue.h"
#include "PropertyCustomizationHelpers.h"
#include "Slate/Properties/SDMPropertyEdit.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "SDMDetailsGridRow"

void SDMDetailsGridRow::Construct(const FArguments& InArgs)
{
	MinColumnWidth = InArgs._MinColumnWidth;
	RowSeparatorColor = InArgs._RowSeparatorColor;
	bResizeable = InArgs._Resizeable;
	bShowSeparators = InArgs._ShowSeparators;
	SeparatorColor = InArgs._SeparatorColor;
	UseEvenOddRowBackgroundColors = InArgs._UseEvenOddRowBackgroundColors;
	OddRowBackgroundColor = InArgs._OddRowBackgroundColor;
	EvenRowBackgroundColor = InArgs._EvenRowBackgroundColor;

	LabelColumnBox = SNew(SVerticalBox);
	ContentColumnBox = SNew(SVerticalBox);
	ExtensionsColumnBox = SNew(SVerticalBox);

	ChildSlot
		[
			SNew(SSplitter)
			.Style(FAppStyle::Get(), "DetailsView.Splitter")
			+ SSplitter::Slot()
			.Resizable(InArgs._Resizeable)
			.Value(0.3f)
			[
				LabelColumnBox.ToSharedRef()
			]
			+ SSplitter::Slot()
			.Resizable(InArgs._Resizeable)
			.Value(0.7f)
			[
				ContentColumnBox.ToSharedRef()
			]
			+ SSplitter::Slot()
			.Resizable(false)
			[
				ExtensionsColumnBox.ToSharedRef()
			]
		];
}

void SDMDetailsGridRow::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InObject;
	ExtensionRowArgs.Property = InPropertyHandle->GetProperty();
	ExtensionRowArgs.PropertyPath = TEXT("Value");
	ExtensionRowArgs.PropertyHandle = InPropertyHandle;

	AddPropertyRow(InObject, InPropertyHandle, SNew(SDMPropertyEdit).PropertyHandle(InPropertyHandle), ExtensionRowArgs);
}

void SDMDetailsGridRow::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget)
{
	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InObject;
	ExtensionRowArgs.Property = InPropertyHandle->GetProperty();
	ExtensionRowArgs.PropertyPath = TEXT("Value");
	ExtensionRowArgs.PropertyHandle = InPropertyHandle;

	AddPropertyRow(InObject, InPropertyHandle, InEditWidget, ExtensionRowArgs);
}

void SDMDetailsGridRow::AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget,
	const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs)
{
	if (!IsValid(InObject) || !InPropertyHandle.IsValid() || !InPropertyHandle->IsValidHandle())
	{
		return;
	}

	const FText DisplayNameText = InPropertyHandle->GetPropertyDisplayName();

	// ?! No idea why this is commented out.
	//AddPropertyRow(DisplayNameText, InEditWidget, CreateExtensionButtons(InObject, InPropertyHandle, InExtensionArgs));
}

void SDMDetailsGridRow::AddPropertyRow(const FText& InText, const TSharedRef<SWidget>& InContent, const TSharedRef<SWidget>& InExtensions)
{
	const FMargin SeparatorPadding = FMargin(5.0f, 0.0f, 5.0f, 0.0f);

	TSharedRef<SHorizontalBox> LabelWidget = SNew(SHorizontalBox);

	LabelWidget->AddSlot()
		.AutoWidth()
		[
			InExtensions
		];

	if (bShowSeparators)
	{
		LabelWidget->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			.Padding(SeparatorPadding)
			[
				SNew(SColorBlock)
				.Color(SeparatorColor)
			];
	}

	LabelColumnBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InText)
		];

	ContentColumnBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			InContent
		];

	TSharedRef<SHorizontalBox> ExtensionsWidget = SNew(SHorizontalBox);

	if (bShowSeparators)
	{
		ExtensionsWidget->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			.Padding(SeparatorPadding)
			[
				SNew(SColorBlock)
				.Color(SeparatorColor)
			];
	}

	ExtensionsWidget->AddSlot()
		.AutoWidth()
		[
			InExtensions
		];

	ExtensionsColumnBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			ExtensionsWidget
		];
}

#undef LOCTEXT_NAMESPACE
