// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDataGatherer.h"
#include "AssetDataGathererPrivate.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistryArchive.h"
#include "AssetRegistryPrivate.h"
#include "Async/MappedFileHandle.h"
#include "Async/ParallelFor.h"
#include "Containers/BinaryHeap.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Hash/xxhash.h"
#include "Math/NumericLimits.h"
#include "Memory/MemoryView.h"
#include "Misc/AsciiSet.h"
#include "Misc/Char.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "Misc/TrackedActivity.h"
#include "PackageReader.h"
#include "Serialization/Archive.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

namespace AssetDataGathererConstants
{
	constexpr int32 SingleThreadFilesPerBatch = 3;
	constexpr int32 ExpectedMaxBatchSize = 100;
	constexpr int32 MinSecondsToElapseBeforeCacheWrite = 60;
	static constexpr uint32 CacheSerializationMagic = 0x3339D87B; // Versioning and integrity checking
	static constexpr uint64 CurrentVersion = FAssetRegistryVersion::LatestVersion | (uint64(CacheSerializationMagic) << 32);
}

namespace UE::AssetDataGather::Private
{

/** A structure to hold serialized cache data from async loads before adding it to the Gatherer's main cache. */
struct FCachePayload
{
	TUniquePtr<FName[]> PackageNames;
	TUniquePtr<FDiskCachedAssetData[]> AssetDatas;
	int32 NumAssets = 0;
	bool bSucceeded = false;
	void Reset()
	{
		PackageNames.Reset();
		AssetDatas.Reset();
		NumAssets = 0;
		bSucceeded = false;
	}
};

void SerializeCacheSave(FAssetRegistryWriter& Ar, const TArray<TPair<FName, FDiskCachedAssetData*>>& AssetsToSave);
FCachePayload SerializeCacheLoad(FAssetRegistryReader& Ar);
FCachePayload LoadCacheFile(FStringView CacheFilename);


/** InOutResult = Value, but without shrinking the string to fit. */
void AssignStringWithoutShrinking(FString& InOutResult, FStringView Value)
{
	TArray<TCHAR, FString::AllocatorType>& Result = InOutResult.GetCharArray();
	if (Value.IsEmpty())
	{
		Result.Reset();
	}
	else
	{
		Result.SetNumUninitialized(Value.Len() + 1, false /* bAllowShrinking */);
		FMemory::Memcpy(Result.GetData(), Value.GetData(), Value.Len() * sizeof(Value[0]));
		Result[Value.Len()] = '\0';
	}
}

FDiscoveredPathData::FDiscoveredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, const FDateTime& InPackageTimestamp, EGatherableFileType InType)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, RelPath(InRelPath)
	, PackageTimestamp(InPackageTimestamp)
	, Type(InType)
{
}

FDiscoveredPathData::FDiscoveredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, EGatherableFileType InType)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, RelPath(InRelPath)
	, Type(InType)
{
}

void FDiscoveredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, EGatherableFileType InType)
{
	AssignStringWithoutShrinking(LocalAbsPath, InLocalAbsPath);
	AssignStringWithoutShrinking(LongPackageName, InLongPackageName);
	AssignStringWithoutShrinking(RelPath, InRelPath);
	Type = InType;
}

void FDiscoveredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, const FDateTime& InPackageTimestamp, EGatherableFileType InType)
{
	Assign(InLocalAbsPath, InLongPackageName, InRelPath, InType);
	PackageTimestamp = InPackageTimestamp;
}

SIZE_T FDiscoveredPathData::GetAllocatedSize() const
{
	return LocalAbsPath.GetAllocatedSize() + LongPackageName.GetAllocatedSize() + RelPath.GetAllocatedSize();
}

FGatheredPathData::FGatheredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, const FDateTime& InPackageTimestamp, EGatherableFileType InType)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, PackageTimestamp(InPackageTimestamp)
	, Type(InType)
{
}

FGatheredPathData::FGatheredPathData(const FDiscoveredPathData& DiscoveredData)
	: FGatheredPathData(DiscoveredData.LocalAbsPath, DiscoveredData.LongPackageName, DiscoveredData.PackageTimestamp, DiscoveredData.Type)
{
}

FGatheredPathData::FGatheredPathData(FDiscoveredPathData&& DiscoveredData)
	: LocalAbsPath(MoveTemp(DiscoveredData.LocalAbsPath))
	, LongPackageName(MoveTemp(DiscoveredData.LongPackageName))
	, PackageTimestamp(MoveTemp(DiscoveredData.PackageTimestamp))
	, Type(DiscoveredData.Type)
{
}

void FGatheredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, const FDateTime& InPackageTimestamp, EGatherableFileType InType)
{
	AssignStringWithoutShrinking(LocalAbsPath, InLocalAbsPath);
	AssignStringWithoutShrinking(LongPackageName, InLongPackageName);
	PackageTimestamp = InPackageTimestamp;
	Type = InType;
}

void FGatheredPathData::Assign(const FDiscoveredPathData& DiscoveredData)
{
	Assign(DiscoveredData.LocalAbsPath, DiscoveredData.LongPackageName, DiscoveredData.PackageTimestamp, DiscoveredData.Type);
}

SIZE_T FGatheredPathData::GetAllocatedSize() const
{
	return LocalAbsPath.GetAllocatedSize() + LongPackageName.GetAllocatedSize();
}

FScanDir::FScanDir(FMountDir& InMountDir, FScanDir* InParent, FStringView InRelPath)
	: MountDir(&InMountDir)
	, Parent(InParent)
	, RelPath(InRelPath)
{
	FAssetDataDiscovery& Discovery = InMountDir.GetDiscovery();
	Discovery.NumDirectoriesToScan.Increment();
	UE_CLOG(Discovery.bIsIdle, LogAssetRegistry, Warning, TEXT("AssetDataGatherer: FScanDir is constructed while bIsIdle=true."));
}

FScanDir::~FScanDir()
{
	// Assert that Shutdown has been called to confirm that the parent no longer has a reference we need to clear.
	check(!MountDir);
}

void FScanDir::Shutdown()
{
	if (!MountDir)
	{
		// Already shutdown
		return;
	}

	// Shutdown all children
	for (TRefCountPtr<FScanDir>& ScanDir : SubDirs)
	{
		// Destruction contract for FScanDir requires that the parent calls Shutdown before dropping the reference
		ScanDir->Shutdown();
		ScanDir.SafeRelease();
	}
	SubDirs.Empty();

	// Update MountDir data that we influence
	if (!bIsComplete)
	{
		MountDir->GetDiscovery().NumDirectoriesToScan.Decrement();
	}

	// Update Parent data that we influence
	if (Parent) // Root ScanDir has no parent
	{
		if (AccumulatedPriority != EPriority::Normal)
		{
			Parent->OnChildPriorityChanged(AccumulatedPriority, -1);
		}
	}

	// Clear backpointers (which also marks us as shutdown)
	MountDir = nullptr;
	Parent = nullptr;
}

bool FScanDir::IsValid() const
{
	return MountDir != nullptr;
}

FMountDir* FScanDir::GetMountDir() const
{
	return MountDir;
}

FStringView FScanDir::GetRelPath() const
{
	return RelPath;
}

EPriority FScanDir::GetPriority() const
{
	return AccumulatedPriority;
}

void FScanDir::AppendLocalAbsPath(FStringBuilderBase& OutFullPath) const
{
	if (!MountDir)
	{
		return;
	}

	if (Parent)
	{
		Parent->AppendLocalAbsPath(OutFullPath);
		FPathViews::AppendPath(OutFullPath, RelPath);
	}
	else
	{
		// The root ScanDir should have an empty RelPath from the MountDir
		check(RelPath.IsEmpty());
		OutFullPath << MountDir->GetLocalAbsPath();
	}
}

FString FScanDir::GetLocalAbsPath() const
{
	TStringBuilder<128> Result;
	AppendLocalAbsPath(Result);
	return FString(Result);
}

void FScanDir::AppendMountRelPath(FStringBuilderBase& OutRelPath) const
{
	if (!MountDir)
	{
		return;
	}

	if (Parent)
	{
		Parent->AppendMountRelPath(OutRelPath);
		FPathViews::AppendPath(OutRelPath, RelPath);
	}
	else
	{
		// The root ScanDir should have an empty RelPath from the MountDir
		check(RelPath.IsEmpty());
	}
}

FString FScanDir::GetMountRelPath() const
{
	TStringBuilder<128> Result;
	AppendMountRelPath(Result);
	return FString(Result);
}

bool FScanDir::FInherited::IsMonitored() const
{
	return IsOnAllowList() && !IsOnDenyList();
}

bool FScanDir::FInherited::IsOnDenyList() const
{
	return bMatchesDenyList && !bIgnoreDenyList;
}

bool FScanDir::FInherited::IsOnAllowList() const
{
	return bIsOnAllowList;
}

bool FScanDir::FInherited::HasSetting() const
{
	return bIsOnAllowList || bMatchesDenyList || bIgnoreDenyList;
}

FScanDir::FInherited::FInherited(const FInherited& Parent, const FInherited& Child)
	: bIsOnAllowList(Parent.bIsOnAllowList || Child.bIsOnAllowList)
	, bMatchesDenyList(Parent.bMatchesDenyList || Child.bMatchesDenyList)
	, bIgnoreDenyList(Parent.bIgnoreDenyList || Child.bIgnoreDenyList)
{
}

void FScanDir::GetMonitorData(FStringView InRelPath, const FInherited& ParentData, FInherited& OutData) const
{
	if (!MountDir)
	{
		OutData = FInherited();
		return;
	}

	FInherited Accumulated(ParentData, DirectData);

	const FScanDir* SubDir = nullptr;
	FStringView FirstComponent;
	FStringView RemainingPath;
	if (!InRelPath.IsEmpty())
	{
		FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
		SubDir = FindSubDir(FirstComponent);
	}
	if (!SubDir)
	{
		OutData = Accumulated;
	}
	else
	{
		SubDir->GetMonitorData(RemainingPath, Accumulated, OutData);
	}
}

bool FScanDir::IsMonitored(const FInherited& ParentData) const
{
	if (!MountDir)
	{
		return false;
	}
	FInherited Accumulated(ParentData, DirectData);
	return Accumulated.IsMonitored();
}

bool FScanDir::ShouldScan(const FInherited& ParentData) const
{
	return !bHasScanned && IsMonitored(ParentData);
}

bool FScanDir::HasScanned() const
{
	return bHasScanned;
}

bool FScanDir::IsComplete() const
{
	return bIsComplete;
}

SIZE_T FScanDir::GetAllocatedSize() const
{
	SIZE_T Result = 0;
	Result += SubDirs.GetAllocatedSize();
	for (const TRefCountPtr<FScanDir>& Value : SubDirs)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += AlreadyScannedFiles.GetAllocatedSize();
	for (const FString& Value : AlreadyScannedFiles)
	{
		Result += Value.GetAllocatedSize();
	}
	Result += RelPath.GetAllocatedSize();
	return Result;
}

FScanDir* FScanDir::GetControllingDir(FStringView InRelPath, bool bIsDirectory, const FInherited& ParentData, FInherited& OutData, FString& OutRelPath)
{
	// GetControllingDir can only be called on valid ScanDirs, which we rely on since we need to call FindOrAddSubDir which relies on that
	check(IsValid());

	FInherited Accumulated(ParentData, DirectData);
	if (InRelPath.IsEmpty())
	{
		if (!bIsDirectory)
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("GetControllingDir called on %s with !bIsDirectory, but we have it recorded as a directory. Returning null."), *GetLocalAbsPath());
			OutData = FInherited();
			OutRelPath.Reset();
			return nullptr;
		}
		else
		{
			OutData = Accumulated;
			OutRelPath = InRelPath;
			return this;
		}
	}

	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	if (RemainingPath.IsEmpty() && !bIsDirectory)
	{
		OutData = Accumulated;
		OutRelPath = InRelPath;
		return this;
	}
	else
	{
		FScanDir* SubDir = nullptr;
		if (ShouldScan(ParentData))
		{
			SubDir = &FindOrAddSubDir(FirstComponent);
		}
		else
		{
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				OutData = Accumulated;
				OutRelPath = InRelPath;
				return this;
			}
		}
		return SubDir->GetControllingDir(RemainingPath, bIsDirectory, Accumulated, OutData, OutRelPath);
	}
}

bool FScanDir::TrySetDirectoryProperties(FStringView InRelPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	// TrySetDirectoryProperties can only be called on valid ScanDirs, which we rely on so we can call FindOrAddSubDir which requires that
	check(IsValid()); 

	SetComplete(false);
	if (InRelPath.IsEmpty())
	{
		// The properties apply to this entire directory
		if (InProperties.IsOnAllowList.IsSet() && DirectData.bIsOnAllowList != *InProperties.IsOnAllowList)
		{
			if (bScanInFlight)
			{
				bScanInFlightInvalidated = true;
			}
			DirectData.bIsOnAllowList = *InProperties.IsOnAllowList;

			if (DirectData.bIsOnAllowList)
			{
				// Since we are setting this directory to be monitored, we need to implement the guarantee that all Monitored flags of its children are set to false
				// We also need to SetComplete false on all directories in between this and a previously allow listed directory, since those non-allow listed parent directories
				// marked themselves complete once their allow listed children finished
				ForEachDescendent([](FScanDir& ScanDir)
					{
						ScanDir.DirectData.bIsOnAllowList = false;
						ScanDir.SetComplete(false);
					});
			}
			else
			{
				// Cancel any scans since they are no longer allow listed
				ForEachDescendent([](FScanDir& ScanDir)
					{
						if (ScanDir.bScanInFlight)
						{
							ScanDir.bScanInFlightInvalidated = true;
						}
					});
			}
		}
		if ((InProperties.MatchesDenyList.IsSet() && DirectData.bMatchesDenyList != *InProperties.MatchesDenyList) ||
			(InProperties.IgnoreDenyList.IsSet() && DirectData.bIgnoreDenyList != *InProperties.IgnoreDenyList))
		{
			if (InProperties.MatchesDenyList.IsSet())
			{
				DirectData.bMatchesDenyList = *InProperties.MatchesDenyList;
			}
			if (InProperties.IgnoreDenyList.IsSet())
			{
				DirectData.bIgnoreDenyList = *InProperties.IgnoreDenyList;
			}
			bool bIgnoreDenyList = false;
			bool bMatchesDenyList = false;
			for (FScanDir* Current = this; Current; Current = Current->Parent)
			{
				bIgnoreDenyList = bIgnoreDenyList || Current->DirectData.bIgnoreDenyList;
				bMatchesDenyList = bMatchesDenyList || Current->DirectData.bMatchesDenyList;
			}
			bool bIsOnDenyList = bMatchesDenyList && !bIgnoreDenyList;

			// Mark all children as incomplete
			// Also cancel any scans since they are now potentially on the deny list
			if (bIsOnDenyList && bScanInFlight)
			{
				bScanInFlightInvalidated = true;
			}
			ForEachDescendent([&bIsOnDenyList](FScanDir& ScanDir)
				{
					if (bIsOnDenyList && ScanDir.bScanInFlight)
					{
						ScanDir.bScanInFlightInvalidated = true;
					}
					ScanDir.SetComplete(false);
				});
		}
		if (InProperties.HasScanned.IsSet())
		{
			bool bNewValue = *InProperties.HasScanned;
			auto SetProperties = [bNewValue](FScanDir& ScanDir)
			{
				if (ScanDir.bScanInFlight)
				{
					ScanDir.bScanInFlightInvalidated = true;
				}
				ScanDir.bHasScanned = bNewValue;
				ScanDir.AlreadyScannedFiles.Reset();
			};
			SetProperties(*this);
			ForEachDescendent(SetProperties);
		}
		if (InProperties.Priority.IsSet() && DirectPriority != *InProperties.Priority)
		{
			SetDirectPriority(*InProperties.Priority);
		}
		return true;
	}
	else
	{
		TOptional<FSetPathProperties> ModifiedProperties;
		const FSetPathProperties* Properties = &InProperties;
		if (Properties->IsOnAllowList.IsSet() && DirectData.bIsOnAllowList)
		{
			// If this directory is set to be monitored, all Monitored flags of its children are unused, are guaranteed set to false, and should not be changed
			ModifiedProperties = *Properties;
			ModifiedProperties->IsOnAllowList.Reset();
			if (!ModifiedProperties->IsSet())
			{
				return false;
			}
			Properties = &ModifiedProperties.GetValue();
		}

		FStringView FirstComponent;
		FStringView Remainder;
		FPathViews::SplitFirstComponent(InRelPath, FirstComponent, Remainder);

		FScanDir* SubDir = nullptr;
		if (bHasScanned &&
			(!Properties->HasScanned.IsSet() || *Properties->HasScanned == true) &&
			(!Properties->IsOnAllowList.IsSet()) &&
			(!Properties->IgnoreDenyList.IsSet()) &&
			(!Properties->MatchesDenyList.IsSet()))
		{
			// If this parent directory has already been scanned and we are not changing the target directory's has-been-scanned value,
			// and the next child subdirectory does not exist, then the child directory has already been scanned and we do not need to set the properties on it.
			// Therefore call FindSubDir instead of FindOrAddSubDir, and abort if we do not find it
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				return false;
			}
		}
		else
		{
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				if (!bConfirmedExists)
				{
					TStringBuilder<256> LocalAbsPath;
					AppendLocalAbsPath(LocalAbsPath);
					FPathViews::AppendPath(LocalAbsPath, InRelPath);
					FFileStatData StatData = IFileManager::Get().GetStatData(LocalAbsPath.ToString());
					if (!StatData.bIsValid || !StatData.bIsDirectory)
					{
						UE_LOG(LogAssetRegistry, Warning, TEXT("SetDirectoryProperties called on %s path %.*s. Ignoring the call."),
							StatData.bIsValid ? TEXT("file") : TEXT("non-existent"), LocalAbsPath.Len(), LocalAbsPath.GetData());
						return false;
					}
					bConfirmedExists = true;
				}
				SubDir = &FindOrAddSubDir(FirstComponent);
			}
		}
		return SubDir->TrySetDirectoryProperties(Remainder, *Properties, bConfirmedExists);
	}
}

void FScanDir::MarkFileAlreadyScanned(FStringView BaseName)
{
	if (bHasScanned)
	{
		return;
	}
	check(FPathViews::IsPathLeaf(BaseName));
	for (const FString& AlreadyScannedFile : AlreadyScannedFiles)
	{
		if (FStringView(AlreadyScannedFile).Equals(BaseName, ESearchCase::IgnoreCase))
		{
			return;
		}
	}
	AlreadyScannedFiles.Emplace(BaseName);
}

void FScanDir::SetDirectPriority(EPriority InPriority)
{
	DirectPriority = InPriority;
	UpdateAccumulatedPriority();
}

