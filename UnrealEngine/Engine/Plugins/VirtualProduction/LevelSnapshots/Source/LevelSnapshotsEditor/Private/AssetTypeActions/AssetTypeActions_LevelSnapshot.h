// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_LevelSnapshot : public FAssetTypeActions_Base
{

public:
	// ~ Begin IAssetTypeActions Interface
	
	/**
	 * @brief 
	 * @return Whether or not this asset has a live filter
	 */
	virtual bool CanFilter() override
	{
		return true;
	}
	
	/**
	 * @brief 
	 * @return Which category the asset will appear in when creating a new asset in the content browser
	 */
	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Misc;
	}
	
	/**
	 * @brief 
	 * @return The color on the asset image container
	 */
	virtual FColor GetTypeColor() const override
	{
		return FColor(238, 181, 235, 255);
	}

	// These are defined in source - GetName requires a defined LOCTEXT namespace and GetSupportedClass has an external include
	
	/**
	 * @brief
	 * @return The display name for the asset type
	 */
	virtual FText GetName() const override;

	/**
	 * @brief
	 * @return The asset class these actions will belong to
	 */
	virtual UClass* GetSupportedClass() const override;
	
	/**
	 * @brief Called when an item in the content browser is right-clicked. This determines what actions will be shown if HasActions() returns true.
	 * @param InObjects A list of objects selected in the content browser
	 * @param MenuBuilder A reference to an instance of the class which adds menu items to context menus
	 */
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;

	// ~ End IAssetTypeActions Interface

private:

	static void OpenAssetWithLevelSnapshotsEditor(const TArray<UObject*>& InObjects);
};
