// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeImageFileCache.h"
#include "LandscapeSettings.h"

#include "Modules/ModuleManager.h"
#include "Internationalization/Internationalization.h"
#include "IDirectoryWatcher.h"


FLandscapeImageFileCache::FLandscapeImageFileCache()
{
	ULandscapeSettings* Settings = GetMutableDefault<ULandscapeSettings>();
	SettingsChangedHandle = Settings->OnSettingChanged().AddRaw(this, &FLandscapeImageFileCache::OnLandscapeSettingsChanged);
	MaxCacheSize = Settings->MaxImageImportCacheSizeMegaBytes * 1024U * 1024U;
}

FLandscapeImageFileCache::~FLandscapeImageFileCache()
{
	if (UObjectInitialized() && !GExitPurge)
	{
		GetMutableDefault<ULandscapeSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
	}
}

void FLandscapeImageFileCache::MonitorCallback(const TArray<struct FFileChangeData>& Changes)
{
	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Action == FFileChangeData::FCA_Modified || Change.Action == FFileChangeData::FCA_Removed)
		{
			Remove(Change.Filename);
		}
	}
}

bool FLandscapeImageFileCache::MonitorFile(const FString& Filename)
{
	FString Directory = FPaths::GetPath(Filename);

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");

	if (FDirectoryMonitor* Monitor = MonitoredDirs.Find(Directory))
	{
		Monitor->NumFiles++;
		return true;
	}
	else
	{
		FDelegateHandle Handle;
		bool bWatcherResult = DirectoryWatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(Directory, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FLandscapeImageFileCache::MonitorCallback), Handle);
		if (bWatcherResult)
		{
			MonitoredDirs.Add(Directory, FDirectoryMonitor(Handle));
		}
		return bWatcherResult;
	}
}

void FLandscapeImageFileCache::UnmonitorFile(const FString& Filename)
{
	FString Directory = FPaths::GetPath(Filename);

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");

	if (FDirectoryMonitor* Monitor = MonitoredDirs.Find(Directory))
	{
		check(Monitor->NumFiles > 0);

		Monitor->NumFiles--;
		if (Monitor->NumFiles == 0)
		{
			DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(Directory, Monitor->MonitorHandle);
		}

		MonitoredDirs.Remove(Directory);
	}
}

void FLandscapeImageFileCache::Add(const FString& Filename, FLandscapeImageDataRef NewImageData)
{
	check(CachedImages.Find(Filename) == nullptr);

	CachedImages.Add(FString(Filename), FCacheEntry(NewImageData));
	MonitorFile(Filename);
	CacheSize += NewImageData.Data->Num();
}

void FLandscapeImageFileCache::Remove(const FString& Filename)
{
	if (FCacheEntry* CacheEntry = CachedImages.Find(Filename))
	{
		CacheSize -= CacheEntry->ImageData.Data->Num();
		CachedImages.Remove(Filename);
		UnmonitorFile(Filename);
	}
}

void FLandscapeImageFileCache::OnLandscapeSettingsChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (InPropertyChangedEvent.GetPropertyName() == "MaxImageImportCacheSizeMegaBytes")
	{
		ULandscapeSettings* LandscapeSettings = Cast<ULandscapeSettings>(InObject);
		SetMaxSize(LandscapeSettings->MaxImageImportCacheSizeMegaBytes);
	}
}

void FLandscapeImageFileCache::SetMaxSize(uint64 InNewMaxSize)
{
	if (MaxCacheSize != InNewMaxSize)
	{
		MaxCacheSize = InNewMaxSize * 1024U *1024U;
		Trim();
	}
}

void FLandscapeImageFileCache::Clear()
{
	CachedImages.Empty();
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	
	for (const TPair<FString, FDirectoryMonitor>& MonitoredDir : MonitoredDirs)
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(MonitoredDir.Key, MonitoredDir.Value.MonitorHandle);
	}
	MonitoredDirs.Empty();
}


void FLandscapeImageFileCache::Trim()
{
	if (CacheSize < MaxCacheSize)
	{
		return;
	}

	CachedImages.ValueSort([](const FCacheEntry& LHS, const FCacheEntry& RHS) { return  LHS.UsageCount < RHS.UsageCount; });
	TArray<FString> ToRemove;
	uint64 Size = CacheSize;
	for (const TPair<FString, FCacheEntry>& It : CachedImages)
	{
		ToRemove.Add(It.Key);
		uint64 ImageSize = It.Value.ImageData.Data->Num();
		Size -= ImageSize;
		if (Size <= MaxCacheSize)
		{
			break;
		}
	}

	for (const FString& Filename : ToRemove)
	{
		Remove(Filename);
	}
}