// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_ClassTypeBase.h"

class UBlueprintGeneratedClass;
struct FAssetData;
class IClassTypeActions;
class UFactory;

class ASSETTOOLS_API FAssetTypeActions_BlueprintGeneratedClass : public FAssetTypeActions_ClassTypeBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_BlueprintGeneratedClass", "Compiled Blueprint Class"); }
	virtual FColor GetTypeColor() const override { return FColor(133, 173, 255); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;

	// We don't want BPGCs to show up in CB/Ref viewer filters since the BP filters already include them
	virtual uint32 GetCategories() override { return EAssetTypeCategories::None; }
	// @todo
	//virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	// End IAssetTypeActions Implementation

	// FAssetTypeActions_ClassTypeBase Implementation
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActions(const FAssetData& AssetData) const override;
	// End FAssetTypeActions_ClassTypeBase Implementation

protected:
	/** Returns the factory responsible for creating a new Blueprint derived from the specified BPGC */
	virtual UFactory* GetFactoryForNewDerivedBlueprint(UBlueprintGeneratedClass* InBPGC) const;

	/** Returns the class of new Blueprints derived from this BPGC asset type */
	virtual UClass* GetNewDerivedBlueprintClass() const;

	/** Returns the tooltip to display when attempting to derive a Blueprint */
	FText GetNewDerivedBlueprintTooltip(TWeakObjectPtr<UBlueprintGeneratedClass> InObject);

	/** Returns TRUE if you can derive a Blueprint */
	bool CanExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprintGeneratedClass> InObject);

private:
	/** Handler for when NewDerivedBlueprint is selected */
	void ExecuteNewDerivedBlueprint(TWeakObjectPtr<UBlueprintGeneratedClass> InObject);
};