void FScanDir::UpdateAccumulatedPriority()
{
	uint32 LocalAccumulated = static_cast<uint32>(DirectPriority);
	for (uint32 PriorityLevel = 0; PriorityLevel < CountEPriority; ++PriorityLevel)
	{
		if (PriorityRefCounts[PriorityLevel] > 0 && PriorityLevel < LocalAccumulated)
		{
			LocalAccumulated = PriorityLevel;
		}
	}

	EPriority LocalEPriority = static_cast<EPriority>(LocalAccumulated);
	if (LocalEPriority != AccumulatedPriority)
	{
		if (Parent)
		{
			if (AccumulatedPriority != EPriority::Normal)
			{
				Parent->OnChildPriorityChanged(AccumulatedPriority, -1);
			}
			if (LocalEPriority != EPriority::Normal)
			{
				Parent->OnChildPriorityChanged(LocalEPriority, 1);
			}
		}
		AccumulatedPriority = LocalEPriority;
	}
}

void FScanDir::OnChildPriorityChanged(EPriority InPriority, int32 Delta)
{
	check(-(int32)TNumericLimits<uint8>::Max() < Delta && Delta < TNumericLimits<uint8>::Max());
	uint8& PriorityRefCount = PriorityRefCounts[static_cast<uint32>(InPriority)];
	check(Delta > 0 || PriorityRefCount >= -Delta);

	if (Delta > 0 && PriorityRefCount >= TNumericLimits<uint8>::Max() - Delta)
	{
		// Mark that the count is now stuck
		PriorityRefCount = TNumericLimits<uint8>::Max();
	}
	else if (Delta < 0 && PriorityRefCount == TNumericLimits<uint8>::Max())
	{
		// The count is stuck, do not decrement it
	}
	else
	{
		PriorityRefCount = (uint8)(PriorityRefCount + Delta);
	}
	UpdateAccumulatedPriority();
}

void FScanDir::SetScanResults(FStringView LocalAbsPath, const FInherited& ParentData, TArrayView<FDiscoveredPathData>& InOutSubDirs, TArrayView<FDiscoveredPathData>& InOutFiles)
{
	SetComplete(false);
	check(!bScanInFlightInvalidated);
	check(MountDir);

	if (!ensure(!bHasScanned))
	{
		return;
	}
	FInherited Accumulated(ParentData, DirectData);

	// Add SubDirectories in the tree for the directories found by the scan, and report the directories as discovered directory paths as well
	for (int32 Index = 0; Index < InOutSubDirs.Num(); )
	{
		FDiscoveredPathData& SubDirPath = InOutSubDirs[Index];
		FScanDir& SubScanDir = FindOrAddSubDir(SubDirPath.RelPath);
		bool bReportResult = SubScanDir.IsMonitored(Accumulated) && MountDir->GetDiscovery().ShouldDirBeReported(SubDirPath.LongPackageName);
		if (!bReportResult)
		{
			Swap(SubDirPath, InOutSubDirs.Last());
			InOutSubDirs = InOutSubDirs.Slice(0, InOutSubDirs.Num() - 1);
		}
		else
		{
			++Index;
		}
	}

	// Add the files that were found in the scan, skipping any files that have already been scanned
	if (InOutFiles.Num())
	{
		auto IsAlreadyScanned = [this, &LocalAbsPath](const FDiscoveredPathData& InFile)
		{
			return Algo::AnyOf(AlreadyScannedFiles, [&InFile](const FString& AlreadyScannedFileRelPath) { return FPathViews::Equals(AlreadyScannedFileRelPath, InFile.RelPath); });
		};
		bool bScanAll = AlreadyScannedFiles.Num() == 0;
		for (int32 Index = 0; Index < InOutFiles.Num(); )
		{
			FDiscoveredPathData& InFile = InOutFiles[Index];
			if (!bScanAll && IsAlreadyScanned(InFile))
			{
				// Remove this file from InOutFiles
				Swap(InFile, InOutFiles.Last());
				InOutFiles = InOutFiles.Slice(0, InOutFiles.Num() - 1);
			}
			else
			{
				++Index;
			}
		}
	}
	AlreadyScannedFiles.Empty();

	MountDir->SetHasStartedScanning();
	bHasScanned = true;
}

void FScanDir::Update(FScanDir*& OutCursor, FInherited& InOutParentData)
{
	check(MountDir);
	if (bIsComplete)
	{
		return;
	}
	if (ShouldScan(InOutParentData))
	{
		OutCursor = this;
		return;
	}

	if (SubDirs.Num())
	{
		FScanDir* SubDirToScan = FindHighestPrioritySubDir();
		if (SubDirToScan)
		{
			OutCursor = SubDirToScan;
			FInherited Accumulated(InOutParentData, DirectData);
			InOutParentData = Accumulated;
			return;
		}
	}

	OutCursor = Parent; // Note this will be null for the root ScanDir
	if (!Parent)
	{
		InOutParentData = FInherited();
	}
	else
	{
		if (Parent->DirectData.bIsOnAllowList)
		{
			// We have a contract that bIsOnAllowList is only set on the highest-level directory to monitor and
			// applies to all directories under it. So we only need to change ParentData.bIsOnAllowList from true to
			// false when we move up the tree into a parent directory with bIsOnAllowList = true.
			check(!this->DirectData.bIsOnAllowList); // Verify children below allow list true are allow list false
			check(!Parent->Parent || !Parent->Parent->DirectData.bIsOnAllowList); // Verify above allow list true is allow list false
			check(InOutParentData.bIsOnAllowList); // Verify original InOutParentData matches parent's direct value
			InOutParentData.bIsOnAllowList = false;
		}
		if (Parent->DirectData.bMatchesDenyList || Parent->DirectData.bIgnoreDenyList)
		{
			// We don't have the same set-once contract for deny list information, so when we find a direct deny list parent
			// we have to recalculate the parent's deny listed information by checking all ancestors.
			InOutParentData.bMatchesDenyList = false;
			InOutParentData.bIgnoreDenyList = false;
			for (FScanDir* Current = Parent->Parent; Current; Current = Current->Parent)
			{
				InOutParentData.bMatchesDenyList = InOutParentData.bMatchesDenyList || Current->DirectData.bMatchesDenyList;
				InOutParentData.bIgnoreDenyList = InOutParentData.bIgnoreDenyList || Current->DirectData.bIgnoreDenyList;
			}
		}
	}

	SetComplete(true);
	// After calling SetComplete, this may have been removed from tree and should no longer run calculations
}

FScanDir* FScanDir::GetFirstIncompleteScanDir()
{
	for (const TRefCountPtr<FScanDir>& SubDir : SubDirs)
	{
		FScanDir* Result = SubDir->GetFirstIncompleteScanDir();
		if (Result)
		{
			return Result;
		}
	}
	if (!bIsComplete)
	{
		return this;
	}
	return nullptr;
}

bool FScanDir::IsScanInFlight() const
{
	return bScanInFlight;
}

void FScanDir::SetScanInFlight(bool bInScanInFlight)
{
	bScanInFlight = bInScanInFlight;
}

bool FScanDir::IsScanInFlightInvalidated() const
{
	return bScanInFlightInvalidated;
}

void FScanDir::SetScanInFlightInvalidated(bool bInvalidated)
{
	bScanInFlightInvalidated = bInvalidated;
}

void FScanDir::MarkDirty(bool bMarkDescendents)
{
	if (bMarkDescendents)
	{
		ForEachDescendent([](FScanDir& Descendent) { Descendent.SetComplete(false); });
	}
	FScanDir* Current = this;
	while (Current)
	{
		Current->SetComplete(false);
		Current = Current->Parent;
	}
}

void FScanDir::Shrink()
{
	ForEachSubDir([](FScanDir& SubDir) {SubDir.Shrink(); });
	SubDirs.Shrink();
	AlreadyScannedFiles.Shrink();
}


void FScanDir::SetComplete(bool bInIsComplete)
{
	if (!MountDir || bIsComplete == bInIsComplete)
	{
		return;
	}

	bIsComplete = bInIsComplete;
	if (bIsComplete)
	{
		MountDir->GetDiscovery().NumDirectoriesToScan.Decrement();
		// If we were given a priority, remove it when we complete
		SetDirectPriority(EPriority::Normal);
		// All subDirs are complete, so all of their priorities should be set back to normal, so we can unstick any stuck priorities now by setting them all to 0
#if DO_CHECK
		bool bHasPriority = false;
		ForEachSubDir([&bHasPriority](FScanDir& SubDir) { if (SubDir.GetPriority() != EPriority::Normal) { bHasPriority = true; }});
		if (bHasPriority)
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanDir %s is marked complete, but it has subdirectories with still-set priorities."), *GetLocalAbsPath());
		}
		else
#endif
		{
			bool bModifiedRefCount = false;
			for (uint8& PriorityRefCount : PriorityRefCounts)
			{
				bModifiedRefCount = bModifiedRefCount | (PriorityRefCount != 0);
				PriorityRefCount = 0;
			}
			if (bModifiedRefCount)
			{
				UpdateAccumulatedPriority();
			}
		}
		// Upon completion, subdirs that do not need to be maintained are deleted, which is done by removing them from the parent
		// ScanDirs need to be maintained if they are the root, or have persistent settings, or have child ScanDirs that need to be maintained.
		if (Parent != nullptr && !HasPersistentSettings() && SubDirs.IsEmpty())
		{
			Parent->RemoveSubDir(GetRelPath());
			// *this is Shutdown (e.g. Parent is now null) and it may also have been deallocated
			return;
		}
	}
	else
	{
		FAssetDataDiscovery& Discovery = MountDir->GetDiscovery();
		Discovery.NumDirectoriesToScan.Increment();
		UE_CLOG(Discovery.bIsIdle, LogAssetRegistry, Warning, TEXT("AssetDataGatherer: SetComplete(false) is called while bIsIdle=true."));
	}
}

bool FScanDir::HasPersistentSettings() const
{
	return DirectData.HasSetting();
}

FScanDir* FScanDir::FindSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return nullptr;
	}
	else
	{
		return SubDirs[Index].GetReference();
	}
}

const FScanDir* FScanDir::FindSubDir(FStringView SubDirBaseName) const
{
	return const_cast<FScanDir*>(this)->FindSubDir(SubDirBaseName);
}

FScanDir& FScanDir::FindOrAddSubDir(FStringView SubDirBaseName)
{
	// FindOrAddSubDir is only allowed to be called on valid FScanDirs, which we rely on since we need a non-null MountDir which valid ScanDirs have
	check(MountDir != nullptr);

	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return *SubDirs.EmplaceAt_GetRef(Index, new FScanDir(*MountDir, this, SubDirBaseName));
	}
	else
	{
		return *SubDirs[Index];
	}
}

void FScanDir::RemoveSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index < SubDirs.Num() && FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		// Destruction contract for FScanDir requires that the parent calls Shutdown before dropping the reference
		SubDirs[Index]->Shutdown();
		SubDirs.RemoveAt(Index);
	}
}

int32 FScanDir::FindLowerBoundSubDir(FStringView SubDirBaseName)
{
	return Algo::LowerBound(SubDirs, SubDirBaseName,
		[](const TRefCountPtr<FScanDir>& SubDir, FStringView BaseName)
		{
			return FPathViews::Less(SubDir->GetRelPath(), BaseName);
		}
	);
}

FScanDir* FScanDir::FindHighestPrioritySubDir()
{
	if (SubDirs.Num() == 0)
	{
		return nullptr;
	}

	FScanDir* WinningSubDir = nullptr;
	EPriority WinningPriority = EPriority::Normal;

	for (const TRefCountPtr<FScanDir>& SubDir : SubDirs)
	{
		if (SubDir->bIsComplete)
		{
			continue;
		}
		if (WinningSubDir == nullptr || SubDir->AccumulatedPriority < WinningPriority)
		{
			WinningSubDir = SubDir.GetReference();
			WinningPriority = SubDir->AccumulatedPriority;
		}
	}
	return WinningSubDir;
}

template <typename CallbackType>
void FScanDir::ForEachSubDir(const CallbackType& Callback)
{
	for (TRefCountPtr<FScanDir>& Ptr : SubDirs)
	{
		Callback(*Ptr);
	}
}

/** Depth-first-search traversal of all descedent subdirs under this (not including this). Callback is called on parents before children. */
template <typename CallbackType>
void FScanDir::ForEachDescendent(const CallbackType& Callback)
{
	TArray<TPair<FScanDir*, int32>, TInlineAllocator<10>> Stack; // 10 chosen arbitrarily as a depth that is greater than most of our content root directory tree depths
	Stack.Add(TPair<FScanDir*, int32>(this, 0));
	while (Stack.Num())
	{
		TPair<FScanDir*, int32>& Top = Stack.Last();
		FScanDir* ParentOnStack = Top.Get<0>();
		int32& NextIndex = Top.Get<1>();
		if (NextIndex == ParentOnStack->SubDirs.Num())
		{
			Stack.SetNum(Stack.Num() - 1, false /* bAllowShrinking */);
			continue;
		}
		FScanDir* Child = ParentOnStack->SubDirs[NextIndex++];
		Callback(*Child);
		Stack.Add(TPair<FScanDir*, int32>(Child, 0));
	}
}

FMountDir::FMountDir(FAssetDataDiscovery& InDiscovery, FStringView InLocalAbsPath, FStringView InLongPackageName)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, Discovery(InDiscovery)
{
	Root = new FScanDir(*this, nullptr, FStringView());
	UpdateDenyList();
}

FMountDir::~FMountDir()
{
	// ScanDir's destruction contract requires that the parent calls Shutdown on it before dropping the reference
	Root->Shutdown();
	Root.SafeRelease();
}

FStringView FMountDir::GetLocalAbsPath() const
{
	return LocalAbsPath;
}

FStringView FMountDir::GetLongPackageName() const
{
	return LongPackageName;
}

FAssetDataDiscovery& FMountDir::GetDiscovery() const
{
	return Discovery;
}

FScanDir* FMountDir::GetControllingDir(FStringView InLocalAbsPath, bool bIsDirectory, FScanDir::FInherited& OutData,
	FString& OutRelPath)
{
	FStringView RemainingPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), RemainingPath))
	{
		return nullptr;
	}
	return Root->GetControllingDir(RemainingPath, bIsDirectory, FScanDir::FInherited() /* ParentData */,
		OutData, OutRelPath);
}

SIZE_T FMountDir::GetAllocatedSize() const
{
	SIZE_T Result = sizeof(*Root);
	Result += Root->GetAllocatedSize();
	Result += ChildMountPaths.GetAllocatedSize();
	for (const FString& Value : ChildMountPaths)
	{
		Result += Value.GetAllocatedSize();
	}
	Result += LongPackageName.GetAllocatedSize();
	Result += RelPathsDenyList.GetAllocatedSize();
	for (const FString& Value : RelPathsDenyList)
	{
		Result += Value.GetAllocatedSize();
	}
	return Result;
}

void FMountDir::Shrink()
{
	Root->Shrink();
	ChildMountPaths.Shrink();
	RelPathsDenyList.Shrink();
}

bool FMountDir::IsComplete() const
{
	return Root->IsComplete();
}

EPriority FMountDir::GetPriority() const
{
	return Root->GetPriority();
}

void FMountDir::GetMonitorData(FStringView InLocalAbsPath, FScanDir::FInherited& OutData) const
{
	FStringView QueryRelPath;
	if (!ensure(FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), QueryRelPath)))
	{
		OutData = FScanDir::FInherited();
		return;
	}

	return Root->GetMonitorData(QueryRelPath, FScanDir::FInherited() /* ParentData */, OutData);
}

bool FMountDir::IsMonitored(FStringView InLocalAbsPath) const
{
	FScanDir::FInherited MonitorData;
	GetMonitorData(InLocalAbsPath, MonitorData);
	return MonitorData.IsMonitored();
}

bool FMountDir::TrySetDirectoryProperties(FStringView InLocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	FStringView RelPath;
	if (!ensure(FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), RelPath)))
	{
		return false;
	}
	if (InProperties.IgnoreDenyList.IsSet())
	{
		if (!ensure(!IsChildMountPath(RelPath)))
		{
			// Setting IgnoreDenyList on a child path would break behavior because we use MatchesDenyList to indicate that 
			// the scandir is a child path, and setting it to IgnoreDenyLists will defeat that setting.
			// This should never be called, because setting IgnoreDenyList is only called external to FAssetDataDiscovery, and 
			// FAssetDataDiscovery would call it on the child mount dir instead of this parent mountdir
			FSetPathProperties NewProperties(InProperties);
			NewProperties.IgnoreDenyList.Reset();
			if (!NewProperties.IsSet())
			{
				return false;
			}
			return TrySetDirectoryProperties(InLocalAbsPath, NewProperties, bConfirmedExists);
		}
	}
	return Root->TrySetDirectoryProperties(RelPath, InProperties, bConfirmedExists);
}

void FMountDir::UpdateDenyList()
{
	TSet<FString> RemovedDenyLists;
	for (const FString& Old : RelPathsDenyList)
	{
		RemovedDenyLists.Add(Old);
	}

	RelPathsDenyList.Empty(Discovery.MountRelativePathsDenyList.Num());
	for (const FString& DenyListEntry : Discovery.LongPackageNamesDenyList)
	{
		FStringView MountRelPath;
		if (FPathViews::TryMakeChildPathRelativeTo(DenyListEntry, LongPackageName, MountRelPath))
		{
			// Note that an empty RelPath means we deny the entire mountpoint
			RelPathsDenyList.Emplace(MountRelPath);
		}
	}
	for (const FString& MountRelPath : Discovery.MountRelativePathsDenyList)
	{
		RelPathsDenyList.Emplace(MountRelPath);
	}
	for (const FString& ChildPath : ChildMountPaths)
	{
		RelPathsDenyList.Emplace(ChildPath);
	}

	TSet<FString> AddedDenyListPaths;
	for (const FString& New : RelPathsDenyList)
	{
		if (!RemovedDenyLists.Remove(New))
		{
			AddedDenyListPaths.Add(New);
		}
	}

	TStringBuilder<256> AbsPathDenyList;
	IFileManager& FileManager = IFileManager::Get();
	FSetPathProperties ChangeDenyList;
	ChangeDenyList.MatchesDenyList = true;
	for (const FString& RelPath : AddedDenyListPaths)
	{
		AbsPathDenyList.Reset();
		AbsPathDenyList << LocalAbsPath;
		FPathViews::AppendPath(AbsPathDenyList, RelPath);
		if (FileManager.DirectoryExists(AbsPathDenyList.ToString()))
		{
			Root->TrySetDirectoryProperties(RelPath, ChangeDenyList, true /* bConfirmedExists */);
		}
	}
	ChangeDenyList.MatchesDenyList = false;
	for (const FString& RelPath : RemovedDenyLists)
	{
		// We don't need to check for existence when setting the removal property, because the scandir already exists
		Root->TrySetDirectoryProperties(RelPath, ChangeDenyList, false /* bConfirmedExists */);
	}
}

void FMountDir::Update(FScanDir*& OutCursor, FScanDir::FInherited& OutParentData)
{
	OutParentData = FScanDir::FInherited();
	Root->Update(OutCursor, OutParentData);
}

FScanDir* FMountDir::GetFirstIncompleteScanDir()
{
	return Root->GetFirstIncompleteScanDir();
}

void FMountDir::SetHasStartedScanning()
{
	bHasStartedScanning = true;
}

