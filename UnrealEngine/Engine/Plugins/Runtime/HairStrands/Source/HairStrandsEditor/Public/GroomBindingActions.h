// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"
#include "GroomBindingAsset.h"

class ISlateStyle;
struct FToolMenuContext;

/**
 * Implements an action for groom binding assets.
 */
class FGroomBindingActions : public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use for asset editor toolkits.
	 */
	FGroomBindingActions() { }

public:

	//~ FAssetTypeActions_Base overrides

	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

public:
	static void RegisterMenus();

protected:
	static void ExecuteRebuildBindingAsset(const FToolMenuContext& MenuContext);
};
