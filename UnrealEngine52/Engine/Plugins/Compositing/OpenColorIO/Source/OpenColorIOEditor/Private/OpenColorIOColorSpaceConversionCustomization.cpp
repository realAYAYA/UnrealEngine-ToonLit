// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceConversionCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "Engine/EngineTypes.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIOConfiguration.h"
#include "OpenColorIOSettings.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/SOpenColorIOColorSpacePicker.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceConversionCustomization"

void FOpenColorIOColorConversionSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (InPropertyHandle->GetNumPerObjectValues() == 1 && InPropertyHandle->IsValidHandle())
	{
		void* StructData = nullptr;
		if (InPropertyHandle->GetValueData(StructData) == FPropertyAccess::Success)
		{
			ColorSpaceConversion = reinterpret_cast<FOpenColorIOColorConversionSettings*>(StructData);
			check(ColorSpaceConversion);

			TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

			HeaderRow.NameContent()
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
								ColorSpaceConversion->ValidateColorSpaces();

								if (ColorSpaceConversion->IsValid())
								{
									return FText::FromString(*ColorSpaceConversion->ToString());
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
	if (ColorSpaceConversion == nullptr)
	{
		return;
	}

	uint32 NumberOfChild;
	if (InStructPropertyHandle->GetNumChildren(NumberOfChild) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumberOfChild; ++Index)
		{
			TSharedRef<IPropertyHandle> ChildHandle = InStructPropertyHandle->GetChildHandle(Index).ToSharedRef();

			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, ConfigurationSource))
			{
				StructBuilder.AddProperty(ChildHandle).IsEnabled(true).ShowPropertyButtons(false);

				ChildHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]
					{
						TransformPicker[OCIO_Src]->SetConfiguration(ColorSpaceConversion->ConfigurationSource);
						TransformPicker[OCIO_Dst]->SetConfiguration(ColorSpaceConversion->ConfigurationSource);

						ColorSpaceConversion->OnConversionSettingsChanged().Broadcast();
					}));
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, SourceColorSpace))
			{
				SourceColorSpaceProperty = ChildHandle;
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DestinationColorSpace))
			{
				DestinationColorSpaceProperty = ChildHandle;
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DestinationDisplayView))
			{
				DestinationDisplayViewProperty = ChildHandle;
			}
			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FOpenColorIOColorConversionSettings, DisplayViewDirection))
			{
				DisplayViewDirectionProperty = ChildHandle;
			}
		}

		// Note: ChildHandle->SetOnPropertyValueChanged isn't automatically called on parent changes, so we explicitely reset the configuration here.
		InStructPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateSP(this, &FOpenColorIOColorConversionSettingsCustomization::OnConfigurationReset));
		if (TSharedPtr<IPropertyHandle> ParentHandle = InStructPropertyHandle->GetParentHandle())
		{
			ParentHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateSP(this, &FOpenColorIOColorConversionSettingsCustomization::OnConfigurationReset));
		}

		ApplyConfigurationToSelection();

		// Source color space picker widget
		TransformPicker[OCIO_Src] =
			SNew(SOpenColorIOColorSpacePicker)
			.Config(ColorSpaceConversion->ConfigurationSource)
			.InitialColorSpace(TransformSelection[OCIO_Src].ColorSpace)
			.RestrictedColor(TransformSelection[OCIO_Dst].ColorSpace)
			.InitialDisplayView(TransformSelection[OCIO_Src].DisplayView)
			.RestrictedDisplayView(TransformSelection[OCIO_Dst].DisplayView)
			.IsDestination(false)
			.OnColorSpaceChanged(FOnColorSpaceChanged::CreateSP(this, &FOpenColorIOColorConversionSettingsCustomization::OnSourceColorSpaceChanged));

		// Destination color space picker widget
		TransformPicker[OCIO_Dst] =
			SNew(SOpenColorIOColorSpacePicker)
			.Config(ColorSpaceConversion->ConfigurationSource)
			.InitialColorSpace(TransformSelection[OCIO_Dst].ColorSpace)
			.RestrictedColor(TransformSelection[OCIO_Src].ColorSpace)
			.InitialDisplayView(TransformSelection[OCIO_Dst].DisplayView)
			.RestrictedDisplayView(TransformSelection[OCIO_Src].DisplayView)
			.IsDestination(true)
			.OnColorSpaceChanged(FOnColorSpaceChanged::CreateSP(this, &FOpenColorIOColorConversionSettingsCustomization::OnDestinationColorSpaceChanged));

		// Source color space picker widget
		StructBuilder.AddCustomRow(LOCTEXT("TransformSource", "Transform Source"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TransformSource", "Transform Source"))
			.ToolTipText(LOCTEXT("TransformSource_Tooltip", "The source color space used for the transform."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			TransformPicker[OCIO_Src].ToSharedRef()
		];

		StructBuilder.AddCustomRow(LOCTEXT("TransformDestination", "Transform Destination"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TransformDestination", "Transform Destination"))
			.ToolTipText(LOCTEXT("TransformDestination_Tooltip", "The destination color space used for the transform."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			TransformPicker[OCIO_Dst].ToSharedRef()
		];
	}

	InStructPropertyHandle->MarkHiddenByCustomization();
}

void FOpenColorIOColorConversionSettingsCustomization::OnSourceColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView& NewDisplayView)
{
	TransformSelection[OCIO_Src].ColorSpace = NewColorSpace;
	TransformSelection[OCIO_Src].DisplayView = NewDisplayView;

	TransformPicker[OCIO_Dst]->SetRestrictions(NewColorSpace, NewDisplayView);

	ApplySelectionToConfiguration();

	ColorSpaceConversion->OnConversionSettingsChanged().Broadcast();
}

void FOpenColorIOColorConversionSettingsCustomization::OnDestinationColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView& NewDisplayView)
{
	TransformSelection[OCIO_Dst].ColorSpace = NewColorSpace;
	TransformSelection[OCIO_Dst].DisplayView = NewDisplayView;

	TransformPicker[OCIO_Src]->SetRestrictions(NewColorSpace, NewDisplayView);

	ApplySelectionToConfiguration();

	ColorSpaceConversion->OnConversionSettingsChanged().Broadcast();
}

void FOpenColorIOColorConversionSettingsCustomization::ApplyConfigurationToSelection()
{
	if (ColorSpaceConversion->DisplayViewDirection == EOpenColorIOViewTransformDirection::Forward)
	{
		TransformSelection[OCIO_Src].ColorSpace = ColorSpaceConversion->SourceColorSpace;
		TransformSelection[OCIO_Src].DisplayView.Reset();
		TransformSelection[OCIO_Dst].ColorSpace = ColorSpaceConversion->DestinationColorSpace;
		TransformSelection[OCIO_Dst].DisplayView = ColorSpaceConversion->DestinationDisplayView;
	}
	else
	{
		TransformSelection[OCIO_Src].ColorSpace.Reset();
		TransformSelection[OCIO_Src].DisplayView = ColorSpaceConversion->DestinationDisplayView;
		TransformSelection[OCIO_Dst].ColorSpace = ColorSpaceConversion->SourceColorSpace;
		TransformSelection[OCIO_Dst].DisplayView.Reset();
	}
}

void FOpenColorIOColorConversionSettingsCustomization::ApplySelectionToConfiguration()
{
	TArray<TSharedPtr<IPropertyHandle>> Properties = { SourceColorSpaceProperty, DestinationColorSpaceProperty, DestinationDisplayViewProperty, DisplayViewDirectionProperty };

	const FScopedTransaction Transaction(LOCTEXT("OCIOConfigurationSelectionUpdate", "OCIO Configuration Selection Update"));

	// Notify outer object(s). Only called for one property since it is shared.
	TArray<UObject*> Objects;
	Properties[0]->GetOuterObjects(Objects);
	for (UObject* OuterObject : Objects)
	{
		OuterObject->Modify();
	}

	for (const TSharedPtr<IPropertyHandle>& Property : Properties)
	{
		check(Property.IsValid());
		Property->NotifyPreChange();
	}

	if (TransformSelection[OCIO_Src].DisplayView.IsValid())
	{
		ColorSpaceConversion->SourceColorSpace = TransformSelection[OCIO_Dst].ColorSpace;
		ColorSpaceConversion->DestinationColorSpace.Reset();
		ColorSpaceConversion->DestinationDisplayView = TransformSelection[OCIO_Src].DisplayView;
		ColorSpaceConversion->DisplayViewDirection = EOpenColorIOViewTransformDirection::Inverse;
	}
	else
	{
		ColorSpaceConversion->SourceColorSpace = TransformSelection[OCIO_Src].ColorSpace;
		ColorSpaceConversion->DestinationColorSpace = TransformSelection[OCIO_Dst].ColorSpace;
		ColorSpaceConversion->DestinationDisplayView = TransformSelection[OCIO_Dst].DisplayView;
		ColorSpaceConversion->DisplayViewDirection = EOpenColorIOViewTransformDirection::Forward;
	}

	for (const TSharedPtr<IPropertyHandle>& Property : Properties)
	{
		Property->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
	for (const TSharedPtr<IPropertyHandle>& Property : Properties)
	{
		Property->NotifyFinishedChangingProperties();
	}
}

void FOpenColorIOColorConversionSettingsCustomization::OnConfigurationReset()
{
	TransformPicker[OCIO_Src]->SetConfiguration(nullptr);
	TransformPicker[OCIO_Dst]->SetConfiguration(nullptr);

	ColorSpaceConversion->OnConversionSettingsChanged().Broadcast();
}

#undef LOCTEXT_NAMESPACE
