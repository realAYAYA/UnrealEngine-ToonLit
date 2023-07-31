// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

#include "OptimusDeformer.h"

class FOptimusDeformerAssetActions
	: public FAssetTypeActions_Base
{
public:
	FOptimusDeformerAssetActions(EAssetTypeCategories::Type InAssetCategoryBit = EAssetTypeCategories::Animation);
	
protected:
	// IAssetTypeActions overrides
	FText GetName() const override;
	FColor GetTypeColor() const override;
	UClass* GetSupportedClass() const override;
	void OpenAssetEditor(
		const TArray<UObject*>& InObjects, 
		TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()
	) override;
	uint32 GetCategories() override;
	const TArray<FText>& GetSubMenus() const override;
	TSharedPtr<SWidget> GetThumbnailOverlay(
		const FAssetData& AssetData
	) const override;

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};
