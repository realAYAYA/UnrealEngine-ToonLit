// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_MediaOutput : public FAssetTypeActions_Base
{
public:
	//~ Begin IAssetTypeActions Implementation
	virtual bool CanFilter() override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
	virtual bool IsImportedAsset() const override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	//~ End IAssetTypeActions Implementation
};