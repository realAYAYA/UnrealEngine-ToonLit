// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaViewportQualitySettingsPropertyTypeCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Viewport/AvaViewportQualitySettings.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaViewportQualitySettingsPropertyTypeCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaViewportQualitySettingsPropertyTypeCustomization::MakeInstance()
{
	return MakeShared<FAvaViewportQualitySettingsPropertyTypeCustomization>();
}

void FAvaViewportQualitySettingsPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	InHeaderRow.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FAvaViewportQualitySettingsPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InDetailBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> FeaturesProperty = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaViewportQualitySettings, Features));

	const TSharedRef<FDetailArrayBuilder> FeaturesArrayBuilder = MakeShared<FDetailArrayBuilder>(FeaturesProperty.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false);

	static const bool bDisplayDefaultPropertyButtons = false;

	FeaturesArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> InElementPropertyHandle, int32 InArrayIndex, IDetailChildrenBuilder& InChildrenBuilder)
	{
		TSharedPtr<IPropertyHandle> NameProperty  = InElementPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaViewportQualitySettingsFeature, Name));
		TSharedPtr<IPropertyHandle> ValueProperty = InElementPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaViewportQualitySettingsFeature, Enabled));

		FString FeatureName;
		NameProperty->GetValue(FeatureName);

		FText NameText, TooltipText;
		FAvaViewportQualitySettings::FeatureNameAndTooltipText(FeatureName, NameText, TooltipText);

		InChildrenBuilder.AddProperty(InElementPropertyHandle)
			.ToolTip(TooltipText)
			.CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(NameText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				ValueProperty->CreatePropertyValueWidget(bDisplayDefaultPropertyButtons)
			];
	}));

	InDetailBuilder.AddCustomBuilder(FeaturesArrayBuilder);
}

#undef LOCTEXT_NAMESPACE
