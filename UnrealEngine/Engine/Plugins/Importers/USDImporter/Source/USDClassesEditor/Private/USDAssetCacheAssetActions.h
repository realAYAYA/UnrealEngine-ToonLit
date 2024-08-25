// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Toolkits/IToolkitHost.h"

class FUsdAssetCacheAssetActions : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>())
		override;
	//~ End IAssetTypeActions interface
};
