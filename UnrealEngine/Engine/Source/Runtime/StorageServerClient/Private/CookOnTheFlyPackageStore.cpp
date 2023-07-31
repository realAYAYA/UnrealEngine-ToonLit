// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookOnTheFlyPackageStore.h"

#if WITH_COTF

#include "HAL/PlatformTime.h"
#include "Containers/ChunkedArray.h"
#include "Serialization/MemoryReader.h"
#include "CookOnTheFlyMessages.h"
#include "CookOnTheFly.h"
#include "Misc/PackageName.h"

FCookOnTheFlyPackageStoreBackend::FCookOnTheFlyPackageStoreBackend(UE::Cook::ICookOnTheFlyServerConnection& InCookOnTheFlyServerConnection)
	: CookOnTheFlyServerConnection(InCookOnTheFlyServerConnection)
{
	// Index zero is invalid
	PackageEntries.Add();
	CookOnTheFlyServerConnection.OnMessage().AddRaw(this, &FCookOnTheFlyPackageStoreBackend::OnCookOnTheFlyMessage);

	using namespace UE::Cook;
	using namespace UE::ZenCookOnTheFly::Messaging;

	FCookOnTheFlyRequest Request(ECookOnTheFlyMessage::GetCookedPackages);
	
	FCookOnTheFlyResponse Response = CookOnTheFlyServerConnection.SendRequest(Request).Get();

	if (Response.IsOk())
	{
		FCompletedPackages GetCookedPackagesResponse = Response.GetBodyAs<FCompletedPackages>();
		
		UE_LOG(LogCookOnTheFly, Log, TEXT("Got '%d' cooked and '%d' failed packages from server"),
			GetCookedPackagesResponse.CookedPackages.Num(), GetCookedPackagesResponse.FailedPackages.Num());

		AddPackages(MoveTemp(GetCookedPackagesResponse.CookedPackages), MoveTemp(GetCookedPackagesResponse.FailedPackages));

		LastServerActivtyTime = FPlatformTime::Seconds();
	}
	else
	{
		UE_LOG(LogCookOnTheFly, Warning, TEXT("Failed to send 'GetCookedPackages' request"));
	}

	FPackageName::DoesPackageExistOverride().BindLambda([this](FName PackageName)
	{
		return DoesPackageExist(FPackageId::FromName(PackageName));
	});
}

void FCookOnTheFlyPackageStoreBackend::BeginRead()
{
	EntriesLock.ReadLock();
}

void FCookOnTheFlyPackageStoreBackend::EndRead()
{
	TArray<FPackageId> PackageIds = MoveTemp(RequestedPackageIds);
	EntriesLock.ReadUnlock();

	if (PackageIds.Num() > 0)
	{
		SendCookRequest(MoveTemp(PackageIds));
	}
}

bool FCookOnTheFlyPackageStoreBackend::DoesPackageExist(FPackageId PackageId)
{
	FReadScopeLock ReadLock(EntriesLock);
	FEntryInfo EntryInfo = PackageIdToEntryInfo.FindRef(PackageId);
	return EntryInfo.Status != EPackageStoreEntryStatus::Missing;
}

EPackageStoreEntryStatus FCookOnTheFlyPackageStoreBackend::GetPackageStoreEntry(FPackageId PackageId, FPackageStoreEntry& OutPackageStoreEntry)
{
	FEntryInfo& EntryInfo = PackageIdToEntryInfo.FindOrAdd(PackageId, FEntryInfo());

	if (EntryInfo.Status == EPackageStoreEntryStatus::Ok)
	{
		check(EntryInfo.EntryIndex != INDEX_NONE);
		return CreatePackageStoreEntry(EntryInfo, OutPackageStoreEntry);
	}
	else if (EntryInfo.Status == EPackageStoreEntryStatus::None)
	{
		EntryInfo.Status = EPackageStoreEntryStatus::Pending;
		RequestedPackageIds.Add(PackageId);
	}

	return EntryInfo.Status; 
}

void FCookOnTheFlyPackageStoreBackend::SendCookRequest(TArray<FPackageId> PackageIds)
{
	using namespace UE::Cook;
	using namespace UE::ZenCookOnTheFly::Messaging;

	LastClientActivtyTime = FPlatformTime::Seconds();
	
	FCookOnTheFlyRequest Request(ECookOnTheFlyMessage::CookPackage);
	Request.SetBodyTo(FCookPackageRequest { PackageIds });

	FCookOnTheFlyResponse Response = CookOnTheFlyServerConnection.SendRequest(Request).Get();
	FCookPackageResponse CookResponse = Response.GetBodyAs<FCookPackageResponse>();

	if (Response.IsOk())
	{
		AddPackages(MoveTemp(CookResponse.CookedPackages), MoveTemp(CookResponse.FailedPackages));
	}
	else
	{
		UE_LOG(LogCookOnTheFly, Warning, TEXT("Failed to send cook package(s) request"));

		FWriteScopeLock WriteLock(EntriesLock);

		for (const FPackageId& PackageId : PackageIds)
		{
			FEntryInfo& EntryInfo = PackageIdToEntryInfo.FindChecked(PackageId);
			if (EntryInfo.Status == EPackageStoreEntryStatus::Pending)
			{
				EntryInfo.Status = EPackageStoreEntryStatus::Missing;
			}
		}
	}
}

