// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/PCGAttributePropertySelectorDetails.h"

#include "Metadata/PCGAttributePropertySelector.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "PCGAttributePropertySelectorDetails"

void FPCGAttributePropertySelectorDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	check(PropertyHandle.IsValid());

	FText TooltipText = LOCTEXT("TooltipText", "Enter the name of the attribute. You can prefix it by '$' to get a property. Red text means that your attribute or property is invalid.");

	auto Validation = [this]() -> FSlateColor
	{
		FPCGAttributePropertySelector* Selector = GetStruct();
		return (Selector && !Selector->IsValid()) ? FStyleColors::AccentRed : FSlateColor::UseForeground();
	};

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(350.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SEditableTextBox)
				.Text(this, &FPCGAttributePropertySelectorDetails::GetText)
				.ToolTipText(TooltipText)
				.OnTextCommitted(this, &FPCGAttributePropertySelectorDetails::SetText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ForegroundColor_Lambda(Validation)
			]
			+ SHorizontalBox::Slot()
			.MaxWidth(20.0f)
			[
				SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnGetMenuContent(this, &FPCGAttributePropertySelectorDetails::GenerateExtraMenu)
				.HasDownArrow(false)
				.ButtonContent()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
			]
		];
}

TSharedRef<SWidget> FPCGAttributePropertySelectorDetails::GenerateExtraMenu()
{
	// Clear the focus on the text box. It is preventing from updating the text.
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("Attributes", LOCTEXT("AttributesHeader", "Attributes"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LastAttributeHeader", "Last Attribute(None)"),
			FText(),
			FSlateIcon(),
			FExecuteAction::CreateSP(this, &FPCGAttributePropertySelectorDetails::SetAttributeName, FName()),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("PointProperties", LOCTEXT("PointPropertiesHeader", "Point Properties"));
	{
		if (const UEnum* EnumPtr = StaticEnum<EPCGPointProperties>())
		{
			for (int32 i = 0; i < EnumPtr->NumEnums() - 1; ++i)
			{
				FString EnumName = EnumPtr->GetNameStringByIndex(i);
				EPCGPointProperties EnumValue = (EPCGPointProperties)EnumPtr->GetValueByIndex(i);

				MenuBuilder.AddMenuEntry(
					FText::FromString(EnumName),
					FText(),
					FSlateIcon(),
					FExecuteAction::CreateSP(this, &FPCGAttributePropertySelectorDetails::SetPointProperty, EnumValue),
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FPCGAttributePropertySelector* FPCGAttributePropertySelectorDetails::GetStruct()
{
	void* Data = nullptr;
	FPropertyAccess::Result Result = PropertyHandle->GetValueData(Data);

	return (Result == FPropertyAccess::Success) ? reinterpret_cast<FPCGAttributePropertySelector*>(Data) : nullptr;
}

const FPCGAttributePropertySelector* FPCGAttributePropertySelectorDetails::GetStruct() const
{
	void* Data = nullptr;
	FPropertyAccess::Result Result = PropertyHandle->GetValueData(Data);

	return (Result == FPropertyAccess::Success) ? reinterpret_cast<const FPCGAttributePropertySelector*>(Data) : nullptr;
}

FText FPCGAttributePropertySelectorDetails::GetText() const
{
	const FPCGAttributePropertySelector* Selector = GetStruct();

	return Selector ? Selector->GetDisplayText() : FText();
}

void FPCGAttributePropertySelectorDetails::SetText(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::Type::OnCleared)
	{
		return;
	}

	if (FPCGAttributePropertySelector* Selector = GetStruct())
	{
		FScopedTransaction Transaction(LOCTEXT("SetAttributePropertyName", "[PCG] Set Attribute/Property Name"));

		PropertyHandle->NotifyPreChange();
		if (!Selector->Update(NewText.ToString()))
		{
			Transaction.Cancel();
			return;
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

void FPCGAttributePropertySelectorDetails::SetPointProperty(EPCGPointProperties EnumValue)
{
	if (FPCGAttributePropertySelector* Selector = GetStruct())
	{
		FScopedTransaction Transaction(LOCTEXT("SetAttributePropertyName", "[PCG] Set Attribute/Property Name"));

		PropertyHandle->NotifyPreChange();
		if (!Selector->SetPointProperty(EnumValue))
		{
			Transaction.Cancel();
			return;
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

void FPCGAttributePropertySelectorDetails::SetAttributeName(FName NewName)
{
	if (FPCGAttributePropertySelector* Selector = GetStruct())
	{
		FScopedTransaction Transaction(LOCTEXT("SetAttributePropertyName", "[PCG] Set Attribute/Property Name"));

		PropertyHandle->NotifyPreChange();
		if (!Selector->SetAttributeName(NewName))
		{
			Transaction.Cancel();
			return;
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

#undef LOCTEXT_NAMESPACE
