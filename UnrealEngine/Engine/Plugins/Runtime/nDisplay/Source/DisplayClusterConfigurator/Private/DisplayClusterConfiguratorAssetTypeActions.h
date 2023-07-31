// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

class FDisplayClusterConfiguratorAssetTypeActions_Base : public FAssetTypeActions_Blueprint
{
public:
	FDisplayClusterConfiguratorAssetTypeActions_Base(uint32 Categories) : MyAssetCategory(Categories) {}
	virtual uint32 GetCategories() override { return MyAssetCategory; }

protected:
	uint32 MyAssetCategory;
};

class FDisplayClusterConfiguratorAssetTypeActions
	: public FDisplayClusterConfiguratorAssetTypeActions_Base
{
public:
	FDisplayClusterConfiguratorAssetTypeActions(uint32 Category) : FDisplayClusterConfiguratorAssetTypeActions_Base(Category) {}
	
	//~ Begin IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DisplayClusterConfiguratorConfig", "nDisplay Config"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 188, 212); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	//~ End IAssetTypeActions Implementation

};

/** Wrapper just to hide base actor class from being created in the asset browser. */
class FDisplayClusterConfiguratorActorAssetTypeActions : public FDisplayClusterConfiguratorAssetTypeActions_Base
{
public:
	FDisplayClusterConfiguratorActorAssetTypeActions(EAssetTypeCategories::Type InAssetCategory) : FDisplayClusterConfiguratorAssetTypeActions_Base(InAssetCategory) {}

	// FAssetTypeActions_Base
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DisplayClusterConfiguratorActorConfig", "nDisplay Config"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 188, 212); }
	virtual UClass* GetSupportedClass() const override;
	// ~FAssetTypeActions_Base
};