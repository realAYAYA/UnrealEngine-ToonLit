// Copyright Epic Games, Inc. All Rights Reserved.
#include "FilePackageStore.h"
#include "IO/IoContainerId.h"
#include "Misc/CommandLine.h"
#include "IO/IoContainerHeader.h"
#include "Misc/ScopeRWLock.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Internationalization/PackageLocalizationManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/PackageName.h"

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY_STATIC(LogFilePackageStore, Log, All);

namespace FilePackageStore
{
	FStringView GetRootPathFromPackageName(const FStringView Path)
	{
		FStringView RootPath;
		int32 SecondForwardSlash = INDEX_NONE;
		if (ensure(FStringView(Path.GetData() + 1, Path.Len() - 1).FindChar(TEXT('/'), SecondForwardSlash)))
		{
			RootPath = FStringView(Path.GetData(), SecondForwardSlash + 2);
		}
		return RootPath;
	}
}

FFilePackageStoreBackend::FFilePackageStoreBackend()
{
}

FFilePackageStoreBackend::~FFilePackageStoreBackend()
{
}

void FFilePackageStoreBackend::BeginRead()
{
	EntriesLock.ReadLock();
	if (bNeedsContainerUpdate)
	{
		Update();
	}
}

void FFilePackageStoreBackend::EndRead()
{
	EntriesLock.ReadUnlock();
}

EPackageStoreEntryStatus FFilePackageStoreBackend::GetPackageStoreEntry(FPackageId PackageId, FPackageStoreEntry& OutPackageStoreEntry)
{
	const FFilePackageStoreEntry* FindEntry = StoreEntriesMap.FindRef(PackageId);
	if (FindEntry)
	{
		OutPackageStoreEntry.ImportedPackageIds = MakeArrayView(FindEntry->ImportedPackages.Data(), FindEntry->ImportedPackages.Num());
		OutPackageStoreEntry.ShaderMapHashes = MakeArrayView(FindEntry->ShaderMapHashes.Data(), FindEntry->ShaderMapHashes.Num());
#if WITH_EDITOR
		const FFilePackageStoreEntry* FindOptionalSegmentEntry = OptionalSegmentStoreEntriesMap.FindRef(PackageId);
		if (FindOptionalSegmentEntry)
		{
			OutPackageStoreEntry.OptionalSegmentImportedPackageIds = MakeArrayView(FindOptionalSegmentEntry->ImportedPackages.Data(), FindOptionalSegmentEntry->ImportedPackages.Num());
			OutPackageStoreEntry.bHasOptionalSegment = true;
		}
#endif
		return EPackageStoreEntryStatus::Ok;
	}
	return EPackageStoreEntryStatus::Missing;
}

bool FFilePackageStoreBackend::GetPackageRedirectInfo(FPackageId PackageId, FName& OutSourcePackageName, FPackageId& OutRedirectedToPackageId)
{
	TTuple<FName, FPackageId>* FindRedirect = RedirectsPackageMap.Find(PackageId);
	if (FindRedirect)
	{
		OutSourcePackageName = FindRedirect->Get<0>();
		OutRedirectedToPackageId = FindRedirect->Get<1>();
		UE_LOG(LogFilePackageStore, Verbose, TEXT("Redirecting from %s to 0x%llx"), *OutSourcePackageName.ToString(), OutRedirectedToPackageId.Value());
		return true;
	}
	
	const FName* FindLocalizedPackageSourceName = LocalizedPackages.Find(PackageId);
	if (FindLocalizedPackageSourceName)
	{
		FName LocalizedPackageName = FPackageLocalizationManager::Get().FindLocalizedPackageName(*FindLocalizedPackageSourceName);
		if (!LocalizedPackageName.IsNone())
		{
			FPackageId LocalizedPackageId = FPackageId::FromName(LocalizedPackageName);
			if (StoreEntriesMap.Find(LocalizedPackageId))
			{
				OutSourcePackageName = *FindLocalizedPackageSourceName;
				OutRedirectedToPackageId = LocalizedPackageId;
				UE_LOG(LogFilePackageStore, Verbose, TEXT("Redirecting from localized package %s to 0x%llx"), *OutSourcePackageName.ToString(), OutRedirectedToPackageId.Value());
				return true;
			}
		}
	}
	return false;
}

