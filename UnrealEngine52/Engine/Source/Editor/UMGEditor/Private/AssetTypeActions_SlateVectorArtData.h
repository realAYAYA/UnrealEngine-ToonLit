// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Slate/SlateVectorArtData.h"

class FAssetTypeActions_SlateVectorArtData : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SlateVectorArtData", "Slate Vector Art Data"); }
	virtual FColor GetTypeColor() const override { return FColor(105, 165, 60); }
	virtual UClass* GetSupportedClass() const override { return USlateVectorArtData::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::UI; }
}; 