// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_DynamicMaterialModel.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/World.h"
#include "Model/DynamicMaterialModel.h"
#include "Toolkits/IToolkitHost.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_DynamicMaterialModel"

FText UAssetDefinition_DynamicMaterialModel::GetAssetDisplayName() const
{
	return LOCTEXT("DynamicMaterialModel", "Dynamic Material Model");
}

FText UAssetDefinition_DynamicMaterialModel::GetAssetDisplayName(const FAssetData& InAssetData) const
{
	return FText::FromName(InAssetData.AssetName);
}

TSoftClassPtr<> UAssetDefinition_DynamicMaterialModel::GetAssetClass() const
{
	return UDynamicMaterialModel::StaticClass();
}

FLinearColor UAssetDefinition_DynamicMaterialModel::GetAssetColor() const
{
	return FLinearColor(FColor(96, 192, 96));
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DynamicMaterialModel::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = {EAssetCategoryPaths::Material};
	return Categories;
}

EAssetCommandResult UAssetDefinition_DynamicMaterialModel::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	for (UObject* Object : InOpenArgs.LoadObjects<UDynamicMaterialModel>())
	{
		UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(Object);

		if (!MaterialModel)
		{
			continue;
		}

		UWorld* World = MaterialModel->GetWorld();

		if (!World && InOpenArgs.ToolkitHost.IsValid())
		{
			World = InOpenArgs.ToolkitHost->GetWorld();
		}

		FDynamicMaterialEditorModule::Get().SetDynamicMaterialModel(
			MaterialModel,
			World,
			/* Invoke Tab */ true
		);

		// Only process the first selected object.
		return EAssetCommandResult::Handled;
	}

	return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
