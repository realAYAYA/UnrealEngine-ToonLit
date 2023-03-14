// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceConversionCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "Engine/EngineTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIOConfiguration.h"
#include "PropertyHandle.h"
#include "SResetToDefaultMenu.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceConversionCustomization"

void FOpenColorIOColorConversionSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ColorConversionProperty = InPropertyHandle;

	if (ColorConversionProperty.IsValid())
	{
		FProperty* Property = ColorConversionProperty->GetProperty();
		check(Property && CastField<FStructProperty>(Property) && CastField<FStructProperty>(Property)->Struct && CastField<FStructProperty>(Property)->Struct->IsChildOf(FOpenColorIOColorConversionSettings::StaticStruct()));

		TArray<void*> RawData;
		ColorConversionProperty->AccessRawData(RawData);
		if (RawData.Num() > 0)
		{
			FOpenColorIOColorConversionSettings* ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);
			bIsDestinationDisplayView = ColorSpaceConversion->DestinationDisplayView.IsValid();
		}

		if (ColorConversionProperty->GetNumPerObjectValues() == 1 && ColorConversionProperty->IsValidHandle())
		{
			TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

			HeaderRow
				.NameContent()
				[
					InPropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(512)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(MakeAttributeLambda([=]
						{
							if (ColorConversionProperty.IsValid())
							{
								TArray<void*> RawData;
								ColorConversionProperty->AccessRawData(RawData);
								if (RawData.Num() > 0)
								{
									FOpenColorIOColorConversionSettings* Conversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);
									if (Conversion != nullptr)
									{
										Conversion->ValidateColorSpaces();
										return FText::FromString(*Conversion->ToString());
									}
								}
							}

							return FText::FromString(TEXT("<Invalid Conversion>"));

						}))
					]
				].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
		}
	}
}

void FOpenColorIOColorConversionSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{

	TAttribute<EVisibility> DestinationColorSpaceVisibility(this, &FOpenColorIOColorConversionSettingsCustomization::ShouldShowDestinationColorSpace);
	TAttribute<EVisibility> DestinationDisplayViewVisibility(this, &FOpenColorIOColorConversionSettingsCustomization::ShouldShowDestinationDisplayView);

	uint32 NumberOfChild;
	if (InStructPropertyHandle->GetNumChildren(NumberOfChild) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumberOfChild; ++Index)
		{
			TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(Index).ToSharedRef();
	
			//Create custom rows for source and destination color space of the conversion. Since the struct is hooked to an OCIOConfiguration
			//We use it to populate the available color spaces instead of using a raw configuration file.
			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, ConfigurationSource))
			{
				StructBuilder.AddProperty(ChildHandle).IsEnabled(true).ShowPropertyButtons(false);
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, SourceColorSpace))
			{
				SourceColorSpaceProperty = ChildHandle;

				FDetailWidgetRow& ColorSpaceWidget = StructBuilder.AddCustomRow(FText::FromName(ChildHandle->GetProperty()->GetFName()));
				AddPropertyRow(ColorSpaceWidget, ChildHandle, StructCustomizationUtils, false);

				AddDestinationModeRow(StructBuilder, StructCustomizationUtils);
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DestinationColorSpace))
			{
				DestinationColorSpaceProperty = ChildHandle;

				FDetailWidgetRow& ColorSpaceWidget = StructBuilder.AddCustomRow(FText::FromName(ChildHandle->GetProperty()->GetFName()));
				ColorSpaceWidget.Visibility(DestinationColorSpaceVisibility);
				AddPropertyRow(ColorSpaceWidget, ChildHandle, StructCustomizationUtils, false);
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DestinationDisplayView))
			{
				DestinationDisplayViewProperty = ChildHandle;

				FDetailWidgetRow& ColorSpaceWidget = StructBuilder.AddCustomRow(FText::FromName(ChildHandle->GetProperty()->GetFName()));
				ColorSpaceWidget.Visibility(DestinationDisplayViewVisibility);
				AddPropertyRow(ColorSpaceWidget, ChildHandle, StructCustomizationUtils, true);
			}
		}
	}
	InStructPropertyHandle->MarkHiddenByCustomization();
}

