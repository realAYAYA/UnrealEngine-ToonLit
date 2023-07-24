// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "ImagePlateFileSequence.h"

class FAssetTypeActions_ImagePlateFileSequence : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ImagePlateFileSequence", "Image Plate File Sequence"); }
	virtual FColor GetTypeColor() const override { return FColor(105, 0, 60); }
	virtual UClass* GetSupportedClass() const override { return UImagePlateFileSequence::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Media; }
};
