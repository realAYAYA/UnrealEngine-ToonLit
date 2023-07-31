// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataRegistryTypeCustomization.h"
#include "DataRegistryEditorModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "DataRegistryEditor"

void FDataRegistryTypeCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	if (StructPropertyHandle.IsValid())
	{
		FName FilterStructName;
		const bool bAllowClear = !(StructPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);

		if (StructPropertyHandle->HasMetaData(FDataRegistryType::ItemStructMetaData))
		{
			const FString& RowType = StructPropertyHandle->GetMetaData(FDataRegistryType::ItemStructMetaData);
			FilterStructName = FName(*RowType);
		}

		HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			PropertyCustomizationHelpers::MakePropertyComboBox(InStructPropertyHandle, FOnGetPropertyComboBoxStrings::CreateStatic(&FDataRegistryEditorModule::GenerateDataRegistryTypeComboBoxStrings, bAllowClear, FilterStructName))
		];
	}
}

void SDataRegistryTypeGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SDataRegistryTypeGraphPin::GetDefaultValueWidget()
{
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	CurrentType = FDataRegistryType(*DefaultString);

	return SNew(SVerticalBox)
		.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			FDataRegistryEditorModule::MakeDataRegistryTypeSelector(
				FOnGetDataRegistryDisplayText::CreateSP(this, &SDataRegistryTypeGraphPin::GetDisplayText),
				FOnSetDataRegistryType::CreateSP(this, &SDataRegistryTypeGraphPin::OnTypeSelected),
				true)
		];
}

void SDataRegistryTypeGraphPin::OnTypeSelected(FDataRegistryType AssetType)
{
	CurrentType = AssetType;
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, CurrentType.ToString());
}

FText SDataRegistryTypeGraphPin::GetDisplayText() const
{
	return FText::AsCultureInvariant(CurrentType.ToString());
}

#undef LOCTEXT_NAMESPACE