void FMountDir::AddChildMount(FMountDir* ChildMount)
{
	if (!ChildMount)
	{
		return;
	}
	FStringView RelPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(ChildMount->GetLocalAbsPath(), LocalAbsPath, RelPath))
	{
		return;
	}
	AddChildMountPath(RelPath);
	if (bHasStartedScanning)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("AssetDataGatherer directory %.*s has already started scanning when a new mountpoint was added under it at %.*s. ")
			TEXT("Assets in the new mount point may exist twice in the AssetRegistry under two different package names."),
			LocalAbsPath.Len(), *LocalAbsPath, ChildMount->LocalAbsPath.Len(), *ChildMount->LocalAbsPath);
	}
	UpdateDenyList();
	MarkDirty(RelPath);
}

void FMountDir::RemoveChildMount(FMountDir* ChildMount)
{
	if (!ChildMount)
	{
		return;
	}
	FStringView RelPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(ChildMount->GetLocalAbsPath(), LocalAbsPath, RelPath))
	{
		return;
	}
	if (!RemoveChildMountPath(RelPath))
	{
		return;
	}
	if (ChildMount->bHasStartedScanning)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("AssetDataGatherer directory %.*s has already started scanning when it was removed and merged into its parent mount at %.*s. ")
			TEXT("Assets in the new mount point may exist twice in the AssetRegistry under two different package names."),
			ChildMount->LocalAbsPath.Len(), *ChildMount->LocalAbsPath, LocalAbsPath.Len(), *LocalAbsPath);
	}
	UpdateDenyList();
	MarkDirty(RelPath);
}

void FMountDir::OnDestroyClearChildMounts()
{
	ChildMountPaths.Empty();
}

void FMountDir::SetParentMount(FMountDir* Parent)
{
	ParentMount = Parent;
}

FMountDir* FMountDir::GetParentMount() const
{
	return ParentMount;
}

TArray<FMountDir*> FMountDir::GetChildMounts() const
{
	// Called within Discovery's TreeLock
	TArray<FMountDir*> Result;
	for (const FString& ChildPath : ChildMountPaths)
	{
		TStringBuilder<256> ChildAbsPath;
		ChildAbsPath << LocalAbsPath;
		FPathViews::AppendPath(ChildAbsPath, ChildPath);
		FMountDir* ChildMount = Discovery.FindMountPoint(ChildAbsPath);
		if (ensure(ChildMount)) // This PathData information should have been removed with RemoveChildMount when the child MountDir was removed from the Discovery
		{
			Result.Add(ChildMount);
		}
	}
	return Result;
}

void FMountDir::MarkDirty(FStringView MountRelPath)
{
	FScanDir::FInherited UnusedMonitorData;
	FString ControlRelPath;
	FScanDir* ScanDir = Root->GetControllingDir(MountRelPath, true /* bIsDirectory */, FScanDir::FInherited() /* ParentData */,
		UnusedMonitorData, ControlRelPath);
	if (ScanDir)
	{
		// If a ScanDir exists for the directory that is being marked dirty, mark all of its descendants dirty as well.
		// If the control dir is a parent directory of the requested path, just mark it and its parents dirty
		// Mark all parents dirty in either case
		bool bDirtyAllDescendents = ControlRelPath.IsEmpty();
		ScanDir->MarkDirty(bDirtyAllDescendents);
	}
}

void FMountDir::AddChildMountPath(FStringView MountRelPath)
{
	FString* ExistingPath = ChildMountPaths.FindByPredicate([&MountRelPath](const FString& ChildPath) { return FPathViews::Equals(ChildPath, MountRelPath); });
	if (!ExistingPath)
	{
		ChildMountPaths.Emplace(MountRelPath);
	}
}

bool FMountDir::RemoveChildMountPath(FStringView MountRelPath)
{
	return ChildMountPaths.RemoveAllSwap([MountRelPath](const FString& ChildPath) { return FPathViews::Equals(ChildPath, MountRelPath);  }) != 0;
}

bool FMountDir::IsChildMountPath(FStringView MountRelPath) const
{
	for (const FString& ChildPath : ChildMountPaths)
	{
		if (FPathViews::IsParentPathOf(ChildPath, MountRelPath))
		{
			return true;
		}
	}
	return false;
}


FAssetDataDiscovery::FAssetDataDiscovery(const TArray<FString>& InLongPackageNamesDenyList, const TArray<FString>& InMountRelativePathsDenyList, bool bInIsSynchronous)
	: LongPackageNamesDenyList(InLongPackageNamesDenyList)
	, MountRelativePathsDenyList(InMountRelativePathsDenyList)
	, Thread(nullptr)
	, bIsSynchronous(bInIsSynchronous)
	, bIsIdle(false)
	, IsStopped(0)
	, IsPaused(0)
	, NumDirectoriesToScan(0)
{
	DirLongPackageNamesToNotReport.Add(TEXT("/Game/Collections"));

	if (!bIsSynchronous && !FPlatformProcess::SupportsMultithreading())
	{
		bIsSynchronous = true;
		UE_LOG(LogAssetRegistry, Warning, TEXT("Requested asyncronous asset data discovery, but threading support is disabled. Performing a synchronous discovery instead!"));
	}
}

FAssetDataDiscovery::~FAssetDataDiscovery()
{
	EnsureCompletion();
	Cursor.SafeRelease();
	// Remove pointers to other MountDirs before we delete any of them
	for (TUniquePtr<FMountDir>& MountDir : MountDirs)
	{
		MountDir->SetParentMount(nullptr);
		MountDir->OnDestroyClearChildMounts();
	}
	MountDirs.Empty();
}

void FAssetDataDiscovery::StartAsync()
{
	if (!bIsSynchronous && !Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataDiscovery"), 0, TPri_BelowNormal);
		checkf(Thread, TEXT("Failed to create asset data discovery thread"));
	}
}

bool FAssetDataDiscovery::Init()
{
	return true;
}

uint32 FAssetDataDiscovery::Run()
{
	constexpr float IdleSleepTime = 0.1f;
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		DiscoverStartTime = FPlatformTime::Seconds();
		NumDiscoveredFiles = 0;
	}

	while (!IsStopped)
	{
		{
			CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
			CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
			FGathererScopeLock TickScopeLock(&TickLock);
			while (!IsStopped && !bIsIdle && !IsPaused)
			{
				TickInternal();
			}
		}

		while (!IsStopped && (IsPaused || bIsIdle))
		{
			// No work to do. Sleep for a little and try again later.
			// TODO: Need IsPaused to be a condition variable so we avoid sleeping while waiting for it and then taking a long time to wake after it is unset.
			FPlatformProcess::Sleep(IdleSleepTime);
		} 
	}
	return 0;
}

FAssetDataDiscovery::FScopedPause::FScopedPause(const FAssetDataDiscovery& InOwner)
	:Owner(InOwner)
{
	if (!Owner.bIsSynchronous)
	{
		Owner.IsPaused++;
	}
}

FAssetDataDiscovery::FScopedPause::~FScopedPause()
{
	if (!Owner.bIsSynchronous)
	{
		check(Owner.IsPaused > 0);
		Owner.IsPaused--;
	}
}

void FAssetDataDiscovery::Stop()
{
	IsStopped++;
}

void FAssetDataDiscovery::Exit()
{
}

void FAssetDataDiscovery::EnsureCompletion()
{
	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}


void FAssetDataDiscovery::TickInternal()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	TStringBuilder<256> DirLocalAbsPath;
	TStringBuilder<128> DirLongPackageName;
	TStringBuilder<128> DirMountRelPath;
	int32 DirLongPackageNameRootLen;
	TRefCountPtr<FScanDir> LocalCursor = nullptr;
	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock TreeScopeLock(&TreeLock);
		for (;;)
		{
			// Start at the existing cursor (initializing it if necessary) at call Update until we find a ScanTree that requires scanning
			if (!Cursor || !Cursor->IsValid())
			{
				FScanDir* NewCursor;
				FindFirstCursor(NewCursor, CursorParentData);
				Cursor = NewCursor;
				if (!NewCursor)
				{
					SetIsIdle(true);
					int32 LocalNumDirectoriesToScan = NumDirectoriesToScan.GetValue();
					if (LocalNumDirectoriesToScan != 0)
					{
						FScanDir* Incomplete = nullptr;
						for (TUniquePtr<FMountDir>& MountDir : MountDirs)
						{
							Incomplete = MountDir->GetFirstIncompleteScanDir();
							if (Incomplete)
							{
								break;
							}
						}
						UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataDiscovery::SetIsIdle(true) called when NumDirectoriesToScan == %d.\n")
							TEXT("First incomplete scandir: %s"), LocalNumDirectoriesToScan, Incomplete ? *Incomplete->GetLocalAbsPath() : TEXT("<NoneFound>"));
					}
					return;
				}
			}
			if (Cursor->ShouldScan(CursorParentData))
			{
				SetIsIdle(false);
				break;
			}

			FScanDir* NewCursor = Cursor.GetReference();
			NewCursor->Update(NewCursor, CursorParentData);
			check(NewCursor != Cursor);
			Cursor = NewCursor;
		}
		// IsScanInFlight must be false, because it is not valid to have two TickInternals run at the same time, and we set ScanInFlight back to false after each TickInternal.
		// If ScanInFlight were true here we would not be able to proceed since we currently don't have a way to find the next ScanTree to update without scanning the current one.
		check(!Cursor->IsScanInFlight());

		Cursor->SetScanInFlight(true);
		FMountDir* MountDir = Cursor->GetMountDir();
		check(MountDir);
		Cursor->AppendMountRelPath(DirMountRelPath);
		DirLocalAbsPath << MountDir->GetLocalAbsPath();
		FPathViews::AppendPath(DirLocalAbsPath, DirMountRelPath);
		DirLongPackageName << MountDir->GetLongPackageName();
		FPathViews::AppendPath(DirLongPackageName, DirMountRelPath);
		DirLongPackageNameRootLen = DirLongPackageName.Len();
		LocalCursor = Cursor;
	}

	int32 NumIteratedDirs = 0;
	int32 NumIteratedFiles = 0;
	IFileManager::Get().IterateDirectoryStat(DirLocalAbsPath.ToString(), [this, &DirLocalAbsPath, &DirLongPackageName, DirLongPackageNameRootLen, &NumIteratedDirs, &NumIteratedFiles]
		(const TCHAR* InPackageFilename, const FFileStatData& InPackageStatData)
		{
			FStringView LocalAbsPath(InPackageFilename);
			FStringView RelPath;
			FString Buffer;
			if (!FPathViews::TryMakeChildPathRelativeTo(InPackageFilename, DirLocalAbsPath, RelPath))
			{
				// Try again with the path converted to the absolute path format that we passed in; some IFileManagers can send relative paths to the visitor even though the search path is absolute
				Buffer = FPaths::ConvertRelativePathToFull(FString(InPackageFilename));
				LocalAbsPath = Buffer;
				if (!FPathViews::TryMakeChildPathRelativeTo(Buffer, DirLocalAbsPath, RelPath))
				{
					UE_LOG(LogAssetRegistry, Warning, TEXT("IterateDirectoryStat returned unexpected result %s which is not a child of the requested path %s."), InPackageFilename, DirLocalAbsPath.ToString());
					return true;
				}
			}
			if (FPathViews::GetPathLeaf(RelPath).Len() != RelPath.Len())
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("IterateDirectoryStat returned unexpected result %s which is not a direct child of the requested path %s."), InPackageFilename, DirLocalAbsPath.ToString());
				return true;
			}
			ON_SCOPE_EXIT{ DirLongPackageName.RemoveSuffix(DirLongPackageName.Len() - DirLongPackageNameRootLen); };

			if (InPackageStatData.bIsDirectory)
			{
				FPathViews::AppendPath(DirLongPackageName, RelPath);
				// Don't enter directories that contain invalid packagepath characters (including '.'; extensions are not valid in content directories because '.' is not valid in a packagepath)
				if (!FPackageName::DoesPackageNameContainInvalidCharacters(RelPath))
				{
					if (IteratedSubDirs.Num() < NumIteratedDirs + 1)
					{
						check(IteratedSubDirs.Num() == NumIteratedDirs);
						IteratedSubDirs.Emplace();
					}
					IteratedSubDirs[NumIteratedDirs++].Assign(LocalAbsPath, DirLongPackageName, RelPath, EGatherableFileType::Directory);
				}
			}
			else
			{
				EGatherableFileType FileType = GetFileType(RelPath);
				// Don't record files that contain invalid packagepath characters (not counting their extension) or that do not end with a recognized extension
				if (FileType != EGatherableFileType::Invalid)
				{
					FStringView BaseName = FPathViews::GetBaseFilename(RelPath);
					if (!FPackageName::DoesPackageNameContainInvalidCharacters(BaseName))
					{
						if (IteratedFiles.Num() < NumIteratedFiles + 1)
						{
							check(IteratedFiles.Num() == NumIteratedFiles);
							IteratedFiles.Emplace();
						}
						FPathViews::AppendPath(DirLongPackageName, BaseName);
						IteratedFiles[NumIteratedFiles++].Assign(LocalAbsPath, DirLongPackageName, RelPath, InPackageStatData.ModificationTime, FileType);
					}
				}
			}
			return true;
		});

	TArrayView<FDiscoveredPathData> LocalSubDirs(IteratedSubDirs.GetData(), NumIteratedDirs);
	TArrayView<FDiscoveredPathData> LocalDiscoveredFiles(IteratedFiles.GetData(), NumIteratedFiles);
	bool bValid = false;
	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock TreeScopeLock(&TreeLock);
		if (!LocalCursor->IsValid())
		{
			// The ScanDir has been shutdown, and it is only still allocated to prevent us from crashing. Drop our reference and allow it to delete.
		}
		else if (LocalCursor->IsScanInFlightInvalidated())
		{
			// Some setting has been applied to the ScanDir that requires a new scan
			// Consume the invalidated flag and ignore the results of our scan
			LocalCursor->SetScanInFlightInvalidated(false);
		}
		else
		{
			LocalCursor->SetScanResults(DirLocalAbsPath, CursorParentData, LocalSubDirs, LocalDiscoveredFiles);
			bValid = true;
			FScanDir* NewCursor = Cursor.GetReference();
			// Other thread may have set the cursor to a new spot; in that case do not update and on the next tick start at the new cursor
			if (LocalCursor == NewCursor)
			{
				LocalCursor->Update(NewCursor, CursorParentData);
				Cursor = NewCursor;
			}
		}
		LocalCursor->SetScanInFlight(false);
	}

	if (bValid && (!LocalSubDirs.IsEmpty() || !LocalDiscoveredFiles.IsEmpty()))
	{
		AddDiscovered(DirLocalAbsPath, LocalSubDirs, LocalDiscoveredFiles);
	}
}

void FAssetDataDiscovery::FindFirstCursor(FScanDir*& OutCursor, FScanDir::FInherited& OutParentData)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	OutCursor = nullptr;
	while (!OutCursor)
	{
		EPriority WinningPriority = EPriority::Normal;
		FMountDir* WinningMountDir = nullptr;
		for (TUniquePtr<FMountDir>& MountDir : MountDirs)
		{
			if (MountDir->IsComplete())
			{
				continue;
			}
			if (WinningMountDir == nullptr || MountDir->GetPriority() < WinningPriority)
			{
				WinningMountDir = MountDir.Get();
				WinningPriority = MountDir->GetPriority();
			}
		}

		if (!WinningMountDir)
		{
			OutCursor = nullptr;
			OutParentData = FScanDir::FInherited();
			break;
		}

		WinningMountDir->Update(OutCursor, OutParentData);
		check(OutCursor != nullptr || WinningMountDir->IsComplete()); // The WinningMountDir's update should either return something to update or it should mark itself complete
	}
}

void FAssetDataDiscovery::InvalidateCursor()
{
	if (Cursor)
	{
		if (Cursor->IsScanInFlight())
		{
			Cursor->SetScanInFlightInvalidated(true);
		}
		Cursor.SafeRelease();
	}
}

void FAssetDataDiscovery::SetIsIdle(bool bInIsIdle)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);

	// Caller is responsible for holding TreeLock around this function; writes of SetIsIdle are done inside the TreeLock
	// If bIsIdle is true, caller holds TickLock and TreeLock
	if (bIsIdle == bInIsIdle)
	{
		return;
	}
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	bIsIdle = bInIsIdle;
	if (!bIsSynchronous)
	{
		if (bIsIdle)
		{
			UE_LOG(LogAssetRegistry, Verbose, TEXT("Discovery took %0.6f seconds and found %d files to process"), FPlatformTime::Seconds() - DiscoverStartTime, NumDiscoveredFiles);
		}
		else
		{
			DiscoverStartTime = FPlatformTime::Seconds();
			NumDiscoveredFiles = 0;
		}
	}

	if (bIsIdle)
	{
		CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
		Shrink();
	}
}


void FAssetDataDiscovery::GetAndTrimSearchResults(bool& bOutIsComplete, TArray<FString>& OutDiscoveredPaths, FFilesToSearch& OutFilesToSearch, int32& OutNumPathsToSearch)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	OutDiscoveredPaths.Append(MoveTemp(DiscoveredDirectories));
	DiscoveredDirectories.Reset();

	for (FDirectoryResult& DirectoryResult : DiscoveredFiles)
	{
		OutFilesToSearch.AddDirectory(MoveTemp(DirectoryResult.DirAbsPath), MoveTemp(DirectoryResult.Files));
	}
	DiscoveredFiles.Reset();
	for (FGatheredPathData& FileResult : DiscoveredSingleFiles)
	{
		// Single files are currently only added from the blocking function FAssetDataDiscovery::SetPropertiesAndWait,
		// so we add them at blocking priority.
		OutFilesToSearch.AddPriorityFile(MoveTemp(FileResult));
	}
	DiscoveredSingleFiles.Reset();

	OutNumPathsToSearch = NumDirectoriesToScan.GetValue();
	bOutIsComplete = bIsIdle;
	if (bIsIdle && OutNumPathsToSearch != 0)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataDiscovery::GetAndTrimSearchResults is returning bIsIdle=true while OutNumPathsToSearch=%d."),
			OutNumPathsToSearch);
	}
}

void FAssetDataDiscovery::WaitForIdle()
{
	if (bIsIdle)
	{
		return;
	}
	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	while (!bIsIdle)
	{
		TickInternal();
	}
}

FPathExistence::FPathExistence(FStringView InLocalAbsPath)
	:LocalAbsPath(InLocalAbsPath)
{
}

const FString& FPathExistence::GetLocalAbsPath() const
{
	return LocalAbsPath;
}

FStringView FPathExistence::GetLowestExistingPath()
{
	LoadExistenceData();
	switch (PathType)
	{
	case EType::MissingButDirExists:
		return FPathViews::GetPath(LocalAbsPath);
	case EType::MissingParentDir:
		return FStringView();
	default:
		return LocalAbsPath;
	}
}

FPathExistence::EType FPathExistence::GetType()
{
	LoadExistenceData();
	return PathType;
}

FDateTime FPathExistence::GetModificationTime()
{
	LoadExistenceData();
	return ModificationTime;
}

