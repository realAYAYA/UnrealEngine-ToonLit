// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"


class FAssetTypeActions_MaterialFunctionInstance : public FAssetTypeActions_MaterialFunction
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_MaterialFunctionInstance", "Material Function Instance"); }
	virtual UClass* GetSupportedClass() const override { return UMaterialFunctionInstance::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::None; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

private:
	/** Handler for when FindParent is selected */
	void ExecuteFindParent(TArray<TWeakObjectPtr<UMaterialFunctionInstance>> Objects);
};


class FAssetTypeActions_MaterialFunctionLayerInstance : public FAssetTypeActions_MaterialFunctionInstance
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MaterialFunctionMaterialLayerInstance", "Material Layer Instance"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 175); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool CanFilter() override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Materials; }
private:
	virtual void ExecuteNewMFI(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects) override;
	virtual FText GetInstanceText() const override { return NSLOCTEXT("AssetTypeActions", "Material_NewMLI", "Create Layer Instance"); }
};

class FAssetTypeActions_MaterialFunctionLayerBlendInstance : public FAssetTypeActions_MaterialFunctionInstance
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MaterialFunctionMaterialLayerBlendInstance", "Material Layer Blend Instance"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 175); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Materials; }
	virtual bool CanFilter() override;

private:
	virtual void ExecuteNewMFI(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects) override;
	virtual FText GetInstanceText() const override { return NSLOCTEXT("AssetTypeActions", "Material_NewMBI", "Create Blend Instance"); }

};
