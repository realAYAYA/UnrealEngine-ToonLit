// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaLinearGradientSettingsCustomization.h"

#include "AvaTextDefs.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "SResetToDefaultPropertyEditor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaLinearGradientSettingsCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaLinearGradientSettingsCustomization::MakeInstance()
{
	return MakeShared<FAvaLinearGradientSettingsCustomization>();
}

void FAvaLinearGradientSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ColorA_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, ColorA));
	
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent()
	[
		// we will fill this out in customize children, since we can only do it there because we will need the StructBuilder
		SAssignNew(HeaderContentBox, SBox)
	];
}

void FAvaLinearGradientSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> ColorA_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, ColorA));
	
	const TSharedPtr<IPropertyHandle> ColorB_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, ColorB));

	const TSharedPtr<IPropertyHandle> Offset_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, Offset));

	const TSharedPtr<IPropertyHandle> Smoothness_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, Smoothness));

	const TSharedPtr<IPropertyHandle> Rotation_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, Rotation));
	
	Direction_Handle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FAvaLinearGradientSettings, Direction));
	
	auto GetResetButton = [](TSharedPtr<IPropertyHandle> PropertyHandle)
	{
		return
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(5.0f)
			[
				SNew(SResetToDefaultPropertyEditor, PropertyHandle)
			];
			
	};
	
	// we need StructBuilder in order to create some widgets in the HeaderContentBox, so we fill it up here in CustomizeChildren
	
	HeaderContentBox
	->SetContent(
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(3.f, 10.f, 3.f, 10.f))
		.Content()
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			[
				SNew(SGridPanel)
				.FillColumn(1, 1.0)
				.FillColumn(3, 1.0)
				+ SGridPanel::Slot(0, 0)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(LOCTEXT("ColorsFieldName", "Colors"))
				]
				+ SGridPanel::Slot(1, 0)
				.HAlign(HAlign_Fill)
				[
					StructBuilder.GenerateStructValueWidget(ColorA_Handle.ToSharedRef())
				]
				+ SGridPanel::Slot(2, 0)
				.HAlign(HAlign_Fill)
				[
					GetResetButton(ColorA_Handle)
				]
				+ SGridPanel::Slot(3, 0)
				.HAlign(HAlign_Fill)
				[
					StructBuilder.GenerateStructValueWidget(ColorB_Handle.ToSharedRef())
				]
				+ SGridPanel::Slot(4, 0)
				.HAlign(HAlign_Fill)
				[
					GetResetButton(ColorB_Handle)
				]
				+ SGridPanel::Slot(0, 1)
				.HAlign(HAlign_Fill)
				.Padding(0, 0, 5.0, 0)
				[
					Offset_Handle->CreatePropertyNameWidget()
				]
				+ SGridPanel::Slot(1, 1)
				.HAlign(HAlign_Fill)
				.ColumnSpan(3)
				[					
					Offset_Handle->CreatePropertyValueWidget()
				]
				+ SGridPanel::Slot(4, 1)
				[
					GetResetButton(Offset_Handle)
				]
				+ SGridPanel::Slot(0, 2)
				.HAlign(HAlign_Fill)
				.Padding(0, 0, 5.0, 0)
				[
					Smoothness_Handle->CreatePropertyNameWidget()
				]
				+ SGridPanel::Slot(1, 2)
				.HAlign(HAlign_Fill)
				.ColumnSpan(3)
				[					
					Smoothness_Handle->CreatePropertyValueWidget()
				]
				+ SGridPanel::Slot(4, 2)
				.HAlign(HAlign_Fill)
				[
					GetResetButton(Smoothness_Handle)
				]
				+ SGridPanel::Slot(0, 3)
				.HAlign(HAlign_Fill)
				.Padding(0, 0, 5.0, 0)
				[
					Direction_Handle->CreatePropertyNameWidget()
				]
				+ SGridPanel::Slot(1, 3)
				.HAlign(HAlign_Fill)
				.ColumnSpan(3)
				[					
					Direction_Handle->CreatePropertyValueWidget()					
				]
				+ SGridPanel::Slot(4, 3)
				.HAlign(HAlign_Fill)
				[
					GetResetButton(Direction_Handle)
				]
				+ SGridPanel::Slot(0, 4)
				.HAlign(HAlign_Fill)
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.Padding(0, 0, 5.0, 0)
					.Visibility(MakeAttributeLambda([this]{return IsCustomDirectionEnabled() ? EVisibility::Visible : EVisibility::Collapsed;}))
					[
						Rotation_Handle->CreatePropertyNameWidget()
					]
				]
				+ SGridPanel::Slot(1, 4)
				.HAlign(HAlign_Fill)
				.ColumnSpan(3)
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.Visibility(MakeAttributeLambda([this]{return IsCustomDirectionEnabled() ? EVisibility::Visible : EVisibility::Collapsed;}))
					[
						Rotation_Handle->CreatePropertyValueWidget()
					]
				]
				+ SGridPanel::Slot(4, 4)
				.HAlign(HAlign_Fill)
				[	
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.Visibility(MakeAttributeLambda([this]{return IsCustomDirectionEnabled() ? EVisibility::Visible : EVisibility::Collapsed;}))
					[
						GetResetButton(Rotation_Handle)
					]
				]
			]
		]
	);
}

bool FAvaLinearGradientSettingsCustomization::IsCustomDirectionEnabled() const
{
	uint8 DirectionAsUint8;
	Direction_Handle->GetValue(DirectionAsUint8);
	const EAvaGradientDirection CurrDirectionSetting = static_cast<EAvaGradientDirection>(DirectionAsUint8);
	
	return CurrDirectionSetting == EAvaGradientDirection::Custom;
}

FLinearColor FAvaLinearGradientSettingsCustomization::GetLinearColorFromProperty(const TSharedPtr<IPropertyHandle>& InColorPropertyHandle)
{
	// Default to full alpha in case the alpha component is disabled.
	FLinearColor OutColor;

	FString StringValue;
	const FPropertyAccess::Result Result = InColorPropertyHandle->GetValueAsFormattedString(StringValue);

	if (Result == FPropertyAccess::Success)
	{		
		OutColor.InitFromString(StringValue);
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		OutColor = FLinearColor::White;
	}

	return OutColor;
}

#undef LOCTEXT_NAMESPACE