void FPathExistence::LoadExistenceData()
{
	if (bHasExistenceData)
	{
		return;
	}
	FFileStatData StatData = IFileManager::Get().GetStatData(*LocalAbsPath);
	if (StatData.bIsValid)
	{
		ModificationTime = StatData.ModificationTime;
		PathType = StatData.bIsDirectory ? EType::Directory : EType::File;
	}
	else
	{
		FString ParentPath = FPaths::GetPath(LocalAbsPath);
		StatData = IFileManager::Get().GetStatData(*ParentPath);
		PathType = (StatData.bIsValid && StatData.bIsDirectory)
			? EType::MissingButDirExists : EType::MissingParentDir;
	}

	bHasExistenceData = true;
}

void FAssetDataDiscovery::SetPropertiesAndWait(FPathExistence& QueryPath, bool bAddToAllowList, bool bForceRescan, bool bIgnoreDenyListScanFilters)
{
	// We might have been asked to wait on a filename missing the extension, in which case QueryPath.GetType() == MissingButDirExists
	// We need to handle Directory, File, and MissingButDirExists in unique ways
	FPathExistence::EType PathType = QueryPath.GetType();
	if (PathType == FPathExistence::EType::MissingParentDir)
	{
		// SetPropertiesAndWait is called for every ScanPathsSynchronous, and this is the first spot that checks for existence. 
		// Some systems call ScanPathsSynchronous speculatively to scan whatever is present, so this log is verbose-only.
		UE_LOG(LogAssetRegistry, Verbose, TEXT("SetPropertiesAndWait called on non-existent path %s. Call will be ignored."),
			*QueryPath.GetLocalAbsPath());
		return;
	}
	FStringView SearchPath = QueryPath.GetLowestExistingPath();

	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock TreeScopeLock(&TreeLock);
		FMountDir* MountDir = FindContainingMountPoint(SearchPath);
		if (!MountDir)
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("SetPropertiesAndWait called on %s which is not in a mounted directory. Call will be ignored."),
				*QueryPath.GetLocalAbsPath());
			return;
		}

		if (PathType == FPathExistence::EType::Directory)
		{
			FSetPathProperties Properties;
			if (bAddToAllowList)
			{
				Properties.IsOnAllowList = bAddToAllowList;
			}
			if (bForceRescan)
			{
				Properties.HasScanned = false;
			}
			if (bIgnoreDenyListScanFilters)
			{
				Properties.IgnoreDenyList = true;
			}
			if (Properties.IsSet())
			{
				SetIsIdle(false);
				MountDir->TrySetDirectoryProperties(SearchPath, Properties, true /* bConfirmedExists */);
			}
		}

		FString RelPath;
		FScanDir::FInherited MonitorData;
		bool bSearchPathIsDirectory = PathType == FPathExistence::EType::Directory || PathType == FPathExistence::EType::MissingButDirExists;
		TRefCountPtr<FScanDir> ScanDir = MountDir->GetControllingDir(SearchPath, bSearchPathIsDirectory, MonitorData, RelPath);
		bool bIsAllowedInThisCall = MonitorData.IsOnAllowList() || bAddToAllowList;
		bool bIsDeniedInThisCall = MonitorData.IsOnDenyList() && !bIgnoreDenyListScanFilters;
		bool bIsMonitoredInThisCall = bIsAllowedInThisCall && !bIsDeniedInThisCall;
		if (!ScanDir || !bIsMonitoredInThisCall)
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("SetPropertiesAndWait called on %s which is not monitored. Call will be ignored."),
				*QueryPath.GetLocalAbsPath());
			return;
		}

		if (bSearchPathIsDirectory)
		{
			// If Relpath from the controlling dir to the requested dir is not empty then we have found a parent directory rather than the requested directory.
			// This can only occur for a monitored directory when the requested directory is already complete and we do not need to wait on it.
			if (RelPath.IsEmpty() && !ScanDir->IsComplete())
			{
				// We are going to wait on the path, so set its priority to blocking
				SetIsIdle(false);
				ScanDir->SetDirectPriority(EPriority::Blocking);
				InvalidateCursor();

				FScopedPause ScopedPause(*this);
				TreeScopeLock.Unlock(); // Entering the ticklock, as well as any long duration task such as a tick, has to be done outside of any locks
				// If the query path is MissingButDirExists, we assume it is a file missing the extension, and scan (or confirm already scanned) its directory
				bool bScanEntireTree = PathType == FPathExistence::EType::Directory;

				CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
				CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
				FGathererScopeLock TickScopeLock(&TickLock);
				for (;;)
				{
					TickInternal();
					FGathererScopeLock LoopTreeScopeLock(&TreeLock);
					if (!ScanDir->IsValid())
					{
						break;
					}
					if (ScanDir->IsComplete() || (!bScanEntireTree && ScanDir->HasScanned()))
					{
						break;
					}
					else if (!ensureMsgf(!bIsIdle, TEXT("It should not be possible for the Discovery to go idle while there is an incomplete ScanDir.")))
					{
						break;
					}
				}
			}
		}
		else
		{
			check(PathType == FPathExistence::EType::File);
			bool bAlreadyScanned = ScanDir->HasScanned() && MonitorData.IsMonitored();
			if (!bAlreadyScanned || bForceRescan)
			{
				FStringView RelPathFromParentDir = FPathViews::GetCleanFilename(RelPath);
				EGatherableFileType FileType = GetFileType(RelPathFromParentDir);
				if (FileType != EGatherableFileType::Invalid)
				{
					FStringView FileRelPathNoExt = FPathViews::GetBaseFilenameWithPath(RelPath);
					if (!FPackageName::DoesPackageNameContainInvalidCharacters(FileRelPathNoExt))
					{
						TStringBuilder<256> LongPackageName;
						LongPackageName << MountDir->GetLongPackageName();
						FPathViews::AppendPath(LongPackageName, ScanDir->GetMountRelPath());
						FPathViews::AppendPath(LongPackageName, FileRelPathNoExt);
						AddDiscoveredFile(FDiscoveredPathData(SearchPath, LongPackageName, RelPathFromParentDir, QueryPath.GetModificationTime(), FileType));
						if (FPathViews::IsPathLeaf(RelPath) && !ScanDir->HasScanned())
						{
							SetIsIdle(false);
							ScanDir->MarkFileAlreadyScanned(RelPath);
						}
					}
				}
			}
		}
	}
}

bool FAssetDataDiscovery::TrySetDirectoryProperties(const FString& LocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	if (!InProperties.IsSet())
	{
		return false;
	}
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	if (!TrySetDirectoryPropertiesInternal(LocalAbsPath, InProperties, bConfirmedExists))
	{
		return false;
	}
	InvalidateCursor();
	return true;
}

bool FAssetDataDiscovery::TrySetDirectoryPropertiesInternal(const FString& LocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataGatherer::SetDirectoryProperties called on unmounted path %.*s. Call will be ignored."), LocalAbsPath.Len(), *LocalAbsPath);
		return false;
	}

	return MountDir->TrySetDirectoryProperties(LocalAbsPath, InProperties, bConfirmedExists);
}

bool FAssetDataDiscovery::IsOnAllowList(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		return false;
	}
	FScanDir::FInherited MonitorData;
	MountDir->GetMonitorData(LocalAbsPath, MonitorData);
	return MonitorData.IsOnAllowList();
}

bool FAssetDataDiscovery::IsOnDenyList(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		return false;
	}
	FScanDir::FInherited MonitorData;
	MountDir->GetMonitorData(LocalAbsPath, MonitorData);
	return MonitorData.IsOnDenyList();
}

bool FAssetDataDiscovery::IsMonitored(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	return MountDir && MountDir->IsMonitored(LocalAbsPath);
}

SIZE_T FAssetDataDiscovery::GetAllocatedSize() const
{
	auto GetArrayRecursiveAllocatedSize = [](auto Container)
	{
		SIZE_T Result = Container.GetAllocatedSize();
		for (const auto& Value : Container)
		{
			Result += Value.GetAllocatedSize();
		}
		return Result;
	};

	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	SIZE_T Result = 0;
	Result += GetArrayRecursiveAllocatedSize(LongPackageNamesDenyList);
	Result += GetArrayRecursiveAllocatedSize(MountRelativePathsDenyList);
	Result += GetArrayRecursiveAllocatedSize(DirLongPackageNamesToNotReport);
	if (Thread)
	{
		// TODO: Thread->GetAllocatedSize()
		Result += sizeof(*Thread);
	}

	Result += GetArrayRecursiveAllocatedSize(DiscoveredDirectories);
	Result += GetArrayRecursiveAllocatedSize(DiscoveredFiles);
	Result += GetArrayRecursiveAllocatedSize(DiscoveredSingleFiles);

	Result += MountDirs.GetAllocatedSize();
	for (const TUniquePtr<FMountDir>& Value : MountDirs)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += GetArrayRecursiveAllocatedSize(IteratedSubDirs);
	Result += GetArrayRecursiveAllocatedSize(IteratedFiles);
	return Result;
}

SIZE_T FAssetDataDiscovery::FDirectoryResult::GetAllocatedSize() const
{
	return DirAbsPath.GetAllocatedSize() + Files.GetAllocatedSize();
}

void FAssetDataDiscovery::Shrink()
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	DirLongPackageNamesToNotReport.Shrink();
	DiscoveredDirectories.Shrink();
	DiscoveredFiles.Shrink();
	DiscoveredSingleFiles.Shrink();
	MountDirs.Shrink();
	for (TUniquePtr<FMountDir>& MountDir : MountDirs)
	{
		MountDir->Shrink();
	}
	IteratedSubDirs.Shrink();
	IteratedFiles.Shrink();
}

void FAssetDataDiscovery::AddMountPoint(const FString& LocalAbsPath, FStringView LongPackageName)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	AddMountPointInternal(LocalAbsPath, LongPackageName);

	InvalidateCursor();
}

void FAssetDataDiscovery::AddMountPointInternal(const FString& LocalAbsPath, FStringView LongPackageName)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	TArray<FMountDir*> ChildMounts;
	FMountDir* ParentMount = nullptr;
	bool bExists = false;
	for (TUniquePtr<FMountDir>& ExistingMount : MountDirs)
	{
		if (FPathViews::Equals(ExistingMount->GetLocalAbsPath(), LocalAbsPath))
		{
			bExists = true;
			break;
		}
		else if (FPathViews::IsParentPathOf(ExistingMount->GetLocalAbsPath(), LocalAbsPath))
		{
			// Overwrite any earlier ParentMount; later mounts are more direct parents than earlier mounts
			ParentMount = ExistingMount.Get();
		}
		else if (FPathViews::IsParentPathOf(LocalAbsPath, ExistingMount->GetLocalAbsPath()))
		{
			// A mount under the new directory might be a grandchild mount.
			// Don't add it as a child mount unless there is no other mount in between the new mount and the mount.
			FMountDir* ExistingParentMount = ExistingMount->GetParentMount();
			if (!ExistingParentMount || ExistingParentMount == ParentMount)
			{
				ChildMounts.Add(ExistingMount.Get());
			}
		}
	}
	if (bExists)
	{
		return;
	}

	FMountDir& Mount = FindOrAddMountPoint(LocalAbsPath, LongPackageName);
	if (ParentMount)
	{
		FStringView RelPath;
		verify(FPathViews::TryMakeChildPathRelativeTo(LocalAbsPath, ParentMount->GetLocalAbsPath(), RelPath));
		ParentMount->AddChildMount(&Mount);
		for (FMountDir* ChildMount : ChildMounts)
		{
			ParentMount->RemoveChildMount(ChildMount);
		}
	}
	for (FMountDir* ChildMount : ChildMounts)
	{
		Mount.AddChildMount(ChildMount);
		ChildMount->SetParentMount(ParentMount);
	}
}

void FAssetDataDiscovery::RemoveMountPoint(const FString& LocalAbsPath)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	RemoveMountPointInternal(LocalAbsPath);

	InvalidateCursor();
}

void FAssetDataDiscovery::RemoveMountPointInternal(const FString& LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 ExistingIndex = FindLowerBoundMountPoint(LocalAbsPath);
	if (ExistingIndex == MountDirs.Num() || !FPathViews::Equals(MountDirs[ExistingIndex]->GetLocalAbsPath(), LocalAbsPath))
	{
		return;
	}
	TUniquePtr<FMountDir> Mount = MoveTemp(MountDirs[ExistingIndex]);
	MountDirs.RemoveAt(ExistingIndex);
	FMountDir* ParentMount = Mount->GetParentMount();

	if (ParentMount)
	{
		for (FMountDir* ChildMount : Mount->GetChildMounts())
		{
			ParentMount->AddChildMount(ChildMount);
			ChildMount->SetParentMount(ParentMount);
		}
		ParentMount->RemoveChildMount(Mount.Get());
	}
	else
	{
		for (FMountDir* ChildMount : Mount->GetChildMounts())
		{
			ChildMount->SetParentMount(nullptr);
		}
	}
}

void FAssetDataDiscovery::OnDirectoryCreated(FStringView LocalAbsPath)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir || !MountDir->IsMonitored(LocalAbsPath))
	{
		return;
	}

	FStringView MountRelPath;
	verify(FPathViews::TryMakeChildPathRelativeTo(LocalAbsPath, MountDir->GetLocalAbsPath(), MountRelPath));
	TStringBuilder<128> LongPackageName;
	LongPackageName << MountDir->GetLongPackageName();
	FPathViews::AppendPath(LongPackageName, MountRelPath);
	if (FPackageName::DoesPackageNameContainInvalidCharacters(LongPackageName))
	{
		return;
	}

	// Skip reporting the directory if it is in the deny list of directories to not report
	if (!ShouldDirBeReported(LongPackageName))
	{
		return;
	}

	FDiscoveredPathData DirData;
	DirData.LocalAbsPath = LocalAbsPath;
	DirData.LongPackageName = LongPackageName;
	DirData.RelPath = FPathViews::GetCleanFilename(MountRelPath);

	// Note that we AddDiscovered but do not scan the directory
	// Any files and paths under it will be added by their own event from the directory watcher, so a scan is unnecessary.
	// The directory may also be scanned in the future because a parent directory is still yet pending to scan,
	// we do not try to prevent that wasteful rescan because this is a rare event and it does not cause a behavior problem
	SetIsIdle(false);
	AddDiscovered(DirData.LocalAbsPath, TConstArrayView<FDiscoveredPathData>(&DirData, 1), TConstArrayView<FDiscoveredPathData>());
}

void FAssetDataDiscovery::OnFilesCreated(TConstArrayView<FString> LocalAbsPaths)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	for (const FString& LocalAbsPath : LocalAbsPaths)
	{
		OnFileCreated(LocalAbsPath);
	}
}

void FAssetDataDiscovery::OnFileCreated(const FString& LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	// Detect whether the file should be scanned and if so pass it through to the gatherer
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		// The content root of the file is not registered; ignore it
		return;
	}
	FFileStatData StatData = IFileManager::Get().GetStatData(*LocalAbsPath);
	if (!StatData.bIsValid || StatData.bIsDirectory)
	{
		// The caller has erroneously told us a file exists that doesn't exist (perhaps due to create/delete hysteresis); ignore it
		return;
	}

	FString FileRelPath;
	FScanDir::FInherited MonitorData;
	FScanDir* ScanDir = MountDir->GetControllingDir(LocalAbsPath, false /* bIsDirectory */, MonitorData, FileRelPath);
	if (!ScanDir || !MonitorData.IsMonitored())
	{
		// The new file is in an unmonitored directory; ignore it
		return;
	}

	FStringView RelPathFromParentDir = FPathViews::GetCleanFilename(FileRelPath);
	EGatherableFileType FileType = GetFileType(RelPathFromParentDir);
	if (FileType != EGatherableFileType::Invalid)
	{
		FStringView FileRelPathNoExt = FPathViews::GetBaseFilenameWithPath(FileRelPath);
		if (!FPackageName::DoesPackageNameContainInvalidCharacters(FileRelPathNoExt))
		{
			TStringBuilder<256> LongPackageName;
			LongPackageName << MountDir->GetLongPackageName();
			FPathViews::AppendPath(LongPackageName, ScanDir->GetMountRelPath());
			FPathViews::AppendPath(LongPackageName, FileRelPathNoExt);
			AddDiscoveredFile(FDiscoveredPathData(LocalAbsPath, LongPackageName, RelPathFromParentDir, StatData.ModificationTime, FileType));
			if (FPathViews::IsPathLeaf(FileRelPath))
			{
				ScanDir->MarkFileAlreadyScanned(FileRelPath);
			}
		}
	}
}

FMountDir* FAssetDataDiscovery::FindContainingMountPoint(FStringView LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	// The LowerBound is >= LocalAbsPath, so it is a parentpath of LocalAbsPath only if it is equal to LocalAbsPath
	if (Index < MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		return MountDirs[Index].Get();
	}

	// The last element before the lower bound is either (1) an unrelated path and LocalAbsPath does not have a parent
	// (2) a parent path of LocalAbsPath, (3) A sibling path that is a child of an earlier path that is a parent path of LocalAbsPath
	// (4) An unrelated path that is a child of an earlier path, but none of its parents are a parent path of LocalAbsPath
	// Distinguishing between cases (3) and (4) doesn't have a fast algorithm based on sorted paths alone, but we have recorded the parent
	// so we can figure it out that way
	if (Index > 0)
	{
		FMountDir* Previous = MountDirs[Index - 1].Get();
		while (Previous)
		{
			if (FPathViews::IsParentPathOf(Previous->GetLocalAbsPath(), LocalAbsPath))
			{
				return Previous;
			}
			Previous = Previous->GetParentMount();
		}
	}
	return nullptr;
}

const FMountDir* FAssetDataDiscovery::FindContainingMountPoint(FStringView LocalAbsPath) const
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	return const_cast<FAssetDataDiscovery*>(this)->FindContainingMountPoint(LocalAbsPath);
}

FMountDir* FAssetDataDiscovery::FindMountPoint(FStringView LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	if (Index != MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		return MountDirs[Index].Get();
	}
	return nullptr;
}

FMountDir& FAssetDataDiscovery::FindOrAddMountPoint(FStringView LocalAbsPath, FStringView LongPackageName)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	if (Index != MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		// Already exists
		return *MountDirs[Index];
	}
	return *MountDirs.EmplaceAt_GetRef(Index, new FMountDir(*this, LocalAbsPath, LongPackageName));
}

int32 FAssetDataDiscovery::FindLowerBoundMountPoint(FStringView LocalAbsPath) const
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	return Algo::LowerBound(MountDirs, LocalAbsPath, [](const TUniquePtr<FMountDir>& MountDir, FStringView LocalAbsPath)
		{
			return FPathViews::Less(MountDir->GetLocalAbsPath(), LocalAbsPath);
		}
	);
}

void FAssetDataDiscovery::AddDiscovered(FStringView DirAbsPath, TConstArrayView<FDiscoveredPathData> SubDirs, TConstArrayView<FDiscoveredPathData> Files)
{
	// This function is inside the critical section so we have moved filtering results outside of it
	// Caller is responsible for filtering SubDirs and Files by ShouldScan and packagename validity
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	for (const FDiscoveredPathData& SubDir : SubDirs)
	{
		DiscoveredDirectories.Add(FString(SubDir.LongPackageName));
	}
	DiscoveredFiles.Emplace(DirAbsPath, Files);
	NumDiscoveredFiles += Files.Num();
}

