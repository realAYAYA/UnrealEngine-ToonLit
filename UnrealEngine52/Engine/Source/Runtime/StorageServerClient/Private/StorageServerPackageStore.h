// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/PackageStore.h"

#if !UE_BUILD_SHIPPING

class FStorageServerConnection;
struct FFilePackageStoreEntry;

class FStorageServerPackageStoreBackend
	: public IPackageStoreBackend
{
public:
	FStorageServerPackageStoreBackend(FStorageServerConnection& Connection);
	virtual ~FStorageServerPackageStoreBackend() = default;

	virtual void OnMounted(TSharedRef<const FPackageStoreBackendContext> Context) override
	{
	}

	virtual void BeginRead() override
	{
	}

	virtual void EndRead() override
	{
	}

	virtual EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageIde, FPackageStoreEntry& OutPackageStoreEntry) override;
	
	virtual bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId) override
	{
		return false;
	}

private:
	struct FStoreEntry
	{
		FPackageStoreExportInfo ExportInfo;
		TArray<FPackageId> ImportedPackages;
		TArray<FSHAHash> ShaderMapHashes;
	};
	TMap<FPackageId, FStoreEntry> StoreEntriesMap;
};

#endif