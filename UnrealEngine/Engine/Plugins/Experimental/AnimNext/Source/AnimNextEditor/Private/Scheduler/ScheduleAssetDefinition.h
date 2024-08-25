// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Scheduler/AnimNextSchedule.h"
#include "ScheduleAssetDefinition.generated.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

UCLASS()
class UAssetDefinition_AnimNextSchedule : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextSchedule", "AnimNext Schedule"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128, 64, 64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextSchedule::StaticClass(); }
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("AnimNextSubMenu", "AnimNext")) };
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE