// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

/**
  * Asset type actions for UStateTree.
 */

class FAssetTypeActions_StateTree : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_StateTree(const uint32 InAssetCategory);

protected:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_StateTree", "StateTree"); }
	virtual FColor GetTypeColor() const override { return FColor(201, 185, 29); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override;

private:
	uint32 AssetCategory = EAssetTypeCategories::Gameplay;
};
