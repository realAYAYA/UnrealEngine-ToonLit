// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UDataprepAssetInterface;

/** Asset type actions for UDataprepAssetInterface class */
class DATAPREPEDITOR_API FAssetTypeActions_DataprepAssetInterface : public FAssetTypeActions_Base
{
public:
	// Begin IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual uint32 GetCategories() override;
	virtual FColor GetTypeColor() const override { return FColor(0, 0, 0); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool IsImportedAsset() const override { return false; }

private:
	void CreateInstance(TArray<TWeakObjectPtr<UDataprepAssetInterface>> Objects);
	void ExecuteDataprepAssets(TArray<TWeakObjectPtr<UDataprepAssetInterface>> Objects);
};

