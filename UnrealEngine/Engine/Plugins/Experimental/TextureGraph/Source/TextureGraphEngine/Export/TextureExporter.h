// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "2D/TargetTextureSet.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Model/Mix/MixInterface.h"
THIRD_PARTY_INCLUDES_START
#include "continuable/continuable.hpp"
THIRD_PARTY_INCLUDES_END
#include <vector>

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"

DECLARE_LOG_CATEGORY_EXTERN(ExportLogs, Log, Verbose);

struct TEXTUREGRAPHENGINE_API FExportMapSettings
{
	bool bIsEnabled = true;
	FName Name;
	FString Path;
	bool UseOverridePath;
	bool OverwriteTextures;
	TiledBlobPtr Map;
	bool IsSRGB;
	bool UseMipMaps;
	int32 Width;
	int32 Height;
	TextureCompressionSettings Compression;
	TextureGroup LODGroup;

	DECLARE_DELEGATE_OneParam(OnExportMapDone, FExportMapSettings MapSettings);
	OnExportMapDone OnDone;
};

typedef std::vector<std::pair<FName, FExportMapSettings>> ExportMaps;

class TEXTUREGRAPHENGINE_API FExportSettings
{
public:
	FString PresetName;
	ExportMaps ExportPreset;

	DECLARE_DELEGATE(OnExportDone);
	OnExportDone OnDone;

	int MapsExported = 0;
	
	int GetMapCount()
	{
		int MapCount = 0;

		
		if(ExportPreset.size() > 0)
		{
			for (const auto& Map : ExportPreset)
			{
				if(Map.second.bIsEnabled)
				{
					MapCount++;
				}
			}
		}

		return MapCount;
	}

	void Reset()
	{
		MapsExported = 0;
		ExportPreset.clear();
	}

	void OnMapExportDone(FExportMapSettings MapSetting)
	{
		if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender())
			return;

		MapsExported++;
		if (GetMapCount() == MapsExported)
		{
			OnDone.ExecuteIfBound();
		}

		if(MapSetting.OnDone.IsBound())
			MapSetting.OnDone.Unbind();
	}
};

struct TEXTUREGRAPHENGINE_API TextureExporter
{
	static FExportMapSettings		GetExportSettingsForTarget(FExportSettings& ExportSettings, TiledBlobPtr BlobObj, FName Name);
	static AsyncBool				ExportAllMapsAsUAsset(UMixInterface* MixObj, const FString& OutputFolder, const FExportSettings& ExportSettings);
	static AsyncBool				ExportAsUAsset(UMixInterface* MixObj, const FExportSettings& ExportSettings, FString C);
	static void						ExportAsUAsset(UMixInterface* MixObj, FName AssetName, FString CurrentPath, const FExportSettings& Setting);
	static AsyncInt					ExportRawAsUAsset(RawBufferPtr RawObj,const FExportMapSettings& Setting, const FString& CompletePath, const FName& TextureName);
	static bool						IsPackageNameValid(FString Path, FString AssetName);
};
