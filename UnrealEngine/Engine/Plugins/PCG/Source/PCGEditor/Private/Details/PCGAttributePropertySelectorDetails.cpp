// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/PCGAttributePropertySelectorDetails.h"

#include "PCGCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"

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

namespace PCGAttributePropertySelectorDetails
{
	static const FText SetAttributePropertyNameTransaction = LOCTEXT("SetAttributePropertyName", "[PCG] Set Attribute/Property Name");

	template <typename EnumType>
	void BuildMenuSectionFromEnum(FMenuBuilder& InMenuBuilder, FPCGAttributePropertySelectorDetails* InDetailsObject, void(FPCGAttributePropertySelectorDetails::*InCallback)(EnumType))
	{
		if (const UEnum* EnumPtr = StaticEnum<EnumType>())
		{
			for (int32 i = 0; i < EnumPtr->NumEnums() - 1; ++i)
			{
				FString EnumName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
				EnumType EnumValue = (EnumType)EnumPtr->GetValueByIndex(i);

				InMenuBuilder.AddMenuEntry(
					FText::FromString(EnumName),
					FText(),
					FSlateIcon(),
					FExecuteAction::CreateSP(InDetailsObject, InCallback, EnumValue),
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
		}
	}

	// Func signature: bool(FPCGAttributePropertySelector*, T)
	template <typename T, typename Func>
	void SetValue(FPCGAttributePropertySelector* InSelector, TSharedPtr<IPropertyHandle>& InPropertyHandle, T InValue, Func InCallback)
	{
		if (InSelector)
		{
			FScopedTransaction Transaction(SetAttributePropertyNameTransaction);

			InPropertyHandle->NotifyPreChange();
			if (!InCallback(InSelector, std::move(InValue)))
			{
				Transaction.Cancel();
				return;
			}

			InPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}
}

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

	TSharedRef<SWidget> PropertyNameWidget = PropertyHandle->CreatePropertyNameWidget();
	PropertyNameWidget->SetEnabled(TAttribute<bool>(this, &FPCGAttributePropertySelectorDetails::IsEnabled));

	HeaderRow
		.NameContent()
		[
			PropertyNameWidget
		]
		.ValueContent()
		.MinDesiredWidth(350.0f)
		[
			SNew(SHorizontalBox)
			.IsEnabled_Raw(this, &FPCGAttributePropertySelectorDetails::IsEnabled)
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
				.Visibility_Raw(this, &FPCGAttributePropertySelectorDetails::ExtraMenuVisibility)
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

EVisibility FPCGAttributePropertySelectorDetails::ExtraMenuVisibility() const
{
	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty());

	const bool bIsVisible = StructProperty &&
		(StructProperty->Struct == FPCGAttributePropertyInputSelector::StaticStruct() ||
			StructProperty->Struct == FPCGAttributePropertyOutputSelector::StaticStruct() ||
			!StructProperty->HasMetaData(PCGObjectMetadata::DiscardPropertySelection) ||
			!StructProperty->HasMetaData(PCGObjectMetadata::DiscardExtraSelection));

	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

bool FPCGAttributePropertySelectorDetails::IsEnabled() const
{
	if (!PropertyHandle->IsEditable())
	{
		return false;
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty());
	TArray<UObject*> Outers;
	PropertyHandle->GetOuterObjects(Outers);
	for (const UObject* Outer : Outers)
	{
		if (Outer && !Outer->CanEditChange(StructProperty))
		{
			return false;
		}
	}

	return true;
}

TSharedRef<SWidget> FPCGAttributePropertySelectorDetails::GenerateExtraMenu()
{
	// Clear the focus on the text box. It is preventing from updating the text.
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	FMenuBuilder MenuBuilder(true, nullptr);

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty());

	MenuBuilder.BeginSection("Attributes", LOCTEXT("AttributesHeader", "Attributes"));
	{
		if (StructProperty->Struct == FPCGAttributePropertyInputSelector::StaticStruct())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("LastAttributeHeader", "Last Attribute"),
				LOCTEXT("LastAttributeTooltip", "Refer to the last modified attribute. You can check it in the input inspection data."),
				FSlateIcon(),
				FExecuteAction::CreateSP(this, &FPCGAttributePropertySelectorDetails::SetAttributeName, PCGMetadataAttributeConstants::LastAttributeName),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}

		if (StructProperty->Struct == FPCGAttributePropertyOutputSelector::StaticStruct())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SourceAttributeHeader", "Source Attribute"),
				LOCTEXT("SourceAttributeTooltip", "Refer to the same attribute that will be used as input."),
				FSlateIcon(),
				FExecuteAction::CreateSP(this, &FPCGAttributePropertySelectorDetails::SetAttributeName, PCGMetadataAttributeConstants::SourceAttributeName),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
	}
	MenuBuilder.EndSection();

	if (!StructProperty->HasMetaData(PCGObjectMetadata::DiscardPropertySelection))
	{
		MenuBuilder.BeginSection("PointProperties", LOCTEXT("PointPropertiesHeader", "Point Properties"));
		{
			PCGAttributePropertySelectorDetails::BuildMenuSectionFromEnum<EPCGPointProperties>(MenuBuilder, this, &FPCGAttributePropertySelectorDetails::SetPointProperty);
		}
		MenuBuilder.EndSection();
	}

	if (!StructProperty->HasMetaData(PCGObjectMetadata::DiscardExtraSelection))
	{
		MenuBuilder.BeginSection("OtherProperties", LOCTEXT("OtherPropertiesHeader", "Other Properties"));
		{
			PCGAttributePropertySelectorDetails::BuildMenuSectionFromEnum<EPCGExtraProperties>(MenuBuilder, this, &FPCGAttributePropertySelectorDetails::SetExtraProperty);
		}
		MenuBuilder.EndSection();
	}

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

	PCGAttributePropertySelectorDetails::SetValue(GetStruct(), PropertyHandle, NewText.ToString(), [](FPCGAttributePropertySelector* InStruct, FString InValue) -> bool { return InStruct && InStruct->Update(std::move(InValue)); });
}

void FPCGAttributePropertySelectorDetails::SetPointProperty(EPCGPointProperties EnumValue)
{
	PCGAttributePropertySelectorDetails::SetValue(GetStruct(), PropertyHandle, EnumValue, [](FPCGAttributePropertySelector* InStruct, EPCGPointProperties InValue) -> bool { return InStruct && InStruct->SetPointProperty(InValue); });
}

void FPCGAttributePropertySelectorDetails::SetAttributeName(FName NewName)
{
	PCGAttributePropertySelectorDetails::SetValue(GetStruct(), PropertyHandle, NewName, [](FPCGAttributePropertySelector* InStruct, FName InValue) -> bool { return InStruct && InStruct->SetAttributeName(InValue); });
}

void FPCGAttributePropertySelectorDetails::SetExtraProperty(EPCGExtraProperties EnumValue)
{
	PCGAttributePropertySelectorDetails::SetValue(GetStruct(), PropertyHandle, EnumValue, [](FPCGAttributePropertySelector* InStruct, EPCGExtraProperties InValue) -> bool { return InStruct && InStruct->SetExtraProperty(InValue); });
}

#undef LOCTEXT_NAMESPACE
