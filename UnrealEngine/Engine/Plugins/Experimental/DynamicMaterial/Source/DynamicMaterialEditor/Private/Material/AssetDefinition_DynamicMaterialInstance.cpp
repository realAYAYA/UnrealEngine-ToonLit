// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_DynamicMaterialInstance.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_DynamicMaterialInstance"

FText UAssetDefinition_DynamicMaterialInstance::GetAssetDisplayName() const
{
	return LOCTEXT("DynamicMaterialInstance", "Dynamic Material Instance");
}

FText UAssetDefinition_DynamicMaterialInstance::GetAssetDisplayName(const FAssetData& InAssetData) const
{
	return FText::FromName(InAssetData.AssetName);
}

TSoftClassPtr<> UAssetDefinition_DynamicMaterialInstance::GetAssetClass() const
{
	return UDynamicMaterialInstance::StaticClass();
}

FLinearColor UAssetDefinition_DynamicMaterialInstance::GetAssetColor() const
{
	return FLinearColor(FColor(64, 192, 64));
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DynamicMaterialInstance::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = {EAssetCategoryPaths::Material};
	return Categories;
}

EAssetCommandResult UAssetDefinition_DynamicMaterialInstance::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	TArray<UObject*> MaterialModels;

	for (UObject* Object : InOpenArgs.LoadObjects<UDynamicMaterialInstance>())
	{
		UDynamicMaterialInstance* Instance = Cast<UDynamicMaterialInstance>(Object);

		if (!Instance)
		{
			continue;
		}

		UDynamicMaterialModel* MaterialModel = Instance->GetMaterialModel();

		if (!MaterialModel)
		{
			continue;
		}

		MaterialModels.Add(MaterialModel);
	}

	if (MaterialModels.IsEmpty())
	{
		return EAssetCommandResult::Unhandled;
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OpenEditorForAssets(MaterialModels);

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
