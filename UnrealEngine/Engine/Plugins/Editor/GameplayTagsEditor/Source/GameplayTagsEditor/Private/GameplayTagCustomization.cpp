// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagCustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "GameplayTagsManager.h"
#include "IDetailChildrenBuilder.h"
#include "GameplayTagsEditorModule.h"
#include "Widgets/Input/SHyperlink.h"
#include "SGameplayTagCombo.h"
#include "SGameplayTagPicker.h"

#define LOCTEXT_NAMESPACE "GameplayTagCustomization"

//---------------------------------------------------------
// FGameplayTagCustomizationPublic
//---------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FGameplayTagCustomizationPublic::MakeInstance()
{
	return MakeShareable(new FGameplayTagCustomization());
}

// Deprecated version.
TSharedRef<IPropertyTypeCustomization> FGameplayTagCustomizationPublic::MakeInstanceWithOptions(const FGameplayTagCustomizationOptions& Options)
{
	return MakeShareable(new FGameplayTagCustomization());
}

//---------------------------------------------------------
// FGameplayTagCustomization
//---------------------------------------------------------

FGameplayTagCustomization::FGameplayTagCustomization()
{
}

void FGameplayTagCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,2,0,1))
		[
			SNew(SGameplayTagCombo)
			.PropertyHandle(StructPropertyHandle)
		]
	];
}

//---------------------------------------------------------
// FGameplayTagCreationWidgetHelperDetails
//---------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FGameplayTagCreationWidgetHelperDetails::MakeInstance()
{
	return MakeShareable(new FGameplayTagCreationWidgetHelperDetails());
}

void FGameplayTagCreationWidgetHelperDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.WholeRowContent()[ SNullWidget::NullWidget ];
}

void FGameplayTagCreationWidgetHelperDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const FString FilterString = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);
	constexpr float MaxPropertyWidth = 480.0f;
	constexpr float MaxPropertyHeight = 240.0f;

	StructBuilder.AddCustomRow(NSLOCTEXT("GameplayTagReferenceHelperDetails", "NewTag", "NewTag"))
		.ValueContent()
		.MaxDesiredWidth(MaxPropertyWidth)
		[
			SAssignNew(TagWidget, SGameplayTagPicker)
			.Filter(FilterString)
			.MultiSelect(false)
			.GameplayTagPickerMode (EGameplayTagPickerMode::ManagementMode)
			.MaxHeight(MaxPropertyHeight)
		];
}

#undef LOCTEXT_NAMESPACE
