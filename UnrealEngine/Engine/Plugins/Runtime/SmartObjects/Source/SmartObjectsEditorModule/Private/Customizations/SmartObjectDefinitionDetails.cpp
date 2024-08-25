// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/SmartObjectDefinitionDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "SmartObjectDefinition.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

TSharedRef<IDetailCustomization> FSmartObjectDefinitionDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectDefinitionDetails);
}

void FSmartObjectDefinitionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedPtr<IPropertyHandle> SlotsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_STRING_CHECKED(USmartObjectDefinition, Slots));
	const TSharedPtr<IPropertyHandle> DefinitionDataProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_STRING_CHECKED(USmartObjectDefinition, DefinitionData));

	// Special header for the defintion data. Need to do this before EditCategory(), or the default property is not accessible.
	if (IDetailPropertyRow* DefinitionDataRow = DetailBuilder.EditDefaultProperty(DefinitionDataProperty))
	{
		DefinitionDataRow->CustomWidget(true)
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
				.Text(DefinitionDataProperty->GetPropertyDisplayName())
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				DefinitionDataProperty->CreateDefaultPropertyButtonWidgets()
			]
		];
	}
	
	IDetailCategoryBuilder& SmartObjectCategory = DetailBuilder.EditCategory(TEXT("SmartObject"));
	SmartObjectCategory.SetSortOrder(0);

	// Slots as category
	MakeArrayCategory(DetailBuilder, "Slots", LOCTEXT("SmartObjectDefinitionSlots", "Slots"), 1, SlotsProperty);
}

void FSmartObjectDefinitionDetails::MakeArrayCategory(IDetailLayoutBuilder& DetailBuilder, FName CategoryName, const FText& DisplayName, int32 SortOrder, TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, DisplayName);
	Category.SetSortOrder(SortOrder);

	const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox);
	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		PropertyHandle->CreateDefaultPropertyButtonWidgets()
	];
	Category.HeaderContent(HeaderContentWidget);

	// Add items inline
	const TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(PropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		ChildrenBuilder.AddProperty(PropertyHandle);
	}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}

#undef LOCTEXT_NAMESPACE
