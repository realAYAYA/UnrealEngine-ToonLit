// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_PhysicalMaterialMask : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PhysicalMaterialMask", "Physical Material Mask"); }
	virtual FColor GetTypeColor() const override { return FColor(200,162,108); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Physics; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

	/** Returns the thumbnail info for the specified asset, if it has one. */
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

	/** Returns the default thumbnail type that should be rendered when rendering primitive shapes.  This does not need to be implemented if the asset does not render a primitive shape */
	virtual EThumbnailPrimType GetDefaultThumbnailPrimitiveType(UObject* Asset) const override;

private:
	/** Handler to check to see if imported asset actions are allowed */
	bool CanExecuteImportedAssetActions(const TArray<FString> ResolvedFilePaths) const;

	/** Handler for Import */
	void ExecuteImport(TWeakObjectPtr<UPhysicalMaterialMask> InSelectedMask);

	/** Handler for Reimport */
	void ExecuteReimport(TArray<TWeakObjectPtr<UPhysicalMaterialMask>> InSelectedMasks);

	/** Handler for ReimportWithNewFile */
	void ExecuteReimportWithNewFile(TWeakObjectPtr<UPhysicalMaterialMask> InSelectedMask);

	/** Handler for OpenSourceLocation */
	void ExecuteOpenSourceLocation(const TArray<FString> ResolvedFilePaths);

	/** Handler for OpenInExternalEditor */
	void ExecuteOpenInExternalEditor(const TArray<FString> ResolvedFilePaths);

	/** Handler for Debug */
	void ExecuteDebug(TWeakObjectPtr<UPhysicalMaterialMask> InSelectedMask);
};
