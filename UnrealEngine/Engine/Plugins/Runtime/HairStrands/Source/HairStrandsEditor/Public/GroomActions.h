// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"

class ISlateStyle;
class UGroomAsset;

/**
 * Implements an action for groom assets.
 */
class FGroomActions : public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FGroomActions();

public:

	//~ FAssetTypeActions_Base overrides

	virtual bool CanFilter() override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

private:

	/** Callback for Rebuild groom action */
	bool CanRebuild(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;
	void ExecuteRebuild(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;

	/** Callback for creating binding asset action */
	bool CanCreateBindingAsset(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;
	void ExecuteCreateBindingAsset(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;

	/** Callback for creating follicle texture action */
	bool CanCreateFollicleTexture(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;
	void ExecuteCreateFollicleTexture(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;

	/** Callback for creating strands texture action */
	bool CanCreateStrandsTextures(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;
	void ExecuteCreateStrandsTextures(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const;
};
