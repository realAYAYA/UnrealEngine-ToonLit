// Copyright Epic Games, Inc. All Rights Reserved.
#include "TextureExporter.h"
#include "2D/Tex.h"
#include "Async/ParallelFor.h"
#include "Data/TiledBlob.h"
#include "Job/JobBatch.h"
#include "Job/Scheduler.h"
#include "Misc/Paths.h"
#include "TextureGraphEngine.h"
#include "Model/Mix/MixSettings.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "T_ExportAsUAsset.h"
#include "UObject/SavePackage.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include <Engine/Texture2D.h>
#include "2D/TextureHelper.h"
#include "TextureResource.h"
#if WITH_EDITOR
#include "FileHelpers.h"
#include "AssetToolsModule.h"
#endif
#include <Misc/PackageName.h> 
#include "TextureCompiler.h"
DEFINE_LOG_CATEGORY(ExportLogs);
bool TextureExporter::IsPackageNameValid(FString Path, FString AssetName)
{
	return FPackageName::TryConvertLongPackageNameToFilename(Path, AssetName);
}
AsyncBool TextureExporter::ExportAllMapsAsUAsset(UMixInterface* MixObj, const FString& OutputFolder, const FExportSettings& ExportSettings)
{
	FInvalidationDetails Details;
	Details.Mix = MixObj;
	Details.All();
	JobBatchPtr Batch = JobBatch::Create(Details);
	MixUpdateCyclePtr Cycle = Batch->GetCycle();
	const ExportMaps Maps = ExportSettings.ExportPreset;
	for (const auto& ExportMap : Maps)
	{
		auto& MapSettings = ExportMap.second;
		FString Path = MapSettings.Path;
		if (ExportMap.second.UseOverridePath)
		{
			Path = OutputFolder;
		}
		T_ExportAsUAsset::ExportAsUAsset(Cycle, 0, ExportMap.second, Path);
	}
	TextureGraphEngine::GetScheduler()->AddBatch(Batch);
	return cti::make_continuable<bool>([=](auto&& Promise) mutable
	{
		Batch->OnDone([=, FWD_PROMISE(Promise)](JobBatch*) mutable
		{
			Promise.set_value(true);
		});
	});
}
AsyncInt TextureExporter::ExportRawAsUAsset(RawBufferPtr RawObj,const FExportMapSettings& Setting, const FString& CompletePath, const FName& TextureName)
{
	return cti::make_continuable<int32>([RawObj,Setting, CompletePath, TextureName](auto Promise) mutable {
		FText PathError;
		FPaths::ValidatePath(CompletePath, &PathError);
		if (CompletePath.IsEmpty())
		{
			UE_LOG(LogTexture, Warning, TEXT("Provide path is empty!"));
			Promise.set_value(-1);
		}
		else if (!PathError.IsEmpty())
		{
			UE_LOG(LogTexture, Warning, TEXT("Invalid file path provided: %s"), *(PathError.ToString()));
			Promise.set_value(-1);
		}
		
		if (!RawObj || RawObj->GetData() == nullptr)
		{
			UE_LOG(LogTexture, Warning, TEXT("Can't export with invalid RawBuffer: %s"), *(RawObj->GetName()));
			Promise.set_value(-1);
		}
		
		const EPixelFormat PixelFormat = BufferDescriptor::BufferPixelFormat(RawObj->GetDescriptor().Format, RawObj->GetDescriptor().ItemsPerPoint);
		const ETextureSourceFormat SourceFormat = TextureHelper::GetTextureSourceFormat(RawObj->GetDescriptor().Format, RawObj->GetDescriptor().ItemsPerPoint);
		FString PackageName = FPaths::Combine(CompletePath, TextureName.ToString());
		FString SuggestedName = TextureName.ToString();
		FPackagePath PackagePath;
#if WITH_EDITOR
		bool bCanOverriteTexture = true;

		// Get the Asset Registry Module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		// Get all asset data in the Content Browser
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackageName), AssetData);

		if (AssetData.Num() > 0)
		{
			bool bIsTextureAsset = false;
			FString AssetType = "";
			// Iterate through the asset data
			for (const FAssetData& Asset : AssetData)
			{
				if (Asset.IsInstanceOf(UTexture::StaticClass()))
				{
					bIsTextureAsset = true;
					break;
				}
			}
			// It is an error if we are trying to replace an object of a different class
			if (!bIsTextureAsset)
			{
				bCanOverriteTexture = false;
				UE_LOG(LogTexture, Warning, TEXT("Creating a Unique name for the output texture as another asset of different type %s exists with same name %s"), *AssetType, *PackageName);
			}
		}
		//Using override paths when exporting from blueprints
		if (!Setting.OverwriteTextures || !bCanOverriteTexture)
		{
			//Helps creating a unique Name if a file with same name exists
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, SuggestedName);
		}

		const uint8* RawBufferData = RawObj->GetData();
		UPackage* Package = CreatePackage( *PackageName);
		Package->FullyLoad();
		UTexture2D* NewTexture = nullptr;
		int32 WidthToUse = Setting.Width == 0 ? RawObj->Width() : Setting.Width;
		int32 HeightToUse = Setting.Height == 0 ? RawObj->Height() : Setting.Height;
		
		if ((WidthToUse % GPixelFormats[PixelFormat].BlockSizeX) == 0 &&
			(HeightToUse % GPixelFormats[PixelFormat].BlockSizeY) == 0)
		{
			NewTexture = NewObject<UTexture2D>(Package, FName(SuggestedName), RF_Public | RF_Standalone | RF_MarkAsRootSet);
		
			// if there is padding, we need to remove that
			if (RawObj->IsPadded())
			{
				FTexturePlatformData* PlatformData = NewTexture->GetPlatformData();
				if (!PlatformData)
				{	
					PlatformData = new FTexturePlatformData();
					NewTexture->SetPlatformData(PlatformData);
				}
				PlatformData->SizeX = WidthToUse;
				PlatformData->SizeY = HeightToUse;
				PlatformData->SetNumSlices(1);
				PlatformData->PixelFormat = PixelFormat;
   
				// Get First MipMap
				//
				FTexture2DMipMap* Mip;
				if (PlatformData->Mips.IsEmpty())
				{
					Mip = new FTexture2DMipMap(0, 0);
					PlatformData->Mips.Add(Mip);
				}
				else
				{
					Mip = &PlatformData->Mips[0];
        
				}
				Mip->SizeX = WidthToUse;
				Mip->SizeY = HeightToUse;
				Mip->SizeZ = 1;
				
				const size_t DestSize = RawObj->GetUnpaddedSize();
				
				Mip->BulkData.Lock(LOCK_READ_WRITE);
				uint8* TextureData = Mip->BulkData.Realloc(DestSize);
				RawObj->CopyUnpaddedBytes(TextureData);
				Mip->BulkData.Unlock();
				
				RawBufferData = TextureData;
			}
		}
		else
		{
			UE_LOG(LogTexture, Warning, TEXT("Cannot Export specified Pixel Format %d as UTexture2D"), PixelFormat);
			Promise.set_value(-1);
			return;
		}
		
		NewTexture->SRGB = Setting.IsSRGB;
		NewTexture->LODGroup = Setting.LODGroup;
		NewTexture->CompressionSettings = Setting.Compression;

		NewTexture->Source.Init(WidthToUse, HeightToUse, 1,1, SourceFormat, RawBufferData);
		NewTexture->UpdateResource();
		FAssetRegistryModule::AssetCreated(NewTexture);
		Package->FullyLoad();
		FTextureCompilingManager::Get().FinishCompilation({NewTexture});
		bool success = UEditorLoadingAndSavingUtils::SavePackages({ Package }, false);
		Promise.set_value(success);
#else
		Promise.set_value(-1);
#endif
	});
}
AsyncBool TextureExporter::ExportAsUAsset(UMixInterface* MixObj, const FExportSettings& ExportSettings, FString CurrentPath)
{
	return ExportAllMapsAsUAsset(MixObj, CurrentPath, ExportSettings);
}
FExportMapSettings TextureExporter::GetExportSettingsForTarget(FExportSettings& ExportSettings, TiledBlobPtr BlobObj, FName Name)
{
	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender())
		return FExportMapSettings();
	ExportSettings.PresetName = "ExportAsUAssetPreset";
	
	FExportMapSettings TargetMap;
	TargetMap.Name = Name;
	TargetMap.IsSRGB = BlobObj->GetDescriptor().bIsSRGB;
	TargetMap.Map = BlobObj;
	TargetMap.Width = TargetMap.Map->GetWidth();
	TargetMap.Height = TargetMap.Map->GetHeight();
	TargetMap.Compression = TextureCompressionSettings::TC_Default;
	TargetMap.OnDone.BindRaw(&ExportSettings, &FExportSettings::OnMapExportDone);
	return TargetMap;
}
void TextureExporter::ExportAsUAsset(UMixInterface* MixObj, FName AssetName, FString CurrentPath, const FExportSettings& Setting)
{
	ExportAllMapsAsUAsset(MixObj, CurrentPath, Setting);
}