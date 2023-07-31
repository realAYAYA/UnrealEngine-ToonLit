// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class URuntimeVirtualTexture;

/** Asset actions setup for URuntimeVirtualTexture */
class FAssetTypeActions_RuntimeVirtualTexture : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_RuntimeVirtualTexture() {}

protected:
	//~ Begin FAssetTypeActions_Base Interface.
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
	virtual void GetActions(TArray<UObject*> const& InObjects, FMenuBuilder& MenuBuilder) override;
	//~ End FAssetTypeActions_Base Interface.

private:
	/** Handler for when FindMaterials is selected */
	void ExecuteFindMaterials(TWeakObjectPtr<URuntimeVirtualTexture> Object);
	/** Handler for when FixMaterialUsage is selected */
	void ExecuteFixMaterialUsage(TWeakObjectPtr<URuntimeVirtualTexture> Object);
};
