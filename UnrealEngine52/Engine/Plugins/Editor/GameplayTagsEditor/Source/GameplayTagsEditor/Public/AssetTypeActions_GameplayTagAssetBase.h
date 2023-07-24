// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"


/** Base asset type actions for any classes with gameplay tagging */
class GAMEPLAYTAGSEDITOR_API FAssetTypeActions_GameplayTagAssetBase : public FAssetTypeActions_Base
{
public:

	/** Constructor */
	FAssetTypeActions_GameplayTagAssetBase(FName InTagPropertyName);

	/** Overridden to offer the gameplay tagging options */
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;

	/** Overridden to specify misc category */
	virtual uint32 GetCategories() override;

private:
	/**
	 * Open the gameplay tag editor
	 * 
	 * @param TagAssets	Assets to open the editor with
	 */
	void OpenGameplayTagEditor(TArray<class UObject*> Objects, TArray<struct FGameplayTagContainer*> Containers);

	/** Name of the property of the owned gameplay tag container */
	FName OwnedGameplayTagPropertyName;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
