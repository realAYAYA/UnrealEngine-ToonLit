// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGSettings.h"

#include "PCGSettings.h"

#include "Misc/AssetCategoryPath.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGSettings"

FText UAssetDefinition_PCGSettings::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Settings");
}

FLinearColor UAssetDefinition_PCGSettings::GetAssetColor() const
{
	return FColor::Turquoise;
}

TSoftClassPtr<UObject> UAssetDefinition_PCGSettings::GetAssetClass() const
{
	return UPCGSettings::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_PCGSettings::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("PCGCategory", "PCG")) }; 
	return Categories;
}

#undef LOCTEXT_NAMESPACE