void FAssetDataDiscovery::AddDiscoveredFile(FDiscoveredPathData&& File)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	DiscoveredSingleFiles.Emplace(MoveTemp(File));
	NumDiscoveredFiles++;
}

EGatherableFileType FAssetDataDiscovery::GetFileType(FStringView FilePath)
{
	return FPackageName::IsPackageFilename(FilePath)
		? EGatherableFileType::PackageFile
		: (FAssetDataGatherer::IsVerseFile(FilePath) ? EGatherableFileType::VerseFile : EGatherableFileType::Invalid);
}

FAssetDataDiscovery::FDirectoryResult::FDirectoryResult(FStringView InDirAbsPath, TConstArrayView<FDiscoveredPathData> InFiles)
	: DirAbsPath(InDirAbsPath)
{
	Files.Reserve(InFiles.Num());
	for (const FDiscoveredPathData& DiscoveredFile : InFiles)
	{
		Files.Emplace(DiscoveredFile); // Emplace passes the FDiscoveredPathData to the FGatheredPathData explicit constructor for it
	}

}
bool FAssetDataDiscovery::ShouldDirBeReported(FStringView LongPackageName) const
{
	return !DirLongPackageNamesToNotReport.ContainsByHash(GetTypeHash(LongPackageName), LongPackageName);
}

// Reads an FMemoryView once
class FMemoryViewReader
{
public:
	FMemoryViewReader() = default;
	FMemoryViewReader(FMemoryView Data) : Remaining(Data), TotalSize(Data.GetSize()) {}

	uint64 GetRemainingSize() const { return Remaining.GetSize(); }
	uint64 GetTotalSize() const { return TotalSize; }
	uint64 Tell() const { return TotalSize - Remaining.GetSize(); }

	FMemoryView Load(uint64 Size)
	{
		check(Size <= Remaining.GetSize());
		FMemoryView Out(Remaining.GetData(), Size);
		Remaining += Size;
		return Out;
	}

	void Load(FMutableMemoryView Out)
	{
		FMemoryView In = Load(Out.GetSize());
		FMemory::Memcpy(Out.GetData(), In.GetData(), In.GetSize());
	}

	template<typename T>
	T Load()
	{
		static_assert(std::is_integral_v<T>, "Only integer loading supported");
		static_assert(PLATFORM_LITTLE_ENDIAN, "Byte-swapping not implemented");
		return FPlatformMemory::ReadUnaligned<T>(Load(sizeof(T)).GetData());
	}

	template<typename T>
	TOptional<T> TryLoad()
	{
		return sizeof(T) <= Remaining.GetSize() ? Load<T>() : TOptional<T>();
	}

private:
	FMemoryView Remaining;
	uint64 TotalSize = 0;
};


// Enables both versioning and distinguishing out-of-sync reads from data corruption
static constexpr uint32 BlockMagic = 0xb1a3;

struct FBlockHeader
{
	uint32 Magic = 0;
	uint32 Size = 0;
	uint64 Checksum = 0;
};

TOptional<FBlockHeader> LoadBlockHeader(FMemoryView Data)
{
	check(Data.GetSize() == sizeof(FBlockHeader));

	FMemoryViewReader Reader(Data);
	FBlockHeader Header;
	Header.Magic = Reader.Load<uint32>();
	Header.Size = Reader.Load<uint32>();
	Header.Checksum = Reader.Load<uint64>();

	if (Header.Magic != BlockMagic)
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("Wrong block magic (0x%x)"), Header.Magic);
		return {};
	}

	return Header;
}

static uint64 CalculateBlockChecksum(FMemoryView Data)
{
	return INTEL_ORDER64(FXxHash64::HashBuffer(Data).Hash);
}

class FChecksumArchiveBase : public FArchiveProxy
{
	static constexpr uint32 SaveBlockSize = 4 << 20;

	struct FBlock
	{
		uint8* Begin = nullptr;
		uint8* Cursor = nullptr;
		uint8* End = nullptr;

		explicit FBlock(uint32 Size)				{ Reset(Size); }
		~FBlock()									{ FMemory::Free(Begin); }
		uint64 GetCapacity() const					{ return End - Begin; }
		uint64 GetUsedSize() const					{ return Cursor - Begin; }
		uint64 GetRemainingSize() const				{ return End - Cursor; }
		FMutableMemoryView GetUsed() const			{ return {Begin, GetUsedSize()}; }
		FMutableMemoryView GetRemaining() const		{ return {Cursor, GetRemainingSize()}; };

		void Reset(uint32 Size)
		{
			if (GetCapacity() < Size)
			{
				FMemory::Free(Begin);
				Begin = (uint8*)FMemory::Malloc(Size);
			}
			
			// All blocks have the same size except the last one, which may be smaller.
			// It doesn't matter that we lose some capacity when loading the last block.
			End = Begin + Size;
			Cursor = Begin;
		}

		void Write(FMemoryView In)
		{
			check(GetRemainingSize() >= In.GetSize());
			FMemory::Memcpy(Cursor, In.GetData(), In.GetSize());
			Cursor += In.GetSize();	
		}

		void Read(FMutableMemoryView Out)
		{
			check(GetRemainingSize() >= Out.GetSize());
			FMemory::Memcpy(Out.GetData(), Cursor, Out.GetSize());
			Cursor += Out.GetSize();	
		}
	};

	FBlock Block;

	void SaveBlock()
	{
		FBlockHeader Header;
		Header.Magic = BlockMagic;
		Header.Size = IntCastChecked<uint32>(Block.GetUsedSize());
		Header.Checksum = CalculateBlockChecksum(Block.GetUsed());
		InnerArchive << Header.Magic << Header.Size << Header.Checksum;

		InnerArchive.Serialize(Block.Begin, Header.Size);

		Block.Cursor = Block.Begin;
	}

	bool LoadBlock()
	{
		check(Block.GetRemainingSize() == 0);

		uint8 HeaderData[sizeof(FBlockHeader)];
		InnerArchive.Serialize(HeaderData, sizeof(HeaderData));
		if (InnerArchive.IsError())
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("Couldn't read block header"));
			return false;
		}

		if (TOptional<FBlockHeader> Header = LoadBlockHeader(MakeMemoryView(HeaderData)))
		{
			Block.Reset(Header->Size);

			InnerArchive.Serialize(Block.Begin, Header->Size);

			if (InnerArchive.IsError())
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Couldn't read block data"));
				return false;
			}
			else if (CalculateBlockChecksum(Block.GetRemaining()) != Header->Checksum)
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Wrong block checksum"));
				return false;
			}

			return true;
		}

		return false;
	}

public:
	explicit FChecksumArchiveBase(FArchive& Inner)
	: FArchiveProxy(Inner)
	, Block(IsLoading() ? 0 : SaveBlockSize)
	{}

protected:
	~FChecksumArchiveBase()
	{
		if (!IsLoading() && Block.GetUsedSize() > 0)
		{
			SaveBlock();
		}
	}
	const FBlock& GetCurrentBlock() const
	{
		return Block;
	}

	void Save(FMemoryView Data)
	{
		for (uint64 Size = Block.GetRemainingSize(); Size < Data.GetSize(); Size = Block.GetRemainingSize())
		{
			Block.Write(Data.Left(Size));
			Data += Size;
			SaveBlock();
		}

		Block.Write(Data);
	}

	void Load(FMutableMemoryView Data)
	{
		if (IsError())
		{
			return;
		}

		for (uint64 Size = Block.GetRemainingSize(); Size < Data.GetSize(); Size = Block.GetRemainingSize())
		{
			Block.Read(Data.Left(Size));
			Data += Size;

			if (!LoadBlock())
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Integrity check failed, '%s' cache will be discarded"), *InnerArchive.GetArchiveName());
				SetError();
				return;
			}
		}

		Block.Read(Data);
	}

public:
	// Use FArchive implementations that map back to Serialize() instead of FArchiveProxy overloads
	// that forward to the inner archive and bypass integrity checking
	virtual FArchive& operator<<(FText& Value) override					{ return FArchive::operator<<(Value); }
	virtual void SerializeBits(void* Bits, int64 LengthBits) override	{ FArchive::SerializeBits(Bits, LengthBits); }
	virtual void SerializeInt(uint32& Value, uint32 Max) override		{ FArchive::SerializeInt(Value, Max); }
	virtual void SerializeIntPacked(uint32& Value) override				{ FArchive::SerializeIntPacked(Value); }

private:
	// Wrapping an inner FArchiveUObject is not supported. The inner archive should be low-level
	// and an outer archive should have intercepted these calls.
	virtual FArchive& operator<<(FName& Value) override					{ unimplemented(); return *this; }
	virtual FArchive& operator<<(UObject*& Value) override				{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FObjectPtr& Value) override			{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FField*& Value) override				{ unimplemented(); return *this; }

	virtual void Seek(int64 InPos) override								{ unimplemented(); }
};

class FChecksumArchiveWriter : public FChecksumArchiveBase
{
public:
	using FChecksumArchiveBase::FChecksumArchiveBase;
	virtual void Serialize(void* V, int64 Len) override { Save(FMemoryView(V, Len)); }
	virtual int64 Tell() override { return InnerArchive.Tell() + GetCurrentBlock().GetUsedSize(); }
};

class FChecksumArchiveReader : public FChecksumArchiveBase
{
public:
	using FChecksumArchiveBase::FChecksumArchiveBase;
	virtual void Serialize(void* V, int64 Len) override { Load(FMutableMemoryView(V, Len)); }
	virtual int64 Tell() override { return InnerArchive.Tell() - GetCurrentBlock().GetRemainingSize(); }
};

// Memory-mapped equivalent of FChecksumArchiveReader
class FChecksumViewReader : public FArchive
{
public:
	explicit FChecksumViewReader(FMemoryViewReader&& Reader, FStringView InFileName)
	: RemainingBlocks(MoveTemp(Reader))
	, FileName(InFileName)
	{
		SetIsLoading(true);
	}

private:
	FMemoryViewReader RemainingBlocks;
	FMemoryViewReader CurrentBlock;
	FString FileName;

	virtual void Seek(int64) override { unimplemented(); }
	virtual int64 Tell() override { return RemainingBlocks.Tell() - CurrentBlock.GetRemainingSize(); }
	virtual int64 TotalSize() override { return RemainingBlocks.GetTotalSize(); }

	virtual void Serialize(void* V, int64 Len) override
	{
		FMutableMemoryView Out(V, Len);

		while (CurrentBlock.GetRemainingSize() < Out.GetSize())
		{
			if (IsError())
			{
				return;
			}

			FMutableMemoryView OutSlice(Out.GetData(), CurrentBlock.GetRemainingSize());
			Out += OutSlice.GetSize();
			CurrentBlock.Load(OutSlice);
			check(CurrentBlock.GetRemainingSize() == 0);

			TOptional<FMemoryView> NextBlock = LoadNextBlock(RemainingBlocks);
			if (!NextBlock)
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Integrity check failed, '%s' cache will be discarded"), *FileName);
				SetError();
				return;
			}
			CurrentBlock = *NextBlock;
		}

		CurrentBlock.Load(Out);
	}

	FORCENOINLINE static TOptional<FMemoryView> LoadNextBlock(FMemoryViewReader& In)
	{
		if (In.GetRemainingSize() < sizeof(FBlockHeader))
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("Couldn't read block header"));
			return {};
		}

		if (TOptional<FBlockHeader> Header = LoadBlockHeader(In.Load(sizeof(FBlockHeader))))
		{
			if (Header->Size > In.GetRemainingSize())
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Incomplete block"));
				return {};
			}

			FMemoryView Block = In.Load(Header->Size);
			if (CalculateBlockChecksum(Block) != Header->Checksum)
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Wrong block checksum"));
				return {};
			}

			return Block;
		}

		return {};
	}
};

// Util that maps an entire file
class FMemoryMappedFile
{
public:
	FMemoryMappedFile(const TCHAR* Path)
		: Handle(FPlatformFileManager::Get().GetPlatformFile().OpenMapped(Path))
		, Region(Handle ? Handle->MapRegion() : nullptr)
	{}

	void Preload(int64 Size = MAX_int64) const
	{
		if (Region)
		{
			Region->PreloadHint(0, Size);
		}
	}
	
	FMemoryView View() const
	{
		return Region ? FMemoryView(Region->GetMappedPtr(), Region->GetMappedSize()) : FMemoryView();
	}

private:
	TUniquePtr<IMappedFileHandle> Handle;
	TUniquePtr<IMappedFileRegion> Region;
};

/**
 * Settings about whether to use cache data for the AssetDataGatherer; these settings are shared by
 * FPreloader and the FAssetDataGatherer.
 */
struct FPreloadSettings
{
	void Initialize()
	{
		if (bInitialized)
		{
			return;
		}
		bInitialized = true;

		FString ProjectIntermediateDir = FPaths::ProjectIntermediateDir();
		bForceDependsGathering = FParse::Param(FCommandLine::Get(), TEXT("ForceDependsGathering"));
		bGatherDependsData = (GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("NoDependsGathering"))) || bForceDependsGathering;
		bCacheEnabled = !FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCache")) && !FParse::Param(FCommandLine::Get(), TEXT("multiprocess"));
		bool bAsyncEnabled = FPlatformProcess::SupportsMultithreading() && FTaskGraphInterface::IsRunning();

		MonolithicCacheFilename = FPaths::ProjectIntermediateDir() / (bGatherDependsData ? TEXT("CachedAssetRegistry.bin") : TEXT("CachedAssetRegistryNoDeps.bin"));
#if UE_EDITOR // See note on FPreloader for why we only allow preloading if UE_EDITOR
		if (bCacheEnabled && bAsyncEnabled)
		{
			bPreloadMonolithicCache = GIsEditor && (!IsRunningCommandlet() || IsRunningCookCommandlet());
		}
		else
#endif
		{
			bPreloadMonolithicCache = false;
		}
	}
	bool IsCacheEnabled() const
	{
		return bCacheEnabled;
	}
	bool IsPreloadMonolithicCache() const
	{
		return bPreloadMonolithicCache;
	}
	bool IsGatherDependsData() const
	{
		return bGatherDependsData;
	}
	bool IsForceDependsGathering() const
	{
		return bForceDependsGathering;
	}
	const FString& GetMonolithicCacheFilename() const
	{
		return MonolithicCacheFilename;
	}
private:
	FString MonolithicCacheFilename;
	bool bForceDependsGathering = false;
	bool bGatherDependsData = false;
	bool bCacheEnabled = false;
	bool bPreloadMonolithicCache = false;
	bool bInitialized = false;
};
FPreloadSettings GPreloadSettings;

#if UE_EDITOR
/** A class to preload the monolithic cache used by FAssetDataGatherer. Preloading the cache allows us to
 * start very early in editor startup, so that we have time to finish the cache load before the engine starts
 * making package load requests that need to use the gathered data.
 *
 * In UE_EDITOR, we know the values we need to decide whether we can preload early enough that it is useful to preload
 * In other configurations we do not know those parameters for sure until EDelayedRegisterRunPhase::ShaderTypesReady,
 *  which occurs around the same time as UAssetRegistryImpl is created, so it is not useful to preload.
 */
class FPreloader : public FDelayedAutoRegisterHelper
{
public:
	FPreloader()
		// The callback needs to occur after GIsEditor, ProjectIntermediateDir, IsRunningCommandlet, and
		// IsRunningCookCommandlet have been set
		:FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::IniSystemReady, [this]() { DelayedInitialize(); })
	{
	}

	~FPreloader()
	{
		PreloadReady.Wait();
	}

	FCachePayload Consume()
	{
		check(bInitialized);
		if (bConsumed)
		{
			return FCachePayload();
		}
		bConsumed = true;
		PreloadReady.Wait();
		PreloadReady.Reset();
		return FCachePayload(MoveTemp(Payload));
	}

private:

	void DelayedInitialize()
	{
		GPreloadSettings.Initialize();

		if (GPreloadSettings.IsPreloadMonolithicCache())
		{
			if (IFileManager::Get().FileExists(*GPreloadSettings.GetMonolithicCacheFilename()))
			{
				PreloadReady = Async(EAsyncExecution::TaskGraph, [this]() { LoadAsync(); });
			}
		}
		bInitialized = true;
	}

	void LoadAsync()
	{
		Payload = LoadCacheFile(GPreloadSettings.GetMonolithicCacheFilename());
	}

	TFuture<void> PreloadReady;
	FCachePayload Payload;
	bool bInitialized = false;
	bool bConsumed = false;
};
FPreloader GPreloader;
#endif

} // namespace UE::AssetDataGather::Private

FAssetDataGatherer::FAssetDataGatherer(const TArray<FString>& InLongPackageNamesDenyList, const TArray<FString>& InMountRelativePathsDenyList, bool bInIsSynchronous)
	: Thread(nullptr)
	, bIsSynchronous(bInIsSynchronous)
	, IsStopped(0)
	, IsPaused(0)
	, bInitialPluginsLoaded(false)
	, bSaveAsyncCacheTriggered(false)
	, SearchStartTime(0)
	, LastCacheWriteTime(0.0)
	, bHasLoadedMonolithicCache(false)
	, bDiscoveryIsComplete(false)
	, bIsComplete(false)
	, bIsIdle(false)
	, bFirstTickAfterIdle(true)
	, bFinishedInitialDiscovery(false)
	, WaitBatchCount(0)
	, NumCachedAssetFiles(0)
	, NumUncachedAssetFiles(0)
	, bIsSavingAsyncCache(false)
{
	using namespace UE::AssetDataGather::Private;

	GPreloadSettings.Initialize();
	bGatherAssetPackageData = GIsEditor || GPreloadSettings.IsForceDependsGathering();
	bGatherDependsData = GPreloadSettings.IsGatherDependsData();
	bCacheEnabled = GPreloadSettings.IsCacheEnabled();
	// If PreloadMonolithicCache is true, we are using MonolithicCache. Otherwise it may be set to true later if game/commandlet calls SearchAllAssets.
	bUseMonolithicCache = GPreloadSettings.IsPreloadMonolithicCache();
	LastCacheWriteTime = FPlatformTime::Seconds();

#if !UE_BUILD_SHIPPING
	bool bCommandlineSynchronous;
	if (FParse::Bool(FCommandLine::Get(), TEXT("AssetGatherSync="), bCommandlineSynchronous))
	{
		bIsSynchronous = bCommandlineSynchronous;
	}
#endif
	if (!bIsSynchronous && !FPlatformProcess::SupportsMultithreading())
	{
		bIsSynchronous = true;
		UE_LOG(LogAssetRegistry, Warning, TEXT("Requested asynchronous asset data gather, but threading support is disabled. Performing a synchronous gather instead!"));
	}
	bIsSynchronousTick = bIsSynchronous;

	Discovery = MakeUnique<UE::AssetDataGather::Private::FAssetDataDiscovery>(InLongPackageNamesDenyList, InMountRelativePathsDenyList, bInIsSynchronous);
	FilesToSearch = MakeUnique<UE::AssetDataGather::Private::FFilesToSearch>();
}