EPackageStoreEntryStatus FCookOnTheFlyPackageStoreBackend::CreatePackageStoreEntry(const FEntryInfo& EntryInfo, FPackageStoreEntry& OutPackageStoreEntry)
{
	if (EntryInfo.Status == EPackageStoreEntryStatus::Ok)
	{
		const FPackageStoreEntryResource& Entry = PackageEntries[EntryInfo.EntryIndex];
		OutPackageStoreEntry.ExportInfo = Entry.ExportInfo;
		OutPackageStoreEntry.ImportedPackageIds = Entry.ImportedPackageIds;
		return EPackageStoreEntryStatus::Ok;
	}
	else
	{
		return EntryInfo.Status;
	}
}

void FCookOnTheFlyPackageStoreBackend::AddPackages(TArray<FPackageStoreEntryResource> Entries, TArray<FPackageId> FailedPackageIds)
{
	FWriteScopeLock WriteLock(EntriesLock);
		
	for (FPackageId FailedPackageId : FailedPackageIds)
	{
		UE_LOG(LogCookOnTheFly, Warning, TEXT("0x%llX [Failed]"), FailedPackageId.ValueForDebugging());
		FEntryInfo& EntryInfo = PackageIdToEntryInfo.FindOrAdd(FailedPackageId, FEntryInfo());
		EntryInfo.Status = EPackageStoreEntryStatus::Missing;
		PackageStats.Failed++;
	}

	for (FPackageStoreEntryResource& Entry : Entries)
	{
		const FPackageId PackageId = Entry.GetPackageId();
		FEntryInfo& EntryInfo = PackageIdToEntryInfo.FindOrAdd(PackageId, FEntryInfo());

		EntryInfo.Status = EPackageStoreEntryStatus::Ok;
		if (EntryInfo.EntryIndex == INDEX_NONE)
		{
			EntryInfo.EntryIndex = PackageEntries.Add();
		}
		PackageEntries[EntryInfo.EntryIndex] = MoveTemp(Entry);
		PackageStats.Cooked++;

		UE_LOG(LogCookOnTheFly, Verbose, TEXT("0x%llX '%s' [OK] (Cooked/Failed='%d/%d')"),
			PackageId.ValueForDebugging(), *PackageEntries[EntryInfo.EntryIndex].PackageName.ToString(), PackageStats.Cooked.Load(), PackageStats.Failed.Load());
	}
}

void FCookOnTheFlyPackageStoreBackend::OnCookOnTheFlyMessage(const UE::Cook::FCookOnTheFlyMessage& Message)
{
	using namespace UE::ZenCookOnTheFly::Messaging;

	switch (Message.GetHeader().MessageType)
	{
		case UE::Cook::ECookOnTheFlyMessage::PackagesCooked:
		{
			FCompletedPackages PackagesCookedMessage = Message.GetBodyAs<FCompletedPackages>();

			UE_LOG(LogCookOnTheFly, Verbose, TEXT("Received 'PackagesCooked' message, Cooked='%d', Failed='%d'"),
				PackagesCookedMessage.CookedPackages.Num(),
				PackagesCookedMessage.FailedPackages.Num());

			AddPackages(MoveTemp(PackagesCookedMessage.CookedPackages), MoveTemp(PackagesCookedMessage.FailedPackages));

			if (Context)
			{
				Context->PendingEntriesAdded.Broadcast();
			}

			break;
		}

		default:
			break;
	}

	LastServerActivtyTime = FPlatformTime::Seconds();
}

void FCookOnTheFlyPackageStoreBackend::CheckActivity()
{
	const double TimeSinceLastClientActivity = FPlatformTime::Seconds() - LastClientActivtyTime;
	const double TimeSinceLastServerActivity = FPlatformTime::Seconds() - LastServerActivtyTime;
	const double TimeSinceLastWarning = FPlatformTime::Seconds() - LastWarningTime;

	if (TimeSinceLastClientActivity > MaxInactivityTime &&
		TimeSinceLastServerActivity > MaxInactivityTime &&
		TimeSinceLastWarning > TimeBetweenWarning)
	{
		LastWarningTime = FPlatformTime::Seconds();

		UE_LOG(LogCookOnTheFly, Log, TEXT("No server response in '%.2lf' seconds"), TimeSinceLastServerActivity);

		UE_LOG(LogCookOnTheFly, Log, TEXT("=== Pending Packages ==="));
		{
			FReadScopeLock ReadLock(EntriesLock);
			for (const auto& KeyValue : PackageIdToEntryInfo)
			{
				const FEntryInfo& EntryInfo = KeyValue.Value;
				if (EntryInfo.Status == EPackageStoreEntryStatus::Pending)
				{
					UE_LOG(LogCookOnTheFly, Log, TEXT("0x%llX"), KeyValue.Key.ValueForDebugging());
				}
			}
		}
	}
}

#endif // WITH_COTF
