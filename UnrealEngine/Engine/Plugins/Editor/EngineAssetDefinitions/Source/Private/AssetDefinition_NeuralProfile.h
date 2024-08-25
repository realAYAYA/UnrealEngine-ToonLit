// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/NeuralProfile.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_NeuralProfile.generated.h"

UCLASS()
class UAssetDefinition_NeuralProfile : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NeuralProfile", "Neural Profile"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128, 128, 255)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNeuralProfile::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
    {
    	static const auto Categories = { EAssetCategoryPaths::Material };
    	return Categories;
    }
	// UAssetDefinition End
};
