// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PixelStreamingStreamerVideoInput.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_StreamerVideoInput : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("PixelStreaming", "AssetTypeActions_StreamerVideoInput", "Streamer Video Input Actions"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,64); }
	virtual UClass* GetSupportedClass() const override { return UPixelStreamingStreamerVideoInput::StaticClass(); }
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return AssetTools.RegisterAdvancedAssetCategory("PixelStreaming", NSLOCTEXT("PixelStreaming", "AssetCategoryDisplayName", "PixelStreaming"));
		//return EAssetTypeCategories::Misc;
	}
	virtual bool IsImportedAsset() const override { return false; }
};
