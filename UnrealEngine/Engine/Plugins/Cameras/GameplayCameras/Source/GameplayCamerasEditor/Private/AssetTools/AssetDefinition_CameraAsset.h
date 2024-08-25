// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraAsset.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_CameraAsset.generated.h"

UCLASS()
class UAssetDefinition_CameraAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:

	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual FAssetOpenSupport GetAssetOpenSupport(const FAssetOpenSupportArgs& OpenSupportArgs) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

