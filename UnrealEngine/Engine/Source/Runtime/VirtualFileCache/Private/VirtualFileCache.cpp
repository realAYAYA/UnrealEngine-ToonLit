// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualFileCache.h"
#include "VirtualFileCacheInternal.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Async/MappedFileHandle.h"

DEFINE_STAT(STAT_FilesAdded);
DEFINE_STAT(STAT_BytesAdded);
DEFINE_STAT(STAT_FilesRemoved);
DEFINE_STAT(STAT_BytesRemoved);
DEFINE_STAT(STAT_FilesEvicted);
DEFINE_STAT(STAT_BytesEvicted);

DEFINE_LOG_CATEGORY(LogVFC);

TSharedRef<IVirtualFileCache, ESPMode::ThreadSafe> IVirtualFileCache::CreateVirtualFileCache()
{
	static TWeakPtr<FVirtualFileCache, ESPMode::ThreadSafe> GVFC;
	TSharedPtr<FVirtualFileCache, ESPMode::ThreadSafe> SharedVFC = GVFC.Pin();
	if (!SharedVFC.IsValid())
	{
		SharedVFC = MakeShared<FVirtualFileCache>();
		GVFC = SharedVFC;
	}
	return SharedVFC.ToSharedRef();
}

void FVirtualFileCache::Shutdown()
{
	Thread.Shutdown();
}

void FVirtualFileCache::Initialize(const FVirtualFileCacheSettings& InSettings)
{
	Settings = InSettings;
	IFileManager& FileManager = IFileManager::Get();
	BasePath = !Settings.OverrideDefaultDirectory.IsEmpty() ? Settings.OverrideDefaultDirectory : GetVFCDirectory();
	if (!FileManager.DirectoryExists(*BasePath))
	{
		FileManager.MakeDirectory(*BasePath, true);
	}

	FFileTableWriter FileTable = Thread.ModifyFileTable();
	FileTable->Initialize(Settings);
	Thread.SetInMemoryCacheSize(Settings.RecentWriteLRUSize);
}

FIoStatus FVirtualFileCache::WriteData(VFCKey Id, const uint8* Data, uint64 DataSize)
{
	{
		FFileTableReader FileTable = Thread.ReadFileTable();
		if (FileTable->DoesChunkExist(Id))
		{
			return FIoStatus(EIoErrorCode::Ok);
		}
	}

	Thread.RequestWrite(Id, MakeArrayView(Data, DataSize));
	return FIoStatus(EIoErrorCode::Ok);
}

TFuture<TArray<uint8>> FVirtualFileCache::ReadData(VFCKey Id, int64 ReadOffset, int64 ReadSizeOrZero)
{
	return Thread.RequestRead(Id, ReadOffset, ReadSizeOrZero);
}

bool FVirtualFileCache::DoesChunkExist(const VFCKey& Id) const
{
	TSharedPtr<TArray<uint8>> CachedData = Thread.MemCache.ReadLockAndFindData(Id);
	if (CachedData.IsValid())
	{
		return true;
	}

	FFileTableReader FileTable = Thread.ReadFileTable();
	return FileTable->DoesChunkExist(Id);
}

TIoStatusOr<uint64> FVirtualFileCache::GetSizeForChunk(const VFCKey& Id) const
{
	FFileTableReader FileTable = Thread.ReadFileTable();
	return FileTable->GetSizeForChunk(Id);
}

void FVirtualFileCache::EraseData(VFCKey Id)
{
	Thread.RequestErase(Id);
}

double FVirtualFileCache::CurrentFragmentation() const
{
	FFileTableReader FileTable = Thread.ReadFileTable();
	return FileTable->CurrentFragmentation();
}

void FVirtualFileCache::Defragment()
{
	FFileTableWriter FileTable = Thread.ModifyFileTable();
	return FileTable->Defragment();
}

int64 FVirtualFileCache::GetTotalSize() const
{
	FFileTableReader FileTable = Thread.ReadFileTable();
	return FileTable->GetTotalSize();
}

int64 FVirtualFileCache::GetUsedSize() const
{
	FFileTableReader FileTable = Thread.ReadFileTable();
	return FileTable->GetUsedSize();
}

uint64 FVirtualFileCache::GetTotalMemCacheHits() const
{
	return Thread.GetTotalMemCacheHits();
}

uint64 FVirtualFileCache::GetTotalMemCacheMisses() const
{
	return Thread.GetTotalMemCacheMisses();
}