void FOpenColorIOColorConversionSettingsCustomization::AddDestinationModeRow(IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyUtilities> PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();
	StructBuilder.AddCustomRow(FText::FromString(TEXT("Destination Mode")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DestinationMode", "Destination Mode"))
		.ToolTipText(LOCTEXT("DestinationMode_Tooltip", "The destination mode to use, between color space and dislay-view."))
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
		.ValueContent()
		[
			SNew(SSegmentedControl<bool>)
			.Value_Lambda([this]()
				{
					return bIsDestinationDisplayView;
				})
		.OnValueChanged(this, &FOpenColorIOColorConversionSettingsCustomization::SetDestinationMode)
			+ SSegmentedControl<bool>::Slot(false)
			.Text(LOCTEXT("ColorSpace", "Color Space"))
			.ToolTip(LOCTEXT("ColorSpace_ToolTip",
				"Select this if you want to use a color space destination."))

			+ SSegmentedControl<bool>::Slot(true)
			.Text(LOCTEXT("DisplayView", "Display-View"))
			.ToolTip(LOCTEXT("DisplayView_ToolTip",
				"Select this if you want to use a display-view destination."))
		];
}

void FOpenColorIOColorConversionSettingsCustomization::AddPropertyRow(FDetailWidgetRow& InWidgetRow, TSharedRef<IPropertyHandle> InChildHandle, IPropertyTypeCustomizationUtils& InCustomizationUtils, bool bIsDisplayViewRow) const
{
	TSharedPtr<IPropertyUtilities> PropertyUtils = InCustomizationUtils.GetPropertyUtilities();
	TSharedPtr<SResetToDefaultMenu> ResetToDefaultMenu;

	InWidgetRow
		.NameContent()
		[
			InChildHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(MakeAttributeLambda([=]
				{
					TArray<void*> RawData;
					InChildHandle->AccessRawData(RawData);
					if (RawData.Num() > 0)
					{
						if (bIsDisplayViewRow)
						{
							FOpenColorIODisplayView* DisplayView = reinterpret_cast<FOpenColorIODisplayView*>(RawData[0]);
							if (DisplayView != nullptr)
							{
								return FText::FromString(*DisplayView->ToString());
							}
						}
						else
						{
							FOpenColorIOColorSpace* ColorSpace = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
							if (ColorSpace != nullptr)
							{
								return FText::FromString(*ColorSpace->ToString());
							}
						}
					}
				
					return FText::FromString(TEXT("<Invalid>"));
				}))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Lambda([=]() { return bIsDisplayViewRow ? HandleDisplayViewComboButtonMenuContent(InChildHandle) : HandleColorSpaceComboButtonMenuContent(InChildHandle); })
				.ContentPadding(FMargin(4.0, 2.0))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SAssignNew(ResetToDefaultMenu, SResetToDefaultMenu)
			]
		].IsEnabled(MakeAttributeLambda([=] { return !InChildHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
		
		ResetToDefaultMenu->AddProperty(InChildHandle);
}

TSharedRef<SWidget> FOpenColorIOColorConversionSettingsCustomization::HandleColorSpaceComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	FOpenColorIOColorConversionSettings* ColorSpaceConversion = nullptr;

	if (InPropertyHandle.IsValid())
	{
		TArray<void*> RawData;
		ColorConversionProperty->AccessRawData(RawData);
		if (RawData.Num() > 0)
		{
			ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);
		}
	}

	if (ColorSpaceConversion && ColorSpaceConversion->ConfigurationSource)
	{
		FOpenColorIOColorSpace RestrictedColorSpace;
		if (InPropertyHandle == SourceColorSpaceProperty)
		{
			RestrictedColorSpace = ColorSpaceConversion->DestinationColorSpace;
		}
		else
		{
			RestrictedColorSpace = ColorSpaceConversion->SourceColorSpace;
		}

		// generate menu
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection("AvailableColorSpaces", LOCTEXT("AvailableColorSpaces", "Available Color Spaces"));
		{
			bool ColorSpaceAdded = false;

			for (int32 i = 0; i < ColorSpaceConversion->ConfigurationSource->DesiredColorSpaces.Num(); ++i)
			{
				const FOpenColorIOColorSpace& ColorSpace = ColorSpaceConversion->ConfigurationSource->DesiredColorSpaces[i];
				if (ColorSpace == RestrictedColorSpace || !ColorSpace.IsValid())
				{
					continue;
				}

				MenuBuilder.AddMenuEntry
				(
					FText::FromString(ColorSpace.ToString()),
					FText::FromString(ColorSpace.ToString()),
					FSlateIcon(),
					FUIAction
					(
						FExecuteAction::CreateLambda([=] 
						{
							if (InPropertyHandle.IsValid())
							{
								if (FStructProperty* StructProperty = CastField<FStructProperty>(InPropertyHandle->GetProperty()))
								{
									TArray<void*> RawData;
									InPropertyHandle->AccessRawData(RawData);
									if (RawData.Num() > 0)
									{
										FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

										FString TextValue;
										StructProperty->Struct->ExportText(TextValue, &ColorSpace, PreviousColorSpaceValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
										ensure(InPropertyHandle->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
									}
								}
							}
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([=] 
						{
							if (SourceColorSpaceProperty.IsValid())
							{
								TArray<void*> RawData;
								SourceColorSpaceProperty->AccessRawData(RawData);
								if (RawData.Num() > 0)
								{
									FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
									return *PreviousColorSpaceValue == ColorSpace;
								}
							}
						
							return false;
						})
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);

				ColorSpaceAdded = true;
			}

			if (!ColorSpaceAdded)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No available color spaces"), false, false);
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FOpenColorIOColorConversionSettingsCustomization::HandleDisplayViewComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	FOpenColorIOColorConversionSettings* ColorSpaceConversion = nullptr;

	if (InPropertyHandle.IsValid())
	{
		TArray<void*> RawData;
		ColorConversionProperty->AccessRawData(RawData);
		if (RawData.Num() > 0)
		{
			ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);
		}
	}

	if (ColorSpaceConversion && ColorSpaceConversion->ConfigurationSource)
	{
		// generate menu
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.BeginSection("AvailableDisplayViews", LOCTEXT("AvailableDisplayViews", "Available Display-Views"));
		{
			bool DisplayViewAdded = false;

			for (int32 i = 0; i < ColorSpaceConversion->ConfigurationSource->DesiredDisplayViews.Num(); ++i)
			{
				const FOpenColorIODisplayView& DisplayView = ColorSpaceConversion->ConfigurationSource->DesiredDisplayViews[i];
				if (!DisplayView.IsValid())
				{
					continue;
				}

				MenuBuilder.AddMenuEntry
				(
					FText::FromString(DisplayView.ToString()),
					FText::FromString(DisplayView.ToString()),
					FSlateIcon(),
					FUIAction
					(
						FExecuteAction::CreateLambda([=]
							{
								if (InPropertyHandle.IsValid())
								{
									if (FStructProperty* StructProperty = CastField<FStructProperty>(InPropertyHandle->GetProperty()))
									{
										TArray<void*> RawData;
										InPropertyHandle->AccessRawData(RawData);
										if (RawData.Num() > 0)
										{
											FOpenColorIODisplayView* PreviousDisplayViewValue = reinterpret_cast<FOpenColorIODisplayView*>(RawData[0]);

											FString TextValue;
											StructProperty->Struct->ExportText(TextValue, &DisplayView, PreviousDisplayViewValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
											ensure(InPropertyHandle->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
										}
									}
								}
							}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([=]
							{
								return false;
							})
						),
						NAME_None,
						EUserInterfaceActionType::RadioButton
					);

				DisplayViewAdded = true;
			}

			if (!DisplayViewAdded)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoDisplayViewFound", "No available display-view"), false, false);
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

void FOpenColorIOColorConversionSettingsCustomization::SetDestinationMode(bool bInIsDestinationDisplayView)
{
	bIsDestinationDisplayView = bInIsDestinationDisplayView;

	if (ColorConversionProperty.IsValid())
	{
		TArray<void*> RawData;
		ColorConversionProperty->AccessRawData(RawData);
		if (RawData.Num() > 0)
		{
			FOpenColorIOColorConversionSettings* ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(RawData[0]);

			if (bIsDestinationDisplayView)
			{
				ColorSpaceConversion->DestinationColorSpace.Reset();
			}
			else
			{
				ColorSpaceConversion->DestinationDisplayView.Reset();
			}
		}
	}
}

EVisibility FOpenColorIOColorConversionSettingsCustomization::ShouldShowDestinationColorSpace() const
{
	return bIsDestinationDisplayView ? EVisibility::Hidden : EVisibility::Visible;
}

EVisibility FOpenColorIOColorConversionSettingsCustomization::ShouldShowDestinationDisplayView() const
{
	return bIsDestinationDisplayView ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
