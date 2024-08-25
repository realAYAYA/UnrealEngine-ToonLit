// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_COTF

#include "IO/PackageStore.h"
#include "CookOnTheFly.h"
#include "Misc/ScopeRWLock.h"

class FCookOnTheFlyPackageStoreBackend final
	: public IPackageStoreBackend
{
public:
	struct FEntryInfo
	{
		EPackageStoreEntryStatus Status = EPackageStoreEntryStatus::None;
		int32 EntryIndex = INDEX_NONE;
	};

	struct FPackageStats
	{
		TAtomic<uint32> Cooked{ 0 };
		TAtomic<uint32> Failed{ 0 };
	};

	FCookOnTheFlyPackageStoreBackend(UE::Cook::ICookOnTheFlyServerConnection& InCookOnTheFlyServerConnection);

	virtual void OnMounted(TSharedRef<const FPackageStoreBackendContext> InContext) override
	{
		Context = InContext;
	}

	virtual void BeginRead() override;

	virtual void EndRead() override;

	bool DoesPackageExist(FPackageId PackageId);
	virtual EPackageStoreEntryStatus GetPackageStoreEntry(FPackageId PackageId, FName PackageName,
		FPackageStoreEntry& OutPackageStoreEntry) override;
	
	virtual bool GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId) override
	{
		return false;
	}
	
private:
	void SendCookRequest(TArray<TPair<FPackageId, FName>> PackageIds);
	EPackageStoreEntryStatus CreatePackageStoreEntry(const FEntryInfo& EntryInfo, FPackageStoreEntry& OutPackageStoreEntry);
	void AddPackages(TArray<FPackageStoreEntryResource> Entries, TArray<FPackageId> FailedPackageIds,
		TArray<TPair<FPackageId, FName>> PackageIdsAndNames);
	void OnCookOnTheFlyMessage(const UE::Cook::FCookOnTheFlyMessage& Message);
	void CheckActivity();

	UE::Cook::ICookOnTheFlyServerConnection& CookOnTheFlyServerConnection;
	TSharedPtr<const FPackageStoreBackendContext> Context;
	FRWLock EntriesLock;
	TMap<FPackageId, FEntryInfo> PackageIdToEntryInfo;
	TChunkedArray<FPackageStoreEntryResource> PackageEntries;
	TArray<TPair<FPackageId, FName>> RequestedPackageIds;
	FPackageStats PackageStats;

	const double MaxInactivityTime = 20;
	const double TimeBetweenWarning = 10;
	double LastClientActivtyTime = 0;
	double LastServerActivtyTime = 0;
	double LastWarningTime = 0;
};

#endif // WITH_COTF