FAssetDataGatherer::~FAssetDataGatherer()
{
	EnsureCompletion();
	NewCachedAssetDataMap.Empty();
	DiskCachedAssetDataMap.Empty();

	for (FDiskCachedAssetData* AssetData : NewCachedAssetData)
	{
		delete AssetData;
	}
	NewCachedAssetData.Empty();
	for (TPair<int32, FDiskCachedAssetData*> BlockData : DiskCachedAssetBlocks)
	{
		delete[] BlockData.Get<1>();
	}
	DiskCachedAssetBlocks.Empty();
}

void FAssetDataGatherer::ActivateMonolithicCache()
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	if (!bCacheEnabled || bUseMonolithicCache )
	{
		return;
	}

	bUseMonolithicCache = true;
	LastCacheWriteTime = FPlatformTime::Seconds();
}

void FAssetDataGatherer::StartAsync()
{
	if (!bIsSynchronous && !Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataGatherer"), 0, TPri_BelowNormal);
		checkf(Thread, TEXT("Failed to create asset data gatherer thread"));
		Discovery->StartAsync();
	}
}

bool FAssetDataGatherer::Init()
{
	return true;
}

uint32 FAssetDataGatherer::Run()
{
	constexpr float IdleSleepTime = 0.1f;
	while (!IsStopped)
	{
		InnerTickLoop(false /* bInIsSynchronousTick */, true /* bContributeToCacheSave */);

		for (;;)
		{
			{
				FGathererScopeLock ResultsScopeLock(&ResultsLock); // bIsIdle requires the lock
				if (IsStopped || bSaveAsyncCacheTriggered || (!IsPaused && !bIsIdle))
				{
					break;
				}
			}
			// No work to do. Sleep for a little and try again later.
			// TODO: Need IsPaused to be a condition variable so we avoid sleeping while waiting for it and then taking a long time to wake after it is unset.
			FPlatformProcess::Sleep(IdleSleepTime);
		}
	}
	return 0;
}

void FAssetDataGatherer::InnerTickLoop(bool bInIsSynchronousTick, bool bContributeToCacheSave)
{
	using namespace UE::AssetDataGather::Private;

	// Synchronous ticks during Wait contribute to saving of the async cache only if there is no dedicated async thread to do it (bIsSynchronous is true)
	// The dedicated async thread always contributes
	bContributeToCacheSave = !bInIsSynchronousTick || (bIsSynchronous && bContributeToCacheSave);

	bool bShouldSaveMonolithicCache = false;
	TArray<TPair<FName, FDiskCachedAssetData*>> AssetsToSave;
	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock RunScopeLock(&TickLock);
		TGuardValue<bool> ScopeSynchronousTick(bIsSynchronousTick, bInIsSynchronousTick);
		TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::Tick);
		bool bTickInterruptionEvent = false;
		while (!IsStopped && (bInIsSynchronousTick || !IsPaused) && !bTickInterruptionEvent)
		{
			TickInternal(bTickInterruptionEvent);
		}

		if (bContributeToCacheSave)
		{
			TryReserveSaveMonolithicCache(bShouldSaveMonolithicCache, AssetsToSave);
		}
	}
	if (bShouldSaveMonolithicCache)
	{
		SaveCacheFileInternal(GPreloadSettings.GetMonolithicCacheFilename(), AssetsToSave, true /* bIsAsyncCache */);
	}
}

FAssetDataGatherer::FScopedPause::FScopedPause(const FAssetDataGatherer& InOwner)
	:Owner(InOwner)
{
	if (!Owner.bIsSynchronous)
	{
		Owner.IsPaused++;
	}
}

FAssetDataGatherer::FScopedPause::~FScopedPause()
{
	if (!Owner.bIsSynchronous)
	{
		check(Owner.IsPaused > 0)
		Owner.IsPaused--;
	}
}

void FAssetDataGatherer::Stop()
{
	Discovery->Stop();
	IsStopped++;
}

void FAssetDataGatherer::Exit()
{
}

bool FAssetDataGatherer::IsSynchronous() const
{
	return bIsSynchronous;
}

void FAssetDataGatherer::EnsureCompletion()
{
	Discovery->EnsureCompletion();

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FAssetDataGatherer::TickInternal(bool& bOutIsTickInterrupt)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	using namespace UE::AssetDataGather::Private;

	const int32 BatchSize = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads()) * AssetDataGathererConstants::SingleThreadFilesPerBatch;
	typedef TInlineAllocator<AssetDataGathererConstants::ExpectedMaxBatchSize> FBatchInlineAllocator;

	TArray<FGatheredPathData, FBatchInlineAllocator> LocalFilesToSearch;
	TArray<FAssetData*, FBatchInlineAllocator> LocalAssetResults;
	TArray<FPackageDependencyData, FBatchInlineAllocator> LocalDependencyResults;
	TArray<FString, FBatchInlineAllocator> LocalCookedPackageNamesWithoutAssetDataResults;
	TArray<FName, FBatchInlineAllocator> LocalVerseResults;
	bool bLoadMonolithicCache = false;
	double LocalLastCacheWriteTime = 0.0;
	bool bWaitBatchCountDecremented = false;
	bOutIsTickInterrupt = false;

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);

		if (bFirstTickAfterIdle)
		{
			bFirstTickAfterIdle = false;
			LastCacheWriteTime = FPlatformTime::Seconds();
			SearchStartTime = LastCacheWriteTime;
		}

		IngestDiscoveryResults();

		// Take a batch off of the work list. If we're waiting only on the first WaitBatchCount results don't take more than that
		int32 NumToProcess = FMath::Min<int32>(BatchSize-LocalFilesToSearch.Num(), FilesToSearch->GetNumAvailable());
		if (WaitBatchCount > 0)
		{
			bWaitBatchCountDecremented = true;
			NumToProcess = FMath::Min(NumToProcess, WaitBatchCount);
			WaitBatchCount -= NumToProcess;
			if (WaitBatchCount == 0)
			{
				bOutIsTickInterrupt = true;
			}
		}

		FilesToSearch->PopFront(LocalFilesToSearch, NumToProcess);

		// If no work is available mark idle and exit
		if (LocalFilesToSearch.Num() == 0 && bDiscoveryIsComplete)
		{
			WaitBatchCount = 0; // Clear WaitBatchCount in case it was set higher than FilesToSearch->GetNumAvailable().
			bOutIsTickInterrupt = true;

			if (!bFinishedInitialDiscovery)
			{
				bSaveAsyncCacheTriggered = true;
			}
			SetIsIdle(true);
			return;
		}
		if (bUseMonolithicCache && !bHasLoadedMonolithicCache)
		{
			bLoadMonolithicCache = true;
		}
		LocalLastCacheWriteTime = LastCacheWriteTime;
	}

	// Load the async cache if not yet loaded
	if (bLoadMonolithicCache)
	{
		FCachePayload Payload;
#if UE_EDITOR
		if (GPreloadSettings.IsPreloadMonolithicCache())
		{
			Payload = GPreloader.Consume();
		}
		else
#endif
		{
			Payload = UE::AssetDataGather::Private::LoadCacheFile(GPreloadSettings.GetMonolithicCacheFilename());
		}
		ConsumeCacheFile(MoveTemp(Payload));

		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		bHasLoadedMonolithicCache = true;
	}

	struct FReadContext
	{
		FName PackageName;
		FName Extension;
		FGatheredPathData& AssetFileData;
		TArray<FAssetData*> AssetDataFromFile;
		FPackageDependencyData DependencyData;
		TArray<FString> CookedPackageNamesWithoutAssetData;
		bool bCanAttemptAssetRetry = false;
		bool bResult = false;
		bool bCanceled = false;

		FReadContext(FName InPackageName, FName InExtension, FGatheredPathData& InAssetFileData)
			: PackageName(InPackageName)
			, Extension(InExtension)
			, AssetFileData(InAssetFileData)
		{
		}
	};

	// Try to read each file in the batch out of the cache, and accumulate a list for more expensive reading of all of the files that are not in the cache 
	TArray<FReadContext> ReadContexts;
	for (FGatheredPathData& AssetFileData : LocalFilesToSearch)
	{
		// If this a Verse source file, just directly add its file name to the Verse results
		if (AssetFileData.Type == EGatherableFileType::VerseFile)
		{
			// Store Verse results in a hybrid format using the LongPackageName but keeping the extension
			WriteToString<256> MappedPath(AssetFileData.LongPackageName, FPathViews::GetExtension(AssetFileData.LocalAbsPath, true));
			LocalVerseResults.Add(*MappedPath);
			continue;
		}

		if (AssetFileData.Type != EGatherableFileType::PackageFile)
		{
			ensureMsgf(false, TEXT("Encountered unrecognized gathered asset %s!"), *AssetFileData.LongPackageName);
			continue;
		}

		const FName PackageName = *AssetFileData.LongPackageName;
		const FName Extension = FName(*FPaths::GetExtension(AssetFileData.LocalAbsPath));

		FDiskCachedAssetData** DiskCachedAssetDataPtr = DiskCachedAssetDataMap.Find(PackageName);
		FDiskCachedAssetData* DiskCachedAssetData = DiskCachedAssetDataPtr ? *DiskCachedAssetDataPtr : nullptr;
		if (DiskCachedAssetData)
		{
			// Check whether we need to invalidate the cached data
			const FDateTime& CachedTimestamp = DiskCachedAssetData->Timestamp;
			if (AssetFileData.PackageTimestamp != CachedTimestamp)
			{
				DiskCachedAssetData = nullptr;
			}
			else if ((DiskCachedAssetData->DependencyData.PackageName != PackageName && DiskCachedAssetData->DependencyData.PackageName != NAME_None) ||
				DiskCachedAssetData->Extension != Extension)
			{
				UE_LOG(LogAssetRegistry, Display, TEXT("Cached dependency data for package '%s' is invalid. Discarding cached data."), *PackageName.ToString());
				DiskCachedAssetData = nullptr;
			}
		}

		if (DiskCachedAssetData)
		{
			// Add the valid cached data to our results, and to the map of data we keep to write out the new version of the cache file
			++NumCachedAssetFiles;

			// Set the transient flags based on whether our current cache has dependency data.
			// Note that in editor, bGatherAssetPackageData is always true, no way to turn it off,
			// and in game it is always equal to bGatherDependsData, so it can share the cache with dependency data.
			DiskCachedAssetData->DependencyData.bHasPackageData = bGatherAssetPackageData;
			DiskCachedAssetData->DependencyData.bHasDependencyData = bGatherDependsData;

			LocalAssetResults.Reserve(LocalAssetResults.Num() + DiskCachedAssetData->AssetDataList.Num());
			for (const FAssetData& AssetData : DiskCachedAssetData->AssetDataList)
			{
				LocalAssetResults.Add(new FAssetData(AssetData));
			}

			LocalDependencyResults.Add(DiskCachedAssetData->DependencyData);

			AddToCache(PackageName, DiskCachedAssetData);
		}
		else
		{
			// Not found in cache (or stale) - schedule to be read from disk
			ReadContexts.Emplace(PackageName, Extension, AssetFileData);
		}
	}

	// For all the files not found in the cache, read them from their package files on disk; the file reads are done in parallel
	ParallelFor(ReadContexts.Num(),
		[this, &ReadContexts](int32 Index)
		{
			FReadContext& ReadContext = ReadContexts[Index];
			if (!bIsSynchronousTick && IsPaused)
			{
				ReadContext.bCanceled = true;
				return;
			}
			UE_SCOPED_IO_ACTIVITY(*WriteToString<512>(TEXT("Loading Asset"), ReadContext.PackageName.ToString()));
			ReadContext.bResult = ReadAssetFile(ReadContext.AssetFileData.LongPackageName, ReadContext.AssetFileData.LocalAbsPath, ReadContext.AssetDataFromFile, ReadContext.DependencyData, ReadContext.CookedPackageNamesWithoutAssetData, ReadContext.bCanAttemptAssetRetry);
		},
		EParallelForFlags::Unbalanced | EParallelForFlags::BackgroundPriority
	);

	// Accumulate the results
	bool bHasCancelation = false;
	for (FReadContext& ReadContext : ReadContexts)
	{
		if (ReadContext.bCanceled)
		{
			bHasCancelation = true;
		}
		else if (ReadContext.bResult)
		{
			++NumUncachedAssetFiles;

			// Add the results from a cooked package into our results on cooked package
			LocalCookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(ReadContext.CookedPackageNamesWithoutAssetData));
			// Do not add the results from a cooked package into the map of data we keep to write out the new version of the cache file
			bool bCachePackage = bCacheEnabled
				&& LocalCookedPackageNamesWithoutAssetDataResults.Num() == 0
				&& ensure(ReadContext.AssetFileData.Type == EGatherableFileType::PackageFile);
			if (bCachePackage)
			{
				for (const FAssetData* AssetData : ReadContext.AssetDataFromFile)
				{
					if (!!(AssetData->PackageFlags & PKG_FilterEditorOnly))
					{
						bCachePackage = false;
						break;
					}
				}
			}

			// Add the results from non-cooked packages into the map of data we keep to write out the new version of the cache file 
			if (bCachePackage)
			{
				// Update the cache
				FDiskCachedAssetData* NewData = new FDiskCachedAssetData(ReadContext.AssetFileData.PackageTimestamp, ReadContext.Extension);
				NewData->AssetDataList.Reserve(ReadContext.AssetDataFromFile.Num());
				for (const FAssetData* BackgroundAssetData : ReadContext.AssetDataFromFile)
				{
					NewData->AssetDataList.Add(*BackgroundAssetData);
				}

				// MoveTemp only used if we don't need DependencyData anymore
				NewData->DependencyData = ReadContext.DependencyData;

				NewCachedAssetData.Add(NewData);
				AddToCache(ReadContext.PackageName, NewData);
			}

			// Add the results from the package into our output results
			LocalAssetResults.Append(MoveTemp(ReadContext.AssetDataFromFile));
			LocalDependencyResults.Add(MoveTemp(ReadContext.DependencyData));
		}
		else if (ReadContext.bCanAttemptAssetRetry)
		{
			// If the read temporarily failed, return it to the worklist, pushed to the end
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			FilesToSearch->AddFileForLaterRetry(MoveTemp(ReadContext.AssetFileData));
		}
	}

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);

		// Submit the results into the thread-shared lists
		AssetResults.Append(MoveTemp(LocalAssetResults));
		DependencyResults.Append(MoveTemp(LocalDependencyResults));
		CookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(LocalCookedPackageNamesWithoutAssetDataResults));
		VerseResults.Append(MoveTemp(LocalVerseResults));

		if (bHasCancelation)
		{
			// If we skipped reading files due to a pause request, push the canceled files back onto the FilesToSearch
			for (int Index = ReadContexts.Num() - 1; Index >= 0; --Index) // AddToFront in reverse order so that the elements are readded in the same order they were popped
			{
				FReadContext& ReadContext = ReadContexts[Index];
				if (ReadContext.bCanceled)
				{
					FilesToSearch->AddFileAgainAfterTimeout(MoveTemp(ReadContext.AssetFileData));
					if (bWaitBatchCountDecremented)
					{
						++WaitBatchCount;
					}
				}
			}
		}

		if (bUseMonolithicCache && !bIsSavingAsyncCache 
			&& FPlatformTime::Seconds() - LocalLastCacheWriteTime >= AssetDataGathererConstants::MinSecondsToElapseBeforeCacheWrite)
		{
			bSaveAsyncCacheTriggered = true;
			bOutIsTickInterrupt = true;
		}
	}
}

void FAssetDataGatherer::IngestDiscoveryResults()
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	Discovery->GetAndTrimSearchResults(bDiscoveryIsComplete, DiscoveredPaths, *FilesToSearch, NumPathsToSearchAtLastSyncPoint);
}

bool FAssetDataGatherer::ReadAssetFile(const FString& AssetLongPackageName, const FString& AssetFilename, TArray<FAssetData*>& AssetDataList, FPackageDependencyData& DependencyData, TArray<FString>& CookedPackageNamesWithoutAssetData, bool& OutCanRetry) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::ReadAssetFile);
	OutCanRetry = false;
	AssetDataList.Reset();

	FPackageReader PackageReader;

	FPackageReader::EOpenPackageResult OpenPackageResult;
	if (!PackageReader.OpenPackageFile(AssetLongPackageName, AssetFilename, &OpenPackageResult))
	{
		// If we're missing a custom version, we might be able to load this package later once the module containing that version is loaded...
		//   -	We can only attempt a retry in editors (not commandlets) that haven't yet finished initializing (!GIsRunning), as we 
		//		have no guarantee that a commandlet or an initialized editor is going to load any more modules/plugins
		const bool bAllowRetry = GIsEditor && !bInitialPluginsLoaded;
		if (OpenPackageResult == FPackageReader::EOpenPackageResult::CustomVersionMissing)
		{
			OutCanRetry = bAllowRetry;
			if (!bAllowRetry)
			{
				UE_LOG(LogAssetRegistry, Display, TEXT("Package %s uses an unknown custom version and cannot be loaded for the AssetRegistry"), *AssetFilename);
			}
		}
		else
		{
			OutCanRetry = false;
		}
		return false;
	}
	else
	{
		return ReadAssetFile(PackageReader, AssetDataList, DependencyData, CookedPackageNamesWithoutAssetData,
			(bGatherAssetPackageData ? FPackageReader::EReadOptions::PackageData : FPackageReader::EReadOptions::None) |
			(bGatherDependsData ? FPackageReader::EReadOptions::Dependencies : FPackageReader::EReadOptions::None));
	}
}

bool FAssetDataGatherer::ReadAssetFile(FPackageReader& PackageReader, TArray<FAssetData*>& AssetDataList,
	FPackageDependencyData& DependencyData, TArray<FString>& CookedPackageNamesWithoutAssetData, FPackageReader::EReadOptions Options)
{
	bool bOutIsCookedWithoutAssetData;
	if (!PackageReader.ReadAssetRegistryData(AssetDataList, bOutIsCookedWithoutAssetData))
	{
		return false;
	}
	if (bOutIsCookedWithoutAssetData)
	{
		CookedPackageNamesWithoutAssetData.Add(PackageReader.GetLongPackageName());
	}

	if (!PackageReader.ReadDependencyData(DependencyData, Options))
	{
		return false;
	}

	if (PackageReader.UEVer() >= VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS && PackageReader.UEVer() < VER_UE4_CORRECT_LICENSEE_FLAG)
	{
		if (EnumHasAnyFlags(Options, FPackageReader::EReadOptions::Dependencies))
		{
			// In version VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS, UObjectRedirectors were incorrectly saved as having
			// editor-only imports, since UObjectRedirector is an editor-only class. But UObjectRedirectors are
			// followed during cooking and so their imports should be considered used-in-game. SavePackage was fixed
			// to save them as in-game imports by adding HasNonEditorOnlyReferences; the next version bump after that
			// fix was VER_UE4_CORRECT_LICENSEE_FLAG. Mark all dependencies in the affected version as used in game
			// if the package has a UObjectRedirector object.
			FTopLevelAssetPath RedirectorClassPathName = UObjectRedirector::StaticClass()->GetClassPathName();
			if (Algo::AnyOf(AssetDataList, [RedirectorClassPathName](FAssetData* AssetData) { return AssetData->AssetClassPath == RedirectorClassPathName; }))
			{
				for (FPackageDependencyData::FPackageDependency& Dependency : DependencyData.PackageDependencies)
				{
					Dependency.Property |= UE::AssetRegistry::EDependencyProperty::Game;
				}
			}
		}
	}

	return true;
}