void FFilePackageStoreBackend::Mount(const FIoContainerHeader* ContainerHeader, uint32 Order)
{
	LLM_SCOPE(ELLMTag::AsyncLoading);
	FWriteScopeLock _(EntriesLock);
	MountedContainers.Add({ ContainerHeader, Order, NextSequence++ });
	Algo::Sort(MountedContainers, [](const FMountedContainer& A, const FMountedContainer& B)
		{
			if (A.Order == B.Order)
			{
				return A.Sequence > B.Sequence;
			}
			return A.Order > B.Order;
		});
	bNeedsContainerUpdate = true;
}

void FFilePackageStoreBackend::Unmount(const FIoContainerHeader* ContainerHeader)
{
	FWriteScopeLock _(EntriesLock);
	for (auto It = MountedContainers.CreateIterator(); It; ++It)
	{
		if (It->ContainerHeader == ContainerHeader)
		{
			It.RemoveCurrent();
			bNeedsContainerUpdate = true;
			return;
		}
	}
}

void FFilePackageStoreBackend::Update()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateFilePackageStore);

	FScopeLock Lock(&UpdateLock);
	if (bNeedsContainerUpdate)
	{
		StoreEntriesMap.Empty();
		LocalizedPackages.Empty();
		RedirectsPackageMap.Empty();
#if WITH_EDITOR
		OptionalSegmentStoreEntriesMap.Empty();
#endif

		uint32 TotalPackageCount = 0;
		for (const FMountedContainer& MountedContainer : MountedContainers)
		{
			TotalPackageCount += MountedContainer.ContainerHeader->PackageIds.Num();
		}

		StoreEntriesMap.Reserve(TotalPackageCount);
		for (const FMountedContainer& MountedContainer : MountedContainers)
		{
			const FIoContainerHeader* ContainerHeader = MountedContainer.ContainerHeader;
			TArrayView<const FFilePackageStoreEntry> ContainerStoreEntries(reinterpret_cast<const FFilePackageStoreEntry*>(ContainerHeader->StoreEntries.GetData()), ContainerHeader->PackageIds.Num());
			int32 Index = 0;
			for (const FFilePackageStoreEntry& StoreEntry : ContainerStoreEntries)
			{
				const FPackageId& PackageId = ContainerHeader->PackageIds[Index];
				check(PackageId.IsValid());
				StoreEntriesMap.FindOrAdd(PackageId, &StoreEntry);
				++Index;
			}

#if WITH_EDITOR
			TArrayView<const FFilePackageStoreEntry> ContainerOptionalSegmentStoreEntries(reinterpret_cast<const FFilePackageStoreEntry*>(ContainerHeader->OptionalSegmentStoreEntries.GetData()), ContainerHeader->OptionalSegmentPackageIds.Num());
			Index = 0;
			for (const FFilePackageStoreEntry& OptionalSegmentStoreEntry : ContainerOptionalSegmentStoreEntries)
			{
				const FPackageId& PackageId = ContainerHeader->OptionalSegmentPackageIds[Index];
				check(PackageId.IsValid());
				OptionalSegmentStoreEntriesMap.FindOrAdd(PackageId, &OptionalSegmentStoreEntry);
				++Index;
			}
#endif //if WITH_EDITOR

			for (const FIoContainerHeaderLocalizedPackage& LocalizedPackage : ContainerHeader->LocalizedPackages)
			{
				FName& LocalizedPackageSourceName = LocalizedPackages.FindOrAdd(LocalizedPackage.SourcePackageId);
				if (LocalizedPackageSourceName.IsNone())
				{
					FDisplayNameEntryId NameEntry = ContainerHeader->RedirectsNameMap[LocalizedPackage.SourcePackageName.GetIndex()];
					LocalizedPackageSourceName = NameEntry.ToName(LocalizedPackage.SourcePackageName.GetNumber());
				}
			}

			for (const FIoContainerHeaderPackageRedirect& Redirect : ContainerHeader->PackageRedirects)
			{
				TTuple<FName, FPackageId>& RedirectEntry = RedirectsPackageMap.FindOrAdd(Redirect.SourcePackageId);
				FName& SourcePackageName = RedirectEntry.Key;
				if (SourcePackageName.IsNone())
				{
					FDisplayNameEntryId NameEntry = ContainerHeader->RedirectsNameMap[Redirect.SourcePackageName.GetIndex()];
					SourcePackageName = NameEntry.ToName(Redirect.SourcePackageName.GetNumber());
					RedirectEntry.Value = Redirect.TargetPackageId;
				}
			}
		}
		bNeedsContainerUpdate = false;
	}
}

//PRAGMA_ENABLE_OPTIMIZATION