// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Vector.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "DirectoryWatcherModule.h"
#include "LandscapeFileFormatInterface.h"
#include "LandscapeEditorModule.h"

#include "Delegates/IDelegateInstance.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor"

struct FLandscapeImageDataRef
{
	TSharedPtr<TArray<uint8>> Data;
	FIntPoint Resolution;
	ELandscapeImportResult Result;
	FText ErrorMessage;
	int32 BytesPerPixel;
};

class FLandscapeImageFileCache
{
public:
	FLandscapeImageFileCache();
	~FLandscapeImageFileCache();


	template<typename T>
	FLandscapeFileInfo FindImage(const TCHAR* InImageFilename, FLandscapeImageDataRef& OutImageData)
	{
		FCacheEntry* CacheEntry = CachedImages.Find(InImageFilename);
		FLandscapeFileInfo Result;

		if (CacheEntry)
		{
			CacheEntry->UsageCount++;
			OutImageData = CacheEntry->ImageData;
			Result.PossibleResolutions.Add(FLandscapeFileResolution(OutImageData.Resolution.X, OutImageData.Resolution.Y));
			Result.ResultCode = CacheEntry->ImageData.Result;
			Result.ErrorMessage = CacheEntry->ImageData.ErrorMessage;
			return Result;
		}

		FLandscapeImageDataRef NewImageData;
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		const ILandscapeFileFormat<T>* FileFormat = LandscapeEditorModule.GetFormatByExtension<T>(*FPaths::GetExtension(InImageFilename, true));

		if (!FileFormat)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			return Result;
		}

		const FLandscapeFileInfo FileInfo = FileFormat->Validate(InImageFilename);

		if (FileInfo.ResultCode != ELandscapeImportResult::Error && FileInfo.PossibleResolutions.Num() > 0)
		{
			FLandscapeFileResolution ExpectedResolution = FileInfo.PossibleResolutions[0];
			FLandscapeImportData<T> ImportData = FileFormat->Import(InImageFilename, ExpectedResolution);

			const int32 BufferSize = ImportData.Data.Num() * sizeof(T);
			NewImageData.Data = TSharedPtr<TArray<uint8>>(new TArray<uint8>());
			NewImageData.Data->SetNumUninitialized(BufferSize);
			FMemory::Memcpy(NewImageData.Data->GetData(), ImportData.Data.GetData(), BufferSize);
			NewImageData.Resolution = FIntPoint(ExpectedResolution.Width, ExpectedResolution.Height);
			NewImageData.Result = FileInfo.ResultCode;
			NewImageData.ErrorMessage = FileInfo.ErrorMessage;
			NewImageData.BytesPerPixel = BufferSize / (ExpectedResolution.Width * ExpectedResolution.Height);
		}
		else
		{
			return FileInfo;
		}
		
		Trim();
		OutImageData = NewImageData;
		Add(FString(InImageFilename), OutImageData);
		
		Result.PossibleResolutions.Add(FLandscapeFileResolution(OutImageData.Resolution.X, OutImageData.Resolution.Y));
		Result.ResultCode = FileInfo.ResultCode;
		Result.ErrorMessage = FileInfo.ErrorMessage;

		return Result;
	}

	void SetMaxSize(uint64 InNewMaxSize);

	void Clear();

private:

	struct FCacheEntry
	{
		FCacheEntry(FLandscapeImageDataRef ImageData) : ImageData(ImageData) {}

		uint32 UsageCount = 1;
		FLandscapeImageDataRef ImageData;
	};

	struct FDirectoryMonitor
	{
		FDirectoryMonitor(FDelegateHandle Handle) { MonitorHandle = Handle; }
		int32 NumFiles = 1;
		FDelegateHandle MonitorHandle;
	};
	
	void OnLandscapeSettingsChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);
	 
	bool MonitorFile(const FString& Filename);
	void UnmonitorFile(const FString& Filename);

	void MonitorCallback(const TArray<struct FFileChangeData>& Changes);

	TMap<FString, FCacheEntry> CachedImages;
	
	TMap<FString, FDirectoryMonitor> MonitoredDirs;

	void Add(const FString& Filename, FLandscapeImageDataRef NewImageData);
	void Remove(const FString& Filename);

	void Trim();

	uint64 MaxCacheSize = 32 * 1024 * 1024;
	uint64 CacheSize = 0;
	FDelegateHandle SettingsChangedHandle;
};

#undef LOCTEXT_NAMESPACE 
