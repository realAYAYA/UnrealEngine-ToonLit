// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_MediaProfile : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FColor GetTypeColor() const override { return FColor(140, 62, 35); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Media; }
	virtual bool IsImportedAsset() const override { return false; }
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};