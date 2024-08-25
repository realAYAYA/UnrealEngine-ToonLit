// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaDynamicMaterialWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DetailLayoutBuilder.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaDynamicMaterialWidget"

void SAvaDynamicMaterialWidget::Construct(const FArguments& InArgs, TSharedRef<IPropertyHandle> InPropertyHandle)
{
	PropertyHandle = InPropertyHandle;

	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	UObject* Value = nullptr;
	InPropertyHandle->GetValue(Value);

	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// @formatter:off
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		[
			SNew(SObjectPropertyEntryBox)
			.Visibility(this, &SAvaDynamicMaterialWidget::GetPickerVisibility)
			.AllowClear(true)
			.AllowedClass(UMaterialInterface::StaticClass())
			.DisplayBrowse(true)
			.DisplayThumbnail(true)
			.DisplayCompactSize(false)
			.DisplayUseSelected(true)
			.EnableContentPicker(true)
			.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
			.ObjectPath(this, &SAvaDynamicMaterialWidget::GetAssetPath)
			.OnObjectChanged(this, &SAvaDynamicMaterialWidget::OnAssetChanged)
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		.AutoHeight()
		[
			SNew(SButton)
			.Visibility(this, &SAvaDynamicMaterialWidget::GetButtonVisibility)
			.OnClicked(this, &SAvaDynamicMaterialWidget::OnButtonClicked)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OpenMaterialDesigner", "Edit with Material Designer"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
	];
	// @formatter:on
}

UObject* SAvaDynamicMaterialWidget::GetAsset() const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return nullptr;
	}

	UObject* Value = nullptr;
	PropertyHandle->GetValue(Value);

	return Value;
}

UDynamicMaterialInstance* SAvaDynamicMaterialWidget::GetDynamicMaterialInstance() const
{
	return Cast<UDynamicMaterialInstance>(GetAsset());
}

void SAvaDynamicMaterialWidget::SetAsset(UObject* NewAsset)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	PropertyHandle->SetValueFromFormattedString(NewAsset ? NewAsset->GetPathName() : "");
}

void SAvaDynamicMaterialWidget::SetDynamicMaterialInstance(UDynamicMaterialInstance* NewInstance)
{
	SetAsset(NewInstance);
}

EVisibility SAvaDynamicMaterialWidget::GetPickerVisibility() const
{
	if (GetDynamicMaterialInstance())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SAvaDynamicMaterialWidget::GetButtonVisibility() const
{
	if (GetDynamicMaterialInstance())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SAvaDynamicMaterialWidget::OnButtonClicked()
{
	if (GetDynamicMaterialInstance())
	{
		return OpenDynamicMaterialInstanceTab();
	}

	return CreateDynamicMaterialInstance();
}

FReply SAvaDynamicMaterialWidget::CreateDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We already have an instance, so we don't need to create one
	if (Instance)
	{
		return FReply::Unhandled();
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return FReply::Unhandled();
	}

	UDynamicMaterialInstanceFactory* DynamicMaterialInstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(DynamicMaterialInstanceFactory);

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(DynamicMaterialInstanceFactory->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		OuterObjects[0],
		"DynamicMaterialInstance",
		RF_NoFlags,
		nullptr,
		GWarn
	));

	PropertyHandle->SetValueFromFormattedString(NewInstance->GetPathName());

	return OpenDynamicMaterialInstanceTab();
}

FReply SAvaDynamicMaterialWidget::ClearDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have an instance, so we don't need to clear it (and don't clear non-MDIs)
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	SetDynamicMaterialInstance(nullptr);

	return FReply::Handled();
}

FReply SAvaDynamicMaterialWidget::OpenDynamicMaterialInstanceTab()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have a MDI, so don't try to open it.
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({Instance});

	return FReply::Handled();
}

FString SAvaDynamicMaterialWidget::GetAssetPath() const
{
	UObject* Asset = GetAsset();

	return Asset ? Asset->GetPathName() : "";
}

void SAvaDynamicMaterialWidget::OnAssetChanged(const FAssetData& AssetData)
{
	SetAsset(AssetData.GetAsset());
}

#undef LOCTEXT_NAMESPACE
