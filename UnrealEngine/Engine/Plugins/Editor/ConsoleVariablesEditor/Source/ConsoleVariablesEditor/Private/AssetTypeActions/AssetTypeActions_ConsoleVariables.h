// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_ConsoleVariables : public FAssetTypeActions_Base
{

public:
	// ~ Begin IAssetTypeActions Interface
	
	/**
	 * @return Whether or not this asset has a live filter
	 */
	virtual bool CanFilter() override
	{
		return true;
	}
	
	/**
	 * @return Which category the asset will appear in when creating a new asset in the content browser
	 */
	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Misc;
	}
	
	/**
	 * @return The color on the asset image container
	 */
	virtual FColor GetTypeColor() const override
	{
		return FColor(238, 181, 235, 255);
	}

	// These are defined in source - GetName requires a defined LOCTEXT namespace and GetSupportedClass has an external include
	
	/**
	 * @return The display name for the asset type
	 */
	virtual FText GetName() const override;

	/**
	 * @return The asset class these actions will belong to
	 */
	virtual UClass* GetSupportedClass() const override;
	
	/**
	 * @brief Called when an item in the content browser is right-clicked. This determines what actions will be shown if HasActions() returns true.
	 * @param InObjects A list of objects selected in the content browser
	 * @param MenuBuilder A reference to an instance of the class which adds menu items to context menus
	 */
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	/**
	 * @return Whether the toolkit use dto open the asset should be world-centric (and thus should not use a standalone editor window)
	 */
	virtual bool ShouldForceWorldCentric() override
	{
		return true;
	}

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;

	// ~ End IAssetTypeActions Interface
};
