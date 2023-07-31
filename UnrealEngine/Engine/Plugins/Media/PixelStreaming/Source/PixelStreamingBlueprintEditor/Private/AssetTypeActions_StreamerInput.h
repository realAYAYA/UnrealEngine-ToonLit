// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PixelStreamingStreamerInput.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_StreamerInput : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("PixelStreaming", "AssetTypeActions_StreamerInput", "Streamer Input Actions"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,64); }
	virtual UClass* GetSupportedClass() const override { return UPixelStreamingStreamerInput::StaticClass(); }
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return false; }
};
