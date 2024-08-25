// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOConfigurationCustomization.h"

#include "OpenColorIOColorSpaceCustomization.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOWrapper.h"
#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "OpenColorIOConfigurationCustomization"


void FOpenColorIOConfigurationCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() != 1)
	{
		return;
	}

	TSharedRef<IPropertyHandle> ConfigurationFileProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile));
	IDetailCategoryBuilder& ConfigCategory = DetailBuilder.EditCategory("Config");
	ConfigCategory.AddProperty(DetailBuilder.GetProperty((GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile))));

	DetailBuilder.AddCustomRowToCategory(ConfigurationFileProperty, LOCTEXT("Config", "Reload and Rebuild"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("ButtonCategory", "Rebuild from Raw Config"))
		]
		.ValueContent()
		[
			SNew(SButton)
			.Text(LOCTEXT("ButtonName", "Reload and Rebuild"))
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("ButtonToolTip", "Reload the current OCIO config file and rebuild shaders."))
			.OnClicked_Lambda([Objects]()
			{
				if(UOpenColorIOConfiguration* OpenColorIOConfig = Cast<UOpenColorIOConfiguration>(Objects[0].Get()))
				{
					OpenColorIOWrapper::ClearAllCaches();

					OpenColorIOConfig->ReloadExistingColorspaces(true);
				}
				return FReply::Handled();
			})
		];

	TSharedPtr<IPropertyHandle> ConfigurationObjectHandle = ConfigurationFileProperty->GetParentHandle();

	DetailBuilder.RegisterInstancedCustomPropertyTypeLayout(
			FOpenColorIOColorSpace::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([ConfigurationObjectHandle] { return FOpenColorIOColorSpaceCustomization::MakeInstance(ConfigurationObjectHandle); }));

	DetailBuilder.RegisterInstancedCustomPropertyTypeLayout(
		FOpenColorIODisplayView::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([ConfigurationObjectHandle] { return FOpenColorIODisplayViewCustomization::MakeInstance(ConfigurationObjectHandle); }));
}

#undef LOCTEXT_NAMESPACE
