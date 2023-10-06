// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMaterialXPipelineCustomizations.h"

#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialXPipeline.h"
#include "Materials/MaterialFunction.h"

#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"

TSharedRef<IDetailCustomization> FInterchangeMaterialXPipelineCustomization::MakeInstance()
{
	return MakeShareable(new FInterchangeMaterialXPipelineCustomization());
}

TSharedRef<IDetailCustomization> FInterchangeMaterialXPipelineSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FInterchangeMaterialXPipelineSettingsCustomization());
}

void FInterchangeMaterialXPipelineCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);
	check(EditingObjects.Num() == 1);

	TWeakObjectPtr<UInterchangeMaterialXPipeline> Pipeline = Cast<UInterchangeMaterialXPipeline>(EditingObjects[0].Get());

	if (!ensure(Pipeline.IsValid()))
	{
		return;
	}

	IDetailCategoryBuilder& MaterialXCategory = DetailBuilder.EditCategory("MaterialX");

	MaterialXCategory.SetDisplayName(NSLOCTEXT("InterchangeMaterialXPipelineCustomization", "CategoryDisplayName", "MaterialX Settings"));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->SetObject(Pipeline->MaterialXSettings);

	MaterialXCategory.AddCustomRow(NSLOCTEXT("InterchangeMaterialXPipelineMaterialSubstitution::Message::Row", "MaterialXPredefinedMaterialSubstitutionMessageRow", "MaterialX Pipeline Material Substitution Message"))
	.NameContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("InterchangeMaterialXPipelineCustomization::Message", "MaterialXPredefinedMaterialSubstitutionMessage", "See 'Project Settings > Engine > Interchange MaterialX' to edit settings."))
		.AutoWrapText(true)
	];
}

void FInterchangeMaterialXPipelineSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	using OnShouldFilterAssetFunc = bool (FInterchangeMaterialXPipelineSettingsCustomization::*)(const FAssetData&);

	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);
	check(EditingObjects.Num() == 1);

	MaterialXSettings = Cast<UMaterialXPipelineSettings>(EditingObjects[0].Get());

	if (!ensure(MaterialXSettings.IsValid()))
	{
		return;
	}

	TSharedRef< IPropertyHandle > PairingsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMaterialXPipelineSettings, PredefinedSurfaceShaders));
	if (!PairingsHandle->IsValidHandle())
	{
		return;
	}

	DetailBuilder.HideProperty(PairingsHandle);

	IDetailCategoryBuilder& MaterialXPredefinedCategory = DetailBuilder.EditCategory("MaterialXPredefined");

	FText DisplayName{ NSLOCTEXT("InterchangeMaterialXPipelineSettingsCustomization", "CategoryDisplayName", "MaterialX Predefined Surface Shaders") };
	MaterialXPredefinedCategory.SetDisplayName(DisplayName);

	uint32 NumChildren = 0;
	PairingsHandle->GetNumChildren(NumChildren);

	for(uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> ChildPropertyHandle = PairingsHandle->GetChildHandle(i);
		TSharedPtr<IPropertyHandle> KeyPropertyHandle = ChildPropertyHandle->GetKeyHandle();

		UEnum* Enum = CastField<FEnumProperty>(KeyPropertyHandle->GetProperty())->GetEnum();

		FText EnumString;
		KeyPropertyHandle->GetValueAsDisplayText(EnumString);

		OnShouldFilterAssetFunc OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAsset;

		uint8 EnumValue = 0;
		KeyPropertyHandle->GetValue(EnumValue);
		if(EnumString.ToString() == TEXT("Standard Surface"))
		{
			OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurface;
		}
		else if(EnumString.ToString() == TEXT("Standard Surface Transmission"))
		{
			OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurfaceTransmission;
		}
		else if(EnumString.ToString() == TEXT("Surface Unlit"))
		{
			OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetSurfaceUnlit;
		}
		else if(EnumString.ToString() == TEXT("Usd Preview Surface"))
		{
			OnShouldFilterAsset = &FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetUsdPreviewSurface;
		}

		FDetailWidgetRow& DetailWidgetRow = MaterialXPredefinedCategory.AddCustomRow(DisplayName);
		DetailWidgetRow
		.NameContent()
		[
			SNew(SEditableText)
			.Text(EnumString)
			.IsReadOnly(true)
			.ToolTipText(Enum->GetToolTipTextByIndex(EnumValue))
		]
		.ValueContent()
		.MinDesiredWidth(1.0f)
		.MaxDesiredWidth(1.0f)
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

	DetailBuilder.HideCategory(GET_MEMBER_NAME_CHECKED(UMaterialXPipelineSettings, PredefinedSurfaceShaders));
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurface(const FAssetData & InAssetData)
{
	return UMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), UMaterialXPipelineSettings::StandardSurfaceInputs, UMaterialXPipelineSettings::StandardSurfaceOutputs);
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetStandardSurfaceTransmission(const FAssetData& InAssetData)
{
	return UMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), UMaterialXPipelineSettings::TransmissionSurfaceInputs, UMaterialXPipelineSettings::TransmissionSurfaceOutputs);
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetSurfaceUnlit(const FAssetData& InAssetData)
{
	return UMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), UMaterialXPipelineSettings::SurfaceUnlitInputs, UMaterialXPipelineSettings::SurfaceUnlitOutputs);
}

bool FInterchangeMaterialXPipelineSettingsCustomization::OnShouldFilterAssetUsdPreviewSurface(const FAssetData& InAssetData)
{
	return UMaterialXPipelineSettings::ShouldFilterAssets(Cast<UMaterialFunction>(InAssetData.GetAsset()), UMaterialXPipelineSettings::UsdPreviewSurfaceInputs, UMaterialXPipelineSettings::UsdPreviewSurfaceOutputs);
}
