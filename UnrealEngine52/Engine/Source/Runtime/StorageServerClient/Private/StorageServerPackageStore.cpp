// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerPackageStore.h"
#include "IO/IoDispatcher.h"
#include "StorageServerConnection.h"
#include "Serialization/MemoryReader.h"

#if !UE_BUILD_SHIPPING

FStorageServerPackageStoreBackend::FStorageServerPackageStoreBackend(FStorageServerConnection& Connection)
{
	Connection.PackageStoreRequest([this](FPackageStoreEntryResource&& PackageStoreEntryResource)
		{
			FStoreEntry& LocalEntry = StoreEntriesMap.FindOrAdd(PackageStoreEntryResource.GetPackageId());
			LocalEntry.ExportInfo = MoveTemp(PackageStoreEntryResource.ExportInfo);
			LocalEntry.ImportedPackages = MoveTemp(PackageStoreEntryResource.ImportedPackageIds);
			LocalEntry.ShaderMapHashes = MoveTemp(PackageStoreEntryResource.ShaderMapHashes);
		});
}

EPackageStoreEntryStatus FStorageServerPackageStoreBackend::GetPackageStoreEntry(FPackageId PackageId, FPackageStoreEntry& OutPackageStoreEntry)
{
	const FStoreEntry* FindEntry = StoreEntriesMap.Find(PackageId);
	if (FindEntry)
	{
		OutPackageStoreEntry.ExportInfo = FindEntry->ExportInfo;
		OutPackageStoreEntry.ImportedPackageIds = FindEntry->ImportedPackages;
		OutPackageStoreEntry.ShaderMapHashes = FindEntry->ShaderMapHashes;
		return EPackageStoreEntryStatus::Ok;
	}
	else
	{
		return EPackageStoreEntryStatus::Missing;
	}
}

#endif
