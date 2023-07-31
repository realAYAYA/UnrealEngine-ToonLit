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
#if WITH_EDITOR
	OnContentPathMountedDelegateHandle = FPackageName::OnContentPathMounted().AddLambda([this](const FString& InAssetPath, const FString& InFilesystemPath)
		{
			{
				FScopeLock _(&UncookedPackageRootsLock);
				PendingAddUncookedPackageRoots.Add(InFilesystemPath);
			}
			{
				FWriteScopeLock _(EntriesLock);
				bNeedsUncookedPackagesUpdate = true;
			}
		});

	OnContentPathDismountedDelegateHandle = FPackageName::OnContentPathDismounted().AddLambda([this](const FString& InAssetPath, const FString& InFilesystemPath)
		{
			{
				FScopeLock _(&UncookedPackageRootsLock);
				PendingRemoveUncookedPackageRoots.Add(InAssetPath);
				PendingAddUncookedPackageRoots.Remove(InFilesystemPath);
			}
			{
				FWriteScopeLock _(EntriesLock);
				bNeedsUncookedPackagesUpdate = true;
			}
		});

	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);
	{
		FScopeLock _(&UncookedPackageRootsLock);
		PendingAddUncookedPackageRoots.Append(RootPaths);
	}
	{
		FWriteScopeLock _(EntriesLock);
		bNeedsUncookedPackagesUpdate = true;
	}
#endif //if WITH_EDITOR
}

FFilePackageStoreBackend::~FFilePackageStoreBackend()
{
#if WITH_EDITOR
	FPackageName::OnContentPathMounted().Remove(OnContentPathMountedDelegateHandle);
	FPackageName::OnContentPathDismounted().Remove(OnContentPathDismountedDelegateHandle);
#endif
}

void FFilePackageStoreBackend::BeginRead()
{
	EntriesLock.ReadLock();
	if (bNeedsContainerUpdate
#if WITH_EDITOR
		|| bNeedsUncookedPackagesUpdate
#endif
		)
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
#if WITH_EDITOR
	const FUncookedPackage* FindUncookedPackage = UncookedPackagesMap.Find(PackageId);
	if (FindUncookedPackage)
	{
		OutPackageStoreEntry.UncookedPackageName = FindUncookedPackage->PackageName;
		OutPackageStoreEntry.UncookedPackageHeaderExtension = static_cast<uint8>(FindUncookedPackage->HeaderExtension);
		return EPackageStoreEntryStatus::Ok;
	}
#endif
	const FFilePackageStoreEntry* FindEntry = StoreEntriesMap.FindRef(PackageId);
	if (FindEntry)
	{
		OutPackageStoreEntry.ExportInfo.ExportCount = FindEntry->ExportCount;
		OutPackageStoreEntry.ExportInfo.ExportBundleCount = FindEntry->ExportBundleCount;
		OutPackageStoreEntry.ImportedPackageIds = MakeArrayView(FindEntry->ImportedPackages.Data(), FindEntry->ImportedPackages.Num());
		OutPackageStoreEntry.ShaderMapHashes = MakeArrayView(FindEntry->ShaderMapHashes.Data(), FindEntry->ShaderMapHashes.Num());
#if WITH_EDITOR
		const FFilePackageStoreEntry* FindOptionalSegmentEntry = OptionalSegmentStoreEntriesMap.FindRef(PackageId);
		if (FindOptionalSegmentEntry)
		{
			OutPackageStoreEntry.OptionalSegmentExportInfo.ExportCount = FindOptionalSegmentEntry->ExportCount;
			OutPackageStoreEntry.OptionalSegmentExportInfo.ExportBundleCount = FindOptionalSegmentEntry->ExportBundleCount;
			OutPackageStoreEntry.OptionalSegmentImportedPackageIds = MakeArrayView(FindOptionalSegmentEntry->ImportedPackages.Data(), FindOptionalSegmentEntry->ImportedPackages.Num());
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

#if WITH_EDITOR
	if (bNeedsUncookedPackagesUpdate)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateUncookedPackages);
		FScopeLock _(&UncookedPackageRootsLock);

		if (!PendingRemoveUncookedPackageRoots.IsEmpty())
		{
			UE_LOG(LogFilePackageStore, Display, TEXT("Removing uncooked packages from %d old roots..."), PendingRemoveUncookedPackageRoots.Num());
			uint64 TotalRemovedCount = RemoveUncookedPackagesFromRoot(PendingRemoveUncookedPackageRoots);
			PendingRemoveUncookedPackageRoots.Empty();
			UE_LOG(LogFilePackageStore, Display, TEXT("Removed %lld uncooked packages"), TotalRemovedCount);
		}

		if (!PendingAddUncookedPackageRoots.IsEmpty())
		{
			UE_LOG(LogFilePackageStore, Display, TEXT("Adding uncooked packages from %d new roots..."), PendingAddUncookedPackageRoots.Num());
			uint64 TotalAddedCount = 0;
			for (const FString& RootPath : PendingAddUncookedPackageRoots)
			{
				TotalAddedCount += AddUncookedPackagesFromRoot(RootPath);
			}
			PendingAddUncookedPackageRoots.Empty();
			UE_LOG(LogFilePackageStore, Display, TEXT("Added %lld uncooked packages"), TotalAddedCount);
		}
		bNeedsUncookedPackagesUpdate = false;
	}
#endif //if WITH_EDITOR
}

#if WITH_EDITOR
uint64 FFilePackageStoreBackend::AddUncookedPackagesFromRoot(const FString& RootPath)
{
	uint64 Count = 0;
	FPackageName::IteratePackagesInDirectory(RootPath, [this, &RootPath, &Count](const TCHAR* InPackageFileName) -> bool
		{
			FPackagePath PackagePath = FPackagePath::FromLocalPath(InPackageFileName);
			FName PackageName = PackagePath.GetPackageFName();
			if (!PackageName.IsNone())
			{
				FPackageId PackageId = FPackageId::FromName(PackageName);
				FUncookedPackage& UncookedPackage = UncookedPackagesMap.FindOrAdd(PackageId);
				UncookedPackage.PackageName = PackageName;
				UncookedPackage.HeaderExtension = PackagePath.GetHeaderExtension();
				++Count;
			}
			return true;
		});
	return Count;
}

uint64 FFilePackageStoreBackend::RemoveUncookedPackagesFromRoot(const TSet<FString>& RootPaths)
{
	uint64 Count = 0;
	TMap<FPackageId, FUncookedPackage> RemainingUncookedPackagesMap;
	RemainingUncookedPackagesMap.Reserve(UncookedPackagesMap.Num());
	for (auto& Pair : UncookedPackagesMap)
	{
		const FNameBuilder PackageName(Pair.Value.PackageName);
		const FStringView PackageRootPath = FilePackageStore::GetRootPathFromPackageName(PackageName);
		if (RootPaths.ContainsByHash(GetTypeHash(PackageRootPath), PackageRootPath))
		{
			++Count;
		}
		else
		{
			RemainingUncookedPackagesMap.Add(Pair.Key, MoveTemp(Pair.Value));
		}
	}
	UncookedPackagesMap = MoveTemp(RemainingUncookedPackagesMap);
	return Count;
}
#endif //if WITH_EDITOR


//PRAGMA_ENABLE_OPTIMIZATION