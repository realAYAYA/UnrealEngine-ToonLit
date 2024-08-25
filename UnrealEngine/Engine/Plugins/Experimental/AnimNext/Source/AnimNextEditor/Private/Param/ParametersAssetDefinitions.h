// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Param/AnimNextParameterBlock.h"
#include "ParametersAssetDefinitions.generated.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"


UCLASS()
class UAssetDefinition_AnimNextParameterBlock : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextParameterBlock", "AnimNext Parameter Block"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,64,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextParameterBlock::StaticClass(); }
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("AnimNextSubMenu", "AnimNext")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE