// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/ControlRigPose.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ControlRigPose.generated.h"

UCLASS()
class UAssetDefinition_ControlRigPose : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ControlRigPose", "Control Rig Pose"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(222, 128, 64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UControlRigPoseAsset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Animation };
		return Categories;
	}
	// UAssetDefinition End
};