void FAssetDataGatherer::AddToCache(FName PackageName, FDiskCachedAssetData* DiskCachedAssetData)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	FDiskCachedAssetData*& ValueInMap = NewCachedAssetDataMap.FindOrAdd(PackageName, DiskCachedAssetData);
	if (ValueInMap != DiskCachedAssetData)
	{
		// An updated DiskCachedAssetData for the same package; replace the existing DiskCachedAssetData with the new one.
		// Note that memory management of the DiskCachedAssetData is handled in a separate structure; we do not need to delete the old value here.
		if (DiskCachedAssetData->Extension != ValueInMap->Extension)
		{
			// Two files with the same package name but different extensions, e.g. basename.umap and basename.uasset
			// This is invalid - some systems in the engine (Cooker's FPackageNameCache) assume that package : filename is 1 : 1 - so issue a warning
			// Because it is invalid, we don't fully support it here (our map is keyed only by packagename), and will remove from cache all but the last filename we find with the same packagename
			// TODO: Turn this into a warning once all sample projects have fixed it
			UE_LOG(LogAssetRegistry, Display, TEXT("Multiple files exist with the same package name %s but different extensions (%s and %s). ")
				TEXT("This is invalid and will cause errors; merge or rename or delete one of the files."),
				*PackageName.ToString(), *ValueInMap->Extension.ToString(), *DiskCachedAssetData->Extension.ToString());
		}
		ValueInMap = DiskCachedAssetData;
	}
}

void FAssetDataGatherer::GetAndTrimSearchResults(FResults& InOutResults, FResultContext& OutContext)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	auto MoveAppendRangeToRingBuffer = [](auto& InOutRingBuffer, auto& InArray)
	{
		InOutRingBuffer.Reserve(InOutRingBuffer.Num() + InArray.Num());
		for (auto& Element : InArray)
		{
			InOutRingBuffer.Add(MoveTemp(Element));
		}
		InArray.Reset();
	};

	MoveAppendRangeToRingBuffer(InOutResults.Assets, AssetResults);
	MoveAppendRangeToRingBuffer(InOutResults.Paths, DiscoveredPaths);
	MoveAppendRangeToRingBuffer(InOutResults.Dependencies, DependencyResults);
	MoveAppendRangeToRingBuffer(InOutResults.CookedPackageNamesWithoutAssetData, CookedPackageNamesWithoutAssetDataResults);
	MoveAppendRangeToRingBuffer(InOutResults.VerseFiles, VerseResults);

	OutContext.SearchTimes.Append(MoveTemp(SearchTimes));
	SearchTimes.Reset();

	OutContext.NumFilesToSearch = FilesToSearch->Num();
	OutContext.NumPathsToSearch = NumPathsToSearchAtLastSyncPoint;
	OutContext.bIsDiscoveringFiles = !bDiscoveryIsComplete;

	// Idle means no more work OR we are blocked on external events, but complete means no more work period.
	bool bLocalIsComplete = bIsIdle && FilesToSearch->Num() == 0;
	if (bLocalIsComplete && !bIsComplete)
	{
		bIsComplete = true;
		Shrink();
	}
	OutContext.bIsSearching = !bLocalIsComplete;
	OutContext.bAbleToProgress = !bIsIdle;
}

void FAssetDataGatherer::GetPackageResults(TRingBuffer<FAssetData*>& OutAssetResults, TRingBuffer<FPackageDependencyData>& OutDependencyResults)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	auto MoveAppendRangeToRingBuffer = [](auto& OutRingBuffer, auto& InArray)
	{
		OutRingBuffer.Reserve(OutRingBuffer.Num() + InArray.Num());
		for (auto& Element : InArray)
		{
			OutRingBuffer.Add(MoveTemp(Element));
		}
		InArray.Reset();
	};

	MoveAppendRangeToRingBuffer(OutAssetResults, AssetResults);
	MoveAppendRangeToRingBuffer(OutDependencyResults, DependencyResults);
}

void FAssetDataGatherer::WaitOnPath(FStringView InPath)
{
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		if (bIsIdle)
		{
			return;
		}
	}
	FString LocalAbsPath = NormalizeLocalPath(InPath);
	UE::AssetDataGather::Private::FPathExistence QueryPath(LocalAbsPath);
	Discovery->SetPropertiesAndWait(QueryPath, false /* bAddToAllowList */, false /* bForceRescan */, false /* bIgnoreDenyListScanFilters */);
	WaitOnPathsInternal(TArrayView<UE::AssetDataGather::Private::FPathExistence>(&QueryPath, 1), FString(), TArray<FString>());
}

void FAssetDataGatherer::ScanPathsSynchronous(const TArray<FString>& InLocalPaths, bool bForceRescan,
	bool bIgnoreDenyListScanFilters, const FString& SaveCacheFilename, const TArray<FString>& SaveCacheLongPackageNameDirs)
{
	TArray<UE::AssetDataGather::Private::FPathExistence> QueryPaths;
	QueryPaths.Reserve(InLocalPaths.Num());
	for (const FString& LocalPath : InLocalPaths)
	{
		QueryPaths.Add(UE::AssetDataGather::Private::FPathExistence(NormalizeLocalPath(LocalPath)));
	}

	for (UE::AssetDataGather::Private::FPathExistence& QueryPath: QueryPaths)
	{
		Discovery->SetPropertiesAndWait(QueryPath, true /* bAddToAllowList */, bForceRescan, bIgnoreDenyListScanFilters);
	}

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}

	WaitOnPathsInternal(QueryPaths, SaveCacheFilename, SaveCacheLongPackageNameDirs);
}

void FAssetDataGatherer::WaitOnPathsInternal(TArrayView<UE::AssetDataGather::Private::FPathExistence> QueryPaths,
	const FString& SaveCacheFilename, const TArray<FString>& SaveCacheLongPackageNameDirs)
{
	// Request a halt to the async tick
	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	{
		FGathererScopeLock TickScopeLock(&TickLock);

		// Read all results from Discovery into our worklist and then sort our worklist
		{
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			IngestDiscoveryResults();

			int32 NumDiscoveredPaths;
			SortPathsByPriority(QueryPaths, UE::AssetDataGather::Private::EPriority::Blocking, NumDiscoveredPaths);
			if (NumDiscoveredPaths == 0)
			{
				return;
			}
			WaitBatchCount = NumDiscoveredPaths;
		}
	}

	// We do not contribute to the async cache save if we have been given a modular cache to save 
	bool bContributeToCacheSave = SaveCacheFilename.IsEmpty();

	// Tick until NumDiscoveredPaths have been read
	TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::Tick);
	for (;;)
	{
		InnerTickLoop(true /* bInIsSynchronousTick */, bContributeToCacheSave);
		FGathererScopeLock ResultsScopeLock(&ResultsLock); // WaitBatchCount requires the lock
		if (WaitBatchCount == 0)
		{
			break;
		}
	}

	if (!SaveCacheFilename.IsEmpty())
	{
		TArray<TPair<FName, FDiskCachedAssetData*>> AssetsToSave;
		{
			FGathererScopeLock TickScopeLock(&TickLock);
			GetAssetsToSave(SaveCacheLongPackageNameDirs, AssetsToSave);
		}
		SaveCacheFileInternal(SaveCacheFilename, AssetsToSave, false /* bIsAsyncCacheSave */);
	}
}

void FAssetDataGatherer::WaitForIdle()
{
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		if (bIsIdle)
		{
			return;
		}
	}
	Discovery->WaitForIdle();
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);

	// Request a halt to the async tick
	FScopedPause ScopedPause(*this);
	// Tick until idle
	for (;;)
	{
		InnerTickLoop(true /* bInIsSynchronousTick */, true /* bContributeToCacheSave */);
		FGathererScopeLock ResultsScopeLock(&ResultsLock); // bIsIdle requires the lock
		if (bIsIdle)
		{
			// We need to break out of WaitForIdle whenever it requires main thread action to proceed,
			// so we check bIsIdle rather than whether we're complete
			break;
		}
	}
}

bool FAssetDataGatherer::IsComplete() const
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	return bIsComplete;
}

void FAssetDataGatherer::SetInitialPluginsLoaded()
{
	bInitialPluginsLoaded = true;
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	SetIsIdle(false);
	FilesToSearch->RetryLaterRetryFiles();
}

bool FAssetDataGatherer::IsGatheringDependencies() const
{
	return bGatherDependsData;
}

bool FAssetDataGatherer::IsCacheEnabled() const
{
	return bCacheEnabled;
}

FString FAssetDataGatherer::GetCacheFilename(TConstArrayView<FString> CacheFilePackagePaths)
{
	// Try and build a consistent hash for this input
	// Normalize the paths; removing any trailing /
	TArray<FString> SortedPaths(CacheFilePackagePaths);
	for (FString& PackagePath : SortedPaths)
	{
		while (PackagePath.Len() > 1 && PackagePath.EndsWith(TEXT("/")))
		{
			PackagePath.LeftChopInline(1);
		}
	}

	// Sort the paths
	SortedPaths.StableSort();

	// todo: handle hash collisions?
	uint32 CacheHash = SortedPaths.Num() > 0 ? GetTypeHash(SortedPaths[0]) : 0;
	for (int32 PathIndex = 1; PathIndex < SortedPaths.Num(); ++PathIndex)
	{
		CacheHash = HashCombine(CacheHash, GetTypeHash(SortedPaths[PathIndex]));
	}

	return FPaths::ProjectIntermediateDir() / TEXT("AssetRegistryCache") / FString::Printf(TEXT("%08x%s.bin"), CacheHash, bGatherDependsData ? TEXT("") : TEXT("NoDeps"));
}

void FAssetDataGatherer::LoadCacheFile(FStringView CacheFilename)
{
	using namespace UE::AssetDataGather::Private;
	if (!bCacheEnabled)
	{
		return;
	}

	FCachePayload Payload = UE::AssetDataGather::Private::LoadCacheFile(CacheFilename);
	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	ConsumeCacheFile(MoveTemp(Payload));
}

void FAssetDataGatherer::ConsumeCacheFile(UE::AssetDataGather::Private::FCachePayload&& Payload)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	if (!Payload.bSucceeded || Payload.NumAssets == 0)
	{
		return;
	}

	DiskCachedAssetDataMap.Reserve(DiskCachedAssetDataMap.Num() + Payload.NumAssets);
	for (int32 AssetIndex = 0; AssetIndex < Payload.NumAssets; ++AssetIndex)
	{
		DiskCachedAssetDataMap.Add(*(Payload.PackageNames.Get() + AssetIndex),
			(Payload.AssetDatas.Get() + AssetIndex)); // -C6385
	}
	DiskCachedAssetBlocks.Emplace(Payload.NumAssets, Payload.AssetDatas.Release());
	Payload.Reset();

	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	DependencyResults.Reserve(DiskCachedAssetDataMap.Num());
	AssetResults.Reserve(DiskCachedAssetDataMap.Num());
}

void FAssetDataGatherer::TryReserveSaveMonolithicCache(bool& bOutShouldSave, TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
	bOutShouldSave = false;
	if (IsStopped)
	{
		return;
	}
	if (!bSaveAsyncCacheTriggered || bIsSavingAsyncCache)
	{
		return;
	}
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		bOutShouldSave = bUseMonolithicCache;
	}
	if (bOutShouldSave)
	{
		GetAssetsToSave(TArrayView<const FString>(), AssetsToSave);
		bIsSavingAsyncCache = true;
	}
	bSaveAsyncCacheTriggered = false;
}

void FAssetDataGatherer::GetAssetsToSave(TArrayView<const FString> SaveCacheLongPackageNameDirs, TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);

	OutAssetsToSave.Reset();
	if (SaveCacheLongPackageNameDirs.IsEmpty())
	{
		OutAssetsToSave.Reserve(NewCachedAssetDataMap.Num());
		for (const TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
		{
			OutAssetsToSave.Add(Pair);
		}
	}
	else
	{
		for (const TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
		{
			TStringBuilder<128> PackageNameStr;
			Pair.Key.ToString(PackageNameStr);
			if (Algo::AnyOf(SaveCacheLongPackageNameDirs, [PackageNameSV = FStringView(PackageNameStr)](const FString& SaveCacheLongPackageNameDir)
			{
				return FPathViews::IsParentPathOf(SaveCacheLongPackageNameDir, PackageNameSV);
			}))
			{
				OutAssetsToSave.Add(Pair);
			}
		}
	}
}

void FAssetDataGatherer::SaveCacheFileInternal(const FString& CacheFilename, const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave, bool bIsAsyncCacheSave)
{
	if (CacheFilename.IsEmpty() || !bCacheEnabled)
	{
		return;
	}
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	TRACE_CPUPROFILER_EVENT_SCOPE(SaveCacheFile);
	// Save to a temp file first, then move to the destination to avoid corruption
	FString CacheFilenameStr(CacheFilename);
	FString TempFilename = CacheFilenameStr + TEXT(".tmp");
	TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(*TempFilename, 0));
	if (FileAr)
	{
		uint64 CurrentVersion = AssetDataGathererConstants::CurrentVersion;
		*FileAr << CurrentVersion;

#if ALLOW_NAME_BATCH_SAVING
		{
			// We might be able to reduce load time by using AssetRegistry::SerializationOptions
			// to save certain common tags as FName.
			UE::AssetDataGather::Private::FChecksumArchiveWriter ChecksummingWriter(*FileAr);
			FAssetRegistryWriter Ar(FAssetRegistryWriterOptions(), ChecksummingWriter);
			UE::AssetDataGather::Private::SerializeCacheSave(Ar, AssetsToSave);
		}
#else		
		checkf(false, TEXT("Cannot save asset registry cache in this configuration"));
#endif
		// Close file handle before moving temp file to target 
		FileAr.Reset();
		IFileManager::Get().Move(*CacheFilenameStr, *TempFilename);
	}
	else
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("Failed to open file for write %s"), *TempFilename);
	}

	if (bIsAsyncCacheSave)
	{
		FScopedPause ScopedPause(*this);
		FGathererScopeLock TickScopeLock(&TickLock);
		bIsSavingAsyncCache = false;
		LastCacheWriteTime = FPlatformTime::Seconds();
	}
}

namespace UE::AssetDataGather::Private
{

void SerializeCacheSave(FAssetRegistryWriter& Ar, const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
#if ALLOW_NAME_BATCH_SAVING
	double SerializeStartTime = FPlatformTime::Seconds();

	// serialize number of objects
	int32 LocalNumAssets = AssetsToSave.Num();
	Ar << LocalNumAssets;

	for (const TPair<FName, FDiskCachedAssetData*>& Pair : AssetsToSave)
	{
		FName AssetName = Pair.Key;
		Ar << AssetName;
		Pair.Value->SerializeForCache(Ar);
	}

	UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
#endif
}

FCachePayload SerializeCacheLoad(FAssetRegistryReader& Ar)
{
	double SerializeStartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
	};

	// serialize number of objects
	int32 LocalNumAssets = 0;
	Ar << LocalNumAssets;

	const int32 MinAssetEntrySize = sizeof(int32);
	const int64 MaxPossibleNumAssets = (Ar.TotalSize() - Ar.Tell()) / MinAssetEntrySize;
	if (Ar.IsError() || LocalNumAssets < 0 || MaxPossibleNumAssets < LocalNumAssets)
	{
		Ar.SetError();
		return FCachePayload();
	}

	if (LocalNumAssets == 0)
	{
		FCachePayload Payload = FCachePayload();
		Payload.bSucceeded = true;
		return Payload;
	}

	FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::NonPackage, ESoftObjectPathSerializeType::AlwaysSerialize);

	// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
	TUniquePtr<FName[]> PackageNameBlock(new FName[LocalNumAssets]);
	TUniquePtr<FDiskCachedAssetData[]> AssetDataBlock(new FDiskCachedAssetData[LocalNumAssets]);
	for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; ++AssetIndex)
	{
		// Load the name first to add the entry to the tmap below
		// Visual Studio Static Analyzer issues C6385 if we call Ar << PackageNameBlock[AssetIndex] or AssetDataBlock[AssetIndex].SerializeForCache
		Ar << *(PackageNameBlock.Get() + AssetIndex); // -C6385
		(AssetDataBlock.Get() + AssetIndex)->SerializeForCache(Ar); // -C6385
		if (Ar.IsError())
		{
			// There was an error reading the cache. Bail out.
			break;
		}
	}

	if (Ar.IsError())
	{
		return FCachePayload();
	}
	FCachePayload Result;
	Result.PackageNames = MoveTemp(PackageNameBlock);
	Result.AssetDatas = MoveTemp(AssetDataBlock);
	Result.NumAssets = LocalNumAssets;
	Result.bSucceeded = true;
	return Result;
}

FCachePayload LoadCacheFile(FStringView InCacheFilename)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadCacheFile);
	FString CacheFilename(InCacheFilename);

	auto DoLoad = [&](FArchive& ChecksummingReader)
	{
		int32 WorkerReduction = 2; // Current worker + preload task
		int32 Parallelism = FMath::Max(FTaskGraphInterface::Get().GetNumWorkerThreads() - WorkerReduction, 0);
		
		// The discovery cache is always serialized with a fixed format.
		// We discard it before this point if it's not the latest version, and it always includes editor-only data.
		FAssetRegistryHeader Header;
		Header.Version = FAssetRegistryVersion::LatestVersion;
		Header.bFilterEditorOnlyData = false;
		FAssetRegistryReader RegistryReader(ChecksummingReader, Parallelism, Header);
		return RegistryReader.IsError() ? FCachePayload() : SerializeCacheLoad(RegistryReader);
	};

	FCachePayload Payload;
	if (FPlatformProperties::SupportsMemoryMappedFiles())
	{
		FMemoryMappedFile File(*CacheFilename);
		UE::Tasks::FTask Preload = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]() { File.Preload(); });

		FMemoryViewReader FileReader(File.View());
		TOptional<uint64> Version = FileReader.TryLoad<uint64>();
		if (Version == AssetDataGathererConstants::CurrentVersion)
		{
			FChecksumViewReader ChecksummingReader(MoveTemp(FileReader), CacheFilename);
			Payload = DoLoad(ChecksummingReader);
			UE_CLOG(!Payload.bSucceeded, LogAssetRegistry, Error, TEXT("There was an error loading the asset registry cache using memory mapping"));
		}

		Preload.Wait();
		UE::Tasks::Launch(UE_SOURCE_LOCATION, [KillAsync = MoveTemp(File)](){});
	}
	else
	{
		TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileReader(*CacheFilename, FILEREAD_Silent));
		if (FileAr && !FileAr->IsError() && FileAr->TotalSize() > sizeof(uint64))
		{
			uint64 Version = 0;
			*FileAr << Version;
			if (Version == AssetDataGathererConstants::CurrentVersion)
			{
				FChecksumArchiveReader ChecksummingReader(*FileAr);
				Payload = DoLoad(ChecksummingReader);
				UE_CLOG(!Payload.bSucceeded, LogAssetRegistry, Error, TEXT("There was an error loading the asset registry cache"));
			}
		}
	}
	
	return Payload;
}

}

