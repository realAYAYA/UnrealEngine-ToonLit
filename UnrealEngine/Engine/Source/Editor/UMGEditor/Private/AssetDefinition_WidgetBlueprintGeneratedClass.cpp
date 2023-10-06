// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_WidgetBlueprintGeneratedClass.h"

UAssetDefinition_WidgetBlueprintGeneratedClass::UAssetDefinition_WidgetBlueprintGeneratedClass() = default;

UAssetDefinition_WidgetBlueprintGeneratedClass::~UAssetDefinition_WidgetBlueprintGeneratedClass() = default;

FText UAssetDefinition_WidgetBlueprintGeneratedClass::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_WidgetBlueprintGeneratedClass", "Compiled Widget Blueprint");
}

FLinearColor UAssetDefinition_WidgetBlueprintGeneratedClass::GetAssetColor() const
{
	return FColor(121,149,207);
}

TSoftClassPtr<> UAssetDefinition_WidgetBlueprintGeneratedClass::GetAssetClass() const
{
	return UWidgetBlueprintGeneratedClass::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_WidgetBlueprintGeneratedClass::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::UI };
	return Categories;
}
