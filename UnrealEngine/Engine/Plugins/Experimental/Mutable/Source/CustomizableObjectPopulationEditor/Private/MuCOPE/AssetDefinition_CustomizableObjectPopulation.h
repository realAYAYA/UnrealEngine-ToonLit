// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "MuCOP/CustomizableObjectPopulation.h"

#include "AssetDefinition_CustomizableObjectPopulation.generated.h"


UCLASS()
class UAssetDefinition_CustomizableObjectPopulation final : public UAssetDefinitionDefault
{
	GENERATED_BODY()
	
public:
	
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CustomizableObjectPopulation", "Customizable Population");};

	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(0, 0, 0));};

	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UCustomizableObjectPopulation::StaticClass();}

	
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
