// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

/**
 * Asset type actions for USmartObjectDefinition.
 */
class FAssetTypeActions_SmartObject : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_SmartObject(EAssetTypeCategories::Type InAssetCategory);

protected:
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override;

private:
	EAssetTypeCategories::Type AssetCategory = EAssetTypeCategories::Gameplay;
};
