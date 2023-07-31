// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerPackageStore.h"
#include "IO/IoContainerHeader.h"
#include "IO/IoDispatcher.h"
#include "StorageServerConnection.h"
#include "Serialization/MemoryReader.h"

#if !UE_BUILD_SHIPPING

FStorageServerPackageStoreBackend::FStorageServerPackageStoreBackend(FStorageServerConnection& Connection)
{
	FIoChunkId HeaderChunkId = CreateIoChunkId(FIoContainerId::FromName(TEXT("global")).Value(), 0, EIoChunkType::ContainerHeader);
	Connection.ReadChunkRequest(HeaderChunkId, 0, uint64(-1), [this](FStorageServerResponse& Response)
	{
		FIoBuffer Chunk;
		if (Response.SerializeChunk(Chunk))
		{
			FMemoryReaderView Ar(MakeArrayView(reinterpret_cast<const uint8*>(Chunk.Data()), Chunk.DataSize()));
			
			FIoContainerHeader ContainerHeader;
			Ar << ContainerHeader;
			StoreEntriesData = MoveTemp(ContainerHeader.StoreEntries);
			StoreEntriesMap.Reserve(ContainerHeader.PackageIds.Num());
			TArrayView<const FFilePackageStoreEntry> StoreEntries(reinterpret_cast<const FFilePackageStoreEntry*>(StoreEntriesData.GetData()), ContainerHeader.PackageIds.Num());
			int32 Index = 0;
			for (const FFilePackageStoreEntry& StoreEntry : StoreEntries)
			{
				const FPackageId& PackageId = ContainerHeader.PackageIds[Index];
				check(PackageId.IsValid());
				StoreEntriesMap.FindOrAdd(PackageId, &StoreEntry);
				++Index;
			}
		}
	});
}

EPackageStoreEntryStatus FStorageServerPackageStoreBackend::GetPackageStoreEntry(FPackageId PackageId, FPackageStoreEntry& OutPackageStoreEntry)
{
	const FFilePackageStoreEntry* FindEntry = StoreEntriesMap.FindRef(PackageId);
	if (FindEntry)
	{
		OutPackageStoreEntry.ExportInfo.ExportCount = FindEntry->ExportCount;
		OutPackageStoreEntry.ExportInfo.ExportBundleCount = FindEntry->ExportBundleCount;
		OutPackageStoreEntry.ImportedPackageIds = MakeArrayView(FindEntry->ImportedPackages.Data(), FindEntry->ImportedPackages.Num());
		OutPackageStoreEntry.ShaderMapHashes = MakeArrayView(FindEntry->ShaderMapHashes.Data(), FindEntry->ShaderMapHashes.Num());
		return EPackageStoreEntryStatus::Ok;
	}
	else
	{
		return EPackageStoreEntryStatus::Missing;
	}
}

#endif
