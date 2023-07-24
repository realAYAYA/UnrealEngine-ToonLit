// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMaterialXPipelineSettingsCustomization.h"
#include "IDetailChildrenBuilder.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeMaterialDefinitions.h"
#include "Materials/MaterialFunction.h"
#include "PropertyCustomizationHelpers.h"

TSharedRef<IPropertyTypeCustomization> FInterchangeMaterialXPipelineSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FInterchangeMaterialXPipelineSettingsCustomization());
}

void FInterchangeMaterialXPipelineSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// nothing to do here
}


void FInterchangeMaterialXPipelineSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder & ChildBuilder, IPropertyTypeCustomizationUtils & CustomizationUtils)
{
	using OnShouldFilterAssetFunc = bool (FInterchangeMaterialXPipelineSettingsCustomization::*)(const FAssetData&);

	// add all the child properties (ApplicationDisplayName & ApplicationDescription)	
	if(PropertyHandle->IsValidHandle())
	{
		TSharedPtr<IPropertyHandle> SurfaceShaderProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMaterialXPipelineSettings, SurfaceShader));
		uint32 NumChildren = 0;
		SurfaceShaderProperty->GetNumChildren(NumChildren);

		for(uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> ChildPropertyHandle = SurfaceShaderProperty->GetChildHandle(i);
			TSharedPtr<IPropertyHandle> KeyPropertyHandle = ChildPropertyHandle->GetKeyHandle();

			OnShouldFilterAssetFunc OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurface;

			if(KeyPropertyHandle.IsValid())
			{
				FString Str;
				KeyPropertyHandle->GetValueAsDisplayString(Str);
				if(Str == TEXT("Standard Surface Transmission"))
				{
					OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurfaceTransmission;
				}
			}

			IDetailPropertyRow& SurfaceShaderTypeRow = ChildBuilder.AddProperty(ChildPropertyHandle.ToSharedRef());
			SurfaceShaderTypeRow.ShowPropertyButtons(false);

			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Row;
			SurfaceShaderTypeRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

			FDetailWidgetRow& DetailWidgetRow = SurfaceShaderTypeRow.CustomWidget();
			DetailWidgetRow
			.NameContent()
			.MinDesiredWidth(Row.NameWidget.MinWidth)
			.MaxDesiredWidth(Row.NameWidget.MaxWidth)
			[
				NameWidget.ToSharedRef()
			]
			.ValueContent()
			.MinDesiredWidth(Row.ValueContent().MaxWidth)
			.MaxDesiredWidth(Row.ValueContent().MaxWidth)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialFunction::StaticClass())
					.PropertyHandle(ChildPropertyHandle)
					.OnShouldFilterAsset(this, OnShouldFilterAsset)
				]
			];
		}
	}
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurface(const FAssetData & InAssetData)
{
	return FMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), FMaterialXPipelineSettings::StandardSurfaceInputs, FMaterialXPipelineSettings::StandardSurfaceOutputs);
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurfaceTransmission(const FAssetData& InAssetData)
{
	return FMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), FMaterialXPipelineSettings::TransmissionSurfaceInputs, FMaterialXPipelineSettings::TransmissionSurfaceOutputs);
}