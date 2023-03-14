// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class IToolkitHost;

class FAssetTypeActions_LensFile : public FAssetTypeActions_Base
{
public:
	//~ Begin IAssetTypeActions interface
	virtual FColor GetTypeColor() const override
	{
		return FColor(100, 255, 100);
	}
	
	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Misc;
	}

	virtual bool IsImportedAsset() const override
	{
		return true;
	}

	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	//~ End IAssetTypeActions interface
};