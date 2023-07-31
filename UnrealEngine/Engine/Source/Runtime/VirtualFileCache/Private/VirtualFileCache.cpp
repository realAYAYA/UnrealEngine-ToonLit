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

TSharedRef<IVirtualFileCache> IVirtualFileCache::CreateVirtualFileCache()
{
	static TWeakPtr<FVirtualFileCache> GVFC;
	TSharedPtr<FVirtualFileCache> SharedVFC;
	if (!GVFC.IsValid())
	{
		SharedVFC = MakeShared<FVirtualFileCache>();
		GVFC = SharedVFC;
	}
	return GVFC.IsValid() ? GVFC.Pin().ToSharedRef() : SharedVFC.ToSharedRef();
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
}

FIoStatus FVirtualFileCache::WriteData(VFCKey Id, const uint8* Data, uint64 DataSize)
{
	Thread.RequestWrite(Id, MakeArrayView(Data, DataSize));
	return FIoStatus(EIoErrorCode::Ok);
}

TFuture<TArray<uint8>> FVirtualFileCache::ReadData(VFCKey Id, int64 ReadOffset, int64 ReadSizeOrZero)
{
	return Thread.RequestRead(Id, ReadOffset, ReadSizeOrZero);
}

bool FVirtualFileCache::DoesChunkExist(const VFCKey& Id) const
{
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