SIZE_T FAssetDataGatherer::GetAllocatedSize() const
{
	using namespace UE::AssetDataGather::Private;
	auto GetArrayRecursiveAllocatedSize = [](auto Container)
	{
		SIZE_T Result = Container.GetAllocatedSize();
		for (const auto& Value : Container)
		{
			Result += Value.GetAllocatedSize();
		}
		return Result;
	};

	SIZE_T Result = 0;
	if (Thread)
	{
		// TODO: Add size of Thread->GetAllocatedSize()
		Result += sizeof(*Thread);
	}

	Result += sizeof(*Discovery) + Discovery->GetAllocatedSize();

	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	Result += sizeof(*FilesToSearch) + FilesToSearch->GetAllocatedSize();

	Result += AssetResults.GetAllocatedSize();
	FAssetDataTagMapSharedView::FMemoryCounter TagMemoryUsage;
	for (FAssetData* Value : AssetResults)
	{
		Result += sizeof(*Value);
		TagMemoryUsage.Include(Value->TagsAndValues);
	}
	Result += FAssetData::GetChunkArrayRegistryAllocatedSize();
	Result += TagMemoryUsage.GetFixedSize() + TagMemoryUsage.GetLooseSize();

	Result += GetArrayRecursiveAllocatedSize(DependencyResults);
	Result += GetArrayRecursiveAllocatedSize(CookedPackageNamesWithoutAssetDataResults);
	Result += VerseResults.GetAllocatedSize();
	Result += SearchTimes.GetAllocatedSize();
	Result += GetArrayRecursiveAllocatedSize(DiscoveredPaths);
	Result += GPreloadSettings.GetMonolithicCacheFilename().GetAllocatedSize();

	Result += NewCachedAssetData.GetAllocatedSize();
	for (const FDiskCachedAssetData* Value : NewCachedAssetData)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += DiskCachedAssetBlocks.GetAllocatedSize();
	for (const TPair<int32, FDiskCachedAssetData*>& ArrayData : DiskCachedAssetBlocks)
	{
		Result += ArrayData.Get<0>() * sizeof(FDiskCachedAssetData);
	}
	Result += DiskCachedAssetDataMap.GetAllocatedSize();
	Result += NewCachedAssetDataMap.GetAllocatedSize();

	return Result;
}

void FAssetDataGatherer::Shrink()
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	FilesToSearch->Shrink();
	AssetResults.Shrink();
	DependencyResults.Shrink();
	CookedPackageNamesWithoutAssetDataResults.Shrink();
	VerseResults.Shrink();
	SearchTimes.Shrink();
	DiscoveredPaths.Shrink();
}

void FAssetDataGatherer::AddMountPoint(FStringView LocalPath, FStringView LongPackageName)
{
	Discovery->AddMountPoint(NormalizeLocalPath(LocalPath), NormalizeLongPackageName(LongPackageName));
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::RemoveMountPoint(FStringView LocalPath)
{
	Discovery->RemoveMountPoint(NormalizeLocalPath(LocalPath));
}

void FAssetDataGatherer::AddRequiredMountPoints(TArrayView<FString> LocalPaths)
{
	TStringBuilder<128> MountPackageName;
	TStringBuilder<128> MountFilePath;
	TStringBuilder<128> RelPath;
	for (const FString& LocalPath : LocalPaths)
	{
		if (FPackageName::TryGetMountPointForPath(LocalPath, MountPackageName, MountFilePath, RelPath))
		{
			Discovery->AddMountPoint(NormalizeLocalPath(MountFilePath), NormalizeLongPackageName(MountPackageName));
		}
	}
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::OnDirectoryCreated(FStringView LocalPath)
{
	Discovery->OnDirectoryCreated(NormalizeLocalPath(LocalPath));
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::OnFilesCreated(TConstArrayView<FString> LocalPaths)
{
	TArray<FString> LocalAbsPaths;
	LocalAbsPaths.Reserve(LocalPaths.Num());
	for (const FString& LocalPath : LocalPaths)
	{
		LocalAbsPaths.Add(NormalizeLocalPath(LocalPath));
	}
	Discovery->OnFilesCreated(LocalAbsPaths);
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	using namespace UE::AssetDataGather::Private;

	FString LocalFilenamePathToPrioritize;
	if (FPackageName::TryConvertLongPackageNameToFilename(PathToPrioritize, LocalFilenamePathToPrioritize))
	{
		FSetPathProperties Properties;
		Properties.Priority = EPriority::High;
		SetDirectoryProperties(LocalFilenamePathToPrioritize, Properties);
	}
}

void FAssetDataGatherer::SetDirectoryProperties(FStringView LocalPath, const UE::AssetDataGather::Private::FSetPathProperties& InProperties)
{
	FString LocalAbsPath = NormalizeLocalPath(LocalPath);
	if (LocalAbsPath.Len() == 0)
	{
		return;
	}

	if (!Discovery->TrySetDirectoryProperties(LocalAbsPath, InProperties, false))
	{
		return;
	}

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
		if (InProperties.Priority.IsSet())
		{
			int32 NumPrioritizedPaths;
			UE::AssetDataGather::Private::FPathExistence QueryPath(LocalAbsPath);
			SortPathsByPriority(TArrayView<UE::AssetDataGather::Private::FPathExistence>(&QueryPath, 1),
				*InProperties.Priority, NumPrioritizedPaths);
		}
	}
}

void FAssetDataGatherer::SortPathsByPriority(
	TArrayView<UE::AssetDataGather::Private::FPathExistence> LocalAbsPathsToPrioritize,
	UE::AssetDataGather::Private::EPriority Priority, int32& OutNumPaths)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	using namespace UE::AssetDataGather::Private;

	for (UE::AssetDataGather::Private::FPathExistence& QueryPath : LocalAbsPathsToPrioritize)
	{
		switch (QueryPath.GetType())
		{
		case FPathExistence::EType::Directory:
			FilesToSearch->PrioritizeDirectory(QueryPath.GetLocalAbsPath(), Priority);
			break;
		case FPathExistence::EType::File:
			// Intentional fall-through
		case FPathExistence::EType::MissingButDirExists:
			// We assume MissingButDirExists is a file missing the extension;
			// PrioritizeFile handles searching for BaseNameWithPath.* and does nothing if no matches are found
			FilesToSearch->PrioritizeFile(QueryPath.GetLocalAbsPath(), Priority);
			break;
		default:
			break;
		}
	}
	OutNumPaths = FilesToSearch->NumBlockingFiles();
}

void FAssetDataGatherer::SetIsOnAllowList(FStringView LocalPath, bool bIsAllowed)
{
	using namespace UE::AssetDataGather::Private;

	FSetPathProperties Properties;
	Properties.IsOnAllowList = bIsAllowed;
	SetDirectoryProperties(LocalPath, Properties);
}

bool FAssetDataGatherer::IsOnAllowList(FStringView LocalPath) const
{
	return Discovery->IsOnAllowList(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsOnDenyList(FStringView LocalPath) const
{
	return Discovery->IsOnDenyList(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsMonitored(FStringView LocalPath) const
{
	return Discovery->IsMonitored(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsVerseFile(FStringView FilePath)
{
	return FilePath.EndsWith(TEXT(".verse")) || FilePath.EndsWith(TEXT(".vmodule"));
}

void FAssetDataGatherer::SetIsIdle(bool bInIsIdle)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	if (bInIsIdle == bIsIdle)
	{
		return;
	}

	bIsIdle = bInIsIdle;
	if (bIsIdle)
	{
		// bIsComplete will be set in GetAndTrimSearchResults
		double SearchTime = FPlatformTime::Seconds() - SearchStartTime;
		if (!bFinishedInitialDiscovery)
		{
			bFinishedInitialDiscovery = true;

			UE_LOG(LogAssetRegistry, Verbose, TEXT("Initial scan took %0.6f seconds (found %d cached assets, and loaded %d)"), (float)SearchTime, NumCachedAssetFiles, NumUncachedAssetFiles);
		}
		SearchTimes.Add(SearchTime);
	}
	else
	{
		bIsComplete = false;
		bDiscoveryIsComplete = false;
		bFirstTickAfterIdle = true;
	}
}

FString FAssetDataGatherer::NormalizeLocalPath(FStringView LocalPath)
{
	FString LocalAbsPath(LocalPath);
	LocalAbsPath = FPaths::ConvertRelativePathToFull(MoveTemp(LocalAbsPath));
	return LocalAbsPath;
}

FStringView FAssetDataGatherer::NormalizeLongPackageName(FStringView LongPackageName)
{
	// Conform LongPackageName to our internal format, which does not have a terminating redundant /
	if (LongPackageName.EndsWith(TEXT('/')))
	{
		LongPackageName = LongPackageName.LeftChop(1);
	}
	return LongPackageName;
}

namespace UE::AssetDataGather::Private
{

void FFilesToSearch::AddPriorityFile(FGatheredPathData&& FilePath)
{
	BlockingFiles.Add(MoveTemp(FilePath));
}

void FFilesToSearch::AddDirectory(FString&& DirAbsPath, TArray<FGatheredPathData>&& FilePaths)
{
	if (FilePaths.Num() == 0)
	{
		return;
	}
	check(!DirAbsPath.IsEmpty());

	FTreeNode& Node = Root.FindOrAddNode(DirAbsPath);
	Node.AddFiles(MoveTemp(FilePaths));
}

void FFilesToSearch::AddFileAgainAfterTimeout(FGatheredPathData&& FilePath)
{
	BlockingFiles.AddFront(MoveTemp(FilePath));
}

void FFilesToSearch::AddFileForLaterRetry(FGatheredPathData&& FilePath)
{
	LaterRetryFiles.Add(FilePath);
}

void FFilesToSearch::RetryLaterRetryFiles()
{
	while (!LaterRetryFiles.IsEmpty())
	{
		FGatheredPathData FilePath = LaterRetryFiles.PopFrontValue();
		FTreeNode& Node = Root.FindOrAddNode(FPathViews::GetPath(FilePath.LocalAbsPath));
		Node.AddFile(MoveTemp(FilePath));
	}
}

template <typename AllocatorType>
void FFilesToSearch::PopFront(TArray<FGatheredPathData, AllocatorType>& Out, int32 NumToPop)
{
	while (NumToPop > 0 && !BlockingFiles.IsEmpty())
	{
		Out.Add(BlockingFiles.PopFrontValue());
		--NumToPop;
	}
	Root.PopFiles(Out, NumToPop);
}

void FFilesToSearch::PrioritizeDirectory(FStringView DirAbsPath, EPriority Priority)
{
	// We may need to prioritize a LaterRetryFile that is now loadable, so add them all into the Root
	RetryLaterRetryFiles();

	if (Priority > EPriority::Blocking)
	{
		// TODO: Implement another tree that is searched first for the High Priority 
		// We cannot add the High Priority files to the BlockingFiles array, because
		// then blocking on BlockingFiles to be empty could be slow. We cannot add them
		// as a separate simple array, because we would have to search that (sometimes large)
		// array linearly when looking for files to accomodate a blocking priority request
		return;
	}
	FTreeNode* TreeNode = Root.FindNode(DirAbsPath);
	if (TreeNode)
	{
		TreeNode->PopAllFiles(BlockingFiles);
	}
}

void FFilesToSearch::PrioritizeFile(FStringView FileAbsPathExtOptional, EPriority Priority)
{
	// We may need to prioritize a LaterRetryFile that is now loadable, so add them all into the Root
	RetryLaterRetryFiles();

	if (Priority > EPriority::Blocking)
	{
		// TODO: Implement High Priority; see note in PrioritizeDirectory
		return;
	}
	FTreeNode* TreeNode = Root.FindNode(FPathViews::GetPath(FileAbsPathExtOptional));
	if (TreeNode)
	{
		int32 BeforeSize = BlockingFiles.Num();
		TreeNode->PopMatchingDirectFiles(BlockingFiles, FileAbsPathExtOptional);
	}
}

int32 FFilesToSearch::NumBlockingFiles() const
{
	return BlockingFiles.Num();
}

void FFilesToSearch::Shrink()
{
	// TODO: Make TRingBuffer::Shrink
	TRingBuffer<UE::AssetDataGather::Private::FGatheredPathData> Buffer;
	Buffer.Reserve(BlockingFiles.Num());
	for (UE::AssetDataGather::Private::FGatheredPathData& File : BlockingFiles)
	{
		Buffer.Add(MoveTemp(File));
	}
	Swap(Buffer, BlockingFiles);

	Buffer.Empty(LaterRetryFiles.Num());
	for (UE::AssetDataGather::Private::FGatheredPathData& File : LaterRetryFiles)
	{
		Buffer.Add(MoveTemp(File));
	}
	Swap(Buffer, LaterRetryFiles);

	Root.Shrink();
}

int32 FFilesToSearch::Num() const
{
	return BlockingFiles.Num() + Root.NumFiles() + LaterRetryFiles.Num();
}

int32 FFilesToSearch::GetNumAvailable() const
{
	return BlockingFiles.Num() + Root.NumFiles();
}

SIZE_T FFilesToSearch::GetAllocatedSize() const
{
	SIZE_T Size = 0;
	Size += BlockingFiles.GetAllocatedSize();
	for (const FGatheredPathData& PathData : BlockingFiles)
	{
		Size += PathData.GetAllocatedSize();
	}
	Size += Root.GetAllocatedSize();
	Size += LaterRetryFiles.GetAllocatedSize();
	for (const FGatheredPathData& PathData : LaterRetryFiles)
	{
		Size += PathData.GetAllocatedSize();
	}
	return Size;
}

FFilesToSearch::FTreeNode::FTreeNode(FStringView InRelPath)
	:RelPath(InRelPath)
{
}

FStringView FFilesToSearch::FTreeNode::GetRelPath() const
{
	return RelPath;
}

FFilesToSearch::FTreeNode& FFilesToSearch::FTreeNode::FindOrAddNode(FStringView InRelPath)
{
	if (InRelPath.IsEmpty())
	{
		return *this;
	}
	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	FTreeNode& SubDir = FindOrAddSubDir(FirstComponent);
	return SubDir.FindOrAddNode(RemainingPath);
}

FFilesToSearch::FTreeNode* FFilesToSearch::FTreeNode::FindNode(FStringView InRelPath)
{
	if (InRelPath.IsEmpty())
	{
		return this;
	}
	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	FTreeNode* SubDir = FindSubDir(FirstComponent);
	if (!SubDir)
	{
		return nullptr;
	}

	return SubDir->FindNode(RemainingPath);
}

FFilesToSearch::FTreeNode& FFilesToSearch::FTreeNode::FindOrAddSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return *SubDirs.EmplaceAt_GetRef(Index, MakeUnique<FTreeNode>(SubDirBaseName));
	}
	else
	{
		return *SubDirs[Index];
	}
}

FFilesToSearch::FTreeNode* FFilesToSearch::FTreeNode::FindSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return nullptr;
	}
	else
	{
		return SubDirs[Index].Get();
	}
}

int32 FFilesToSearch::FTreeNode::FindLowerBoundSubDir(FStringView SubDirBaseName)
{
	return Algo::LowerBound(SubDirs, SubDirBaseName,
		[](const TUniquePtr<FTreeNode>& SubDir, FStringView BaseName)
		{
			return FPathViews::Less(SubDir->RelPath, BaseName);
		}
	);
}

void FFilesToSearch::FTreeNode::AddFiles(TArray<FGatheredPathData>&& FilePaths)
{
	if (Files.Num() == 0)
	{
		Files = MoveTemp(FilePaths);
	}
	else
	{
		Files.Append(MoveTemp(FilePaths));
	}
}

void FFilesToSearch::FTreeNode::AddFile(FGatheredPathData&& FilePath)
{
	Files.Add(MoveTemp(FilePath));
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopFiles(RangeType& Out, int32& NumToPop)
{
	while (NumToPop > 0 && !Files.IsEmpty())
	{
		Out.Add(Files.Pop(false /* bAllowShrinking */));
		--NumToPop;
	}
	while (NumToPop > 0 && SubDirs.Num() != 0)
	{
		TUniquePtr<FTreeNode>& SubDir = SubDirs[SubDirs.Num() - 1];
		SubDir->PopFiles(Out, NumToPop);
		if (SubDir->IsEmpty())
		{
			SubDirs.RemoveAt(SubDirs.Num() - 1);
		}
	}
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopAllFiles(RangeType& Out)
{
	while (!Files.IsEmpty())
	{
		Out.Add(Files.Pop(false /* bAllowShrinking */));
	}
	for (int32 Index = SubDirs.Num() - 1; Index >= 0; --Index) // Match the order of PopFiles
	{
		TUniquePtr<FTreeNode>& SubDir = SubDirs[Index];
		SubDir->PopAllFiles(Out);
		SubDir.Reset();
	}
	SubDirs.Empty();
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopMatchingDirectFiles(RangeType& Out, FStringView FileAbsPathExtOptional)
{
	// TODO: Make this more performant by sorting the list of Files.
	// To prevent shifting costs, when popping the file we will leave a placeholder behind with an ignore flag set.
	FStringView FileAbsPathNoExt = FPathViews::GetBaseFilenameWithPath(FileAbsPathExtOptional);
	int32 NumFiles = Files.Num();
	for (int32 Index = 0; Index < NumFiles; )
	{
		FGatheredPathData& PathData = Files[Index];
		if (FPathViews::Equals(FPathViews::GetBaseFilenameWithPath(PathData.LocalAbsPath), FileAbsPathNoExt))
		{
			Out.Add(MoveTemp(PathData));
			Files.RemoveAt(Index);
			--NumFiles;
		}
		else
		{
			++Index;
		}
	}
}

void FFilesToSearch::FTreeNode::PruneEmptyChild(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (!(Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName)))
	{
		if (SubDirs[Index]->IsEmpty())
		{
			SubDirs.RemoveAt(Index);
		}
	}
}

bool FFilesToSearch::FTreeNode::IsEmpty() const
{
	return Files.Num() == 0 && SubDirs.Num() == 0;
}

void FFilesToSearch::FTreeNode::Shrink()
{
	Files.Shrink();
	SubDirs.Shrink();
	for (TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		SubDir->Shrink();
	}
}

SIZE_T FFilesToSearch::FTreeNode::GetAllocatedSize() const
{
	SIZE_T Size = 0;
	Size = Files.GetAllocatedSize();
	for (const FGatheredPathData& File : Files)
	{
		Size += File.GetAllocatedSize();
	}
	Size += SubDirs.GetAllocatedSize() + SubDirs.Num()*sizeof(FTreeNode);
	for (const TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		Size += SubDir->GetAllocatedSize();
	}
	return Size;
}

int32 FFilesToSearch::FTreeNode::NumFiles() const
{
	int32 Num = Files.Num();
	for (const TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		Num += SubDir->NumFiles();
	}
	return Num;
}

}
