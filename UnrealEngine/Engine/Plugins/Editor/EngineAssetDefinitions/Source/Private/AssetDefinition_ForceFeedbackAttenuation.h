// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/ForceFeedbackAttenuation.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ForceFeedbackAttenuation.generated.h"

UCLASS()
class UAssetDefinition_ForceFeedbackAttenuation : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ForceFeedbackAttenuation", "Force Feedback Attenuation"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(175, 0, 0)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UForceFeedbackAttenuation::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
   	{
   		static const auto Categories = { EAssetCategoryPaths::Input };
   		return Categories;
   	}
	// UAssetDefinition End
};
