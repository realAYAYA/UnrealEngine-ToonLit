// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/AnimNextGraph.h"
#include "AssetDefinitionDefault.h"
#include "AnimNextGraphAssetDefinition.generated.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

UCLASS()
class UAssetDefinition_AnimNextGraph : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextGraph", "AnimNext Graph"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,128,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextGraph::StaticClass(); }
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("AnimNextSubMenu", "AnimNext")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE