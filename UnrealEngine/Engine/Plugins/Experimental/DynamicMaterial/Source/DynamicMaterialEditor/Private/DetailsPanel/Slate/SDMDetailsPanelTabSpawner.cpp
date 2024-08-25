// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMDetailsPanelTabSpawner.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DetailLayoutBuilder.h"
#include "IAssetTools.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelFactory.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMDetailsPanelTabSpawner"

void SDMDetailsPanelTabSpawner::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle)
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

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

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
			.AllowClear(true)
			.AllowedClass(UDynamicMaterialModel::StaticClass())
			.DisplayBrowse(true)
			.DisplayThumbnail(false)
			.DisplayCompactSize(true)
			.DisplayUseSelected(true)
			.EnableContentPicker(true)
			.ObjectPath(this, &SDMDetailsPanelTabSpawner::GetEditorPath)
			.OnObjectChanged(this, &SDMDetailsPanelTabSpawner::OnEditorChanged)
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		.AutoHeight()
		[
			SNew(SButton)
			.OnClicked(this, &SDMDetailsPanelTabSpawner::OnButtonClicked)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SDMDetailsPanelTabSpawner::GetButtonText)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
	];
	// @formatter:on
}

UDynamicMaterialModel* SDMDetailsPanelTabSpawner::GetMaterialModel() const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return nullptr;
	}

	UObject* Value = nullptr;
	PropertyHandle->GetValue(Value);

	return Cast<UDynamicMaterialModel>(Value);
}

void SDMDetailsPanelTabSpawner::SetMaterialModel(UDynamicMaterialModel* InNewModel)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	PropertyHandle->SetValueFromFormattedString(InNewModel ? InNewModel->GetPathName() : "");
}

FText SDMDetailsPanelTabSpawner::GetButtonText() const
{
	if (GetMaterialModel())
	{
		return LOCTEXT("OpenMaterialDesignerModel", "Edit with Material Designer");
	}

	return LOCTEXT("CreateMaterialDesignerModel", "Create with Material Designer");
}

FReply SDMDetailsPanelTabSpawner::OnButtonClicked()
{
	if (GetMaterialModel())
	{
		return OpenDynamicMaterialModelTab();
	}

	return CreateDynamicMaterialModel();
}

FReply SDMDetailsPanelTabSpawner::CreateDynamicMaterialModel()
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	// We already have a builder, so we don't need to create one
	if (MaterialModel)
	{
		return FReply::Unhandled();
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return FReply::Unhandled();
	}

	FString PackageName, AssetName;
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.CreateUniqueAssetName(
		UDynamicMaterialModelFactory::BaseDirectory / UDynamicMaterialModelFactory::BaseName + FGuid::NewGuid().ToString(),
		"",
		PackageName,
		AssetName
	);

	UPackage* Package = CreatePackage(*PackageName);
	check(Package);

	UDynamicMaterialModelFactory* DynamicMaterialModelFactory = NewObject<UDynamicMaterialModelFactory>();
	check(DynamicMaterialModelFactory);

	UDynamicMaterialModel* NewModel = Cast<UDynamicMaterialModel>(DynamicMaterialModelFactory->FactoryCreateNew(
		UDynamicMaterialModel::StaticClass(),
		Package,
		*AssetName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	));

	FAssetRegistryModule::AssetCreated(NewModel);
	Package->FullyLoad();

	PropertyHandle->SetValueFromFormattedString(NewModel->GetPathName());

	return OpenDynamicMaterialModelTab();
}

FReply SDMDetailsPanelTabSpawner::ClearDynamicMaterialModel()
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	// We don't have a builder, so we don't need to clear it
	if (!MaterialModel)
	{
		return FReply::Unhandled();
	}

	MaterialModel->ResetData();
	SetMaterialModel(nullptr);

	return FReply::Handled();
}

FReply SDMDetailsPanelTabSpawner::OpenDynamicMaterialModelTab()
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	// We don't have a builder, so we can't open it
	if (!MaterialModel)
	{
		return FReply::Unhandled();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({MaterialModel});

	return FReply::Handled();
}

FString SDMDetailsPanelTabSpawner::GetEditorPath() const
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	return MaterialModel ? MaterialModel->GetPathName() : "";
}

void SDMDetailsPanelTabSpawner::OnEditorChanged(const FAssetData& InAssetData)
{
	SetMaterialModel(Cast<UDynamicMaterialModel>(InAssetData.GetAsset()));
}

#undef LOCTEXT_NAMESPACE
