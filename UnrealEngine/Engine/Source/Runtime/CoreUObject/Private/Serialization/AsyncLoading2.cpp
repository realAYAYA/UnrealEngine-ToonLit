// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#include "Serialization/AsyncLoading2.h"
#include "Algo/AnyOf.h"
#include "Algo/Partition.h"
#include "IO/IoDispatcher.h"
#include "Serialization/AsyncPackageLoader.h"
#include "IO/PackageStore.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/Event.h"
#include "HAL/ThreadManager.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreStats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/StringBuilder.h"
#include "UObject/ObjectResource.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/UObjectGlobalsInternal.h"
#include "Serialization/DeferredMessageLog.h"
#include "UObject/UObjectThreadContext.h"
#include "Misc/Paths.h"
#include "Misc/PlayInEditorLoadingScope.h"
#include "Misc/ExclusiveLoadPackageTimeTracker.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ExceptionHandling.h"
#include "UObject/UObjectHash.h"
#include "Templates/Casts.h"
#include "Templates/UniquePtr.h"
#include "Serialization/BufferReader.h"
#include "Async/TaskGraphInterfaces.h"
#include "Blueprint/BlueprintSupport.h"
#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "UObject/UObjectArchetypeInternal.h"
#include "Misc/MTAccessDetector.h"
#include "UObject/GarbageCollectionInternal.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Serialization/LoadTimeTracePrivate.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/AsyncPackage.h"
#include "Serialization/UnversionedPropertySerialization.h"
#include "Serialization/Zenaphore.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectRedirector.h"
#include "Serialization/BulkData.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectClusters.h"
#include "UObject/LinkerInstancingContext.h"
#include "UObject/LinkerLoadImportBehavior.h"
#include "UObject/ObjectSerializeAccessScope.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Async/ManualResetEvent.h"
#include "HAL/LowLevelMemStats.h"
#include "HAL/IPlatformFileOpenLogWrapper.h"
#include "Modules/ModuleManager.h"
#include "Containers/MpscQueue.h"
#include "Containers/SpscQueue.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/PathViews.h"
#include "UObject/LinkerLoad.h"
#include "Containers/SpscQueue.h"
#include "IO/IoPriorityQueue.h"
#include "UObject/CoreRedirects.h"
#include "Serialization/ZenPackageHeader.h"
#include "Trace/Trace.h"

#include <atomic>

// For now, the partial request behavior is reserved for the editor only.
#define WITH_PARTIAL_REQUEST_DURING_RECURSION WITH_EDITOR

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, FileIO);
CSV_DEFINE_STAT(FileIO, FrameCompletedExportBundleLoadsKB);

FArchive& operator<<(FArchive& Ar, FZenPackageVersioningInfo& VersioningInfo)
{
	Ar << VersioningInfo.ZenVersion;
	Ar << VersioningInfo.PackageVersion;
	Ar << VersioningInfo.LicenseeVersion;
	VersioningInfo.CustomVersions.Serialize(Ar);
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FZenPackageImportedPackageNamesContainer& Container)
{
	TArray<FDisplayNameEntryId> NameEntries;
	if (Ar.IsSaving())
	{
#if ALLOW_NAME_BATCH_SAVING
		NameEntries.Reserve(Container.Names.Num());
		for (FName ImportedPackageName : Container.Names)
		{
			NameEntries.Emplace(ImportedPackageName);
		}
		SaveNameBatch(NameEntries, Ar);
		for (FName ImportedPackageName : Container.Names)
		{
			int32 Number = ImportedPackageName.GetNumber();
			Ar << Number;
		}
#else
		check(false);
#endif
	}
	else
	{
		NameEntries = LoadNameBatch(Ar);
		Container.Names.SetNum(NameEntries.Num());
		for (int32 Index = 0; Index < NameEntries.Num(); ++Index)
		{
			int32 Number;
			Ar << Number;
			Container.Names[Index] = NameEntries[Index].ToName(Number);
		}
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FExportBundleEntry& ExportBundleEntry)
{
	Ar << ExportBundleEntry.LocalExportIndex;
	Ar << ExportBundleEntry.CommandType;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FDependencyBundleEntry& DependencyBundleEntry)
{
	Ar << DependencyBundleEntry.LocalImportOrExportIndex;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FDependencyBundleHeader& DependencyBundleHeader)
{
	Ar << DependencyBundleHeader.FirstEntryIndex;
	for (int32 I = 0; I < FExportBundleEntry::ExportCommandType_Count; ++I)
	{
		for (int32 J = 0; J < FExportBundleEntry::ExportCommandType_Count; ++J)
		{
			Ar << DependencyBundleHeader.EntryCount[I][J];
		}
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FScriptObjectEntry& ScriptObjectEntry)
{
	Ar << ScriptObjectEntry.Mapped;
	Ar << ScriptObjectEntry.GlobalIndex;
	Ar << ScriptObjectEntry.OuterIndex;
	Ar << ScriptObjectEntry.CDOClassIndex;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FExportMapEntry& ExportMapEntry)
{
	Ar << ExportMapEntry.CookedSerialOffset;
	Ar << ExportMapEntry.CookedSerialSize;
	Ar << ExportMapEntry.ObjectName;
	Ar << ExportMapEntry.OuterIndex;
	Ar << ExportMapEntry.ClassIndex;
	Ar << ExportMapEntry.SuperIndex;
	Ar << ExportMapEntry.TemplateIndex;
	Ar << ExportMapEntry.PublicExportHash;

	uint32 ObjectFlags = uint32(ExportMapEntry.ObjectFlags);
	Ar << ObjectFlags;
	
	if (Ar.IsLoading())
	{
		ExportMapEntry.ObjectFlags = EObjectFlags(ObjectFlags);
	}

	uint8 FilterFlags = uint8(ExportMapEntry.FilterFlags);
	Ar << FilterFlags;

	if (Ar.IsLoading())
	{
		ExportMapEntry.FilterFlags = EExportFilterFlags(FilterFlags);
	}

	Ar.Serialize(&ExportMapEntry.Pad, sizeof(ExportMapEntry.Pad));

	return Ar;
}
	
FArchive& operator<<(FArchive& Ar, FBulkDataMapEntry& BulkDataEntry)
{
	Ar << BulkDataEntry.SerialOffset;
	Ar << BulkDataEntry.DuplicateSerialOffset;
	Ar << BulkDataEntry.SerialSize;
	Ar << BulkDataEntry.Flags;
	Ar << BulkDataEntry.Pad;

	return Ar;
}

uint64 FPackageObjectIndex::GenerateImportHashFromObjectPath(const FStringView& ObjectPath)
{
	TArray<TCHAR, TInlineAllocator<FName::StringBufferSize>> FullImportPath;
	const int32 Len = ObjectPath.Len();
	FullImportPath.AddUninitialized(Len);
	for (int32 I = 0; I < Len; ++I)
	{
		if (ObjectPath[I] == TEXT('.') || ObjectPath[I] == TEXT(':'))
		{
			FullImportPath[I] = TEXT('/');
		}
		else
		{
			FullImportPath[I] = TChar<TCHAR>::ToLower(ObjectPath[I]);
		}
	}
	uint64 Hash = CityHash64(reinterpret_cast<const char*>(FullImportPath.GetData()), Len * sizeof(TCHAR));
	Hash &= ~(3ull << 62ull);
	return Hash;
}

void FindAllRuntimeScriptPackages(TArray<UPackage*>& OutPackages)
{
	OutPackages.Empty(256);
	ForEachObjectOfClass(UPackage::StaticClass(), [&OutPackages](UObject* InPackageObj)
	{
		UPackage* Package = CastChecked<UPackage>(InPackageObj);
		if (Package->HasAnyPackageFlags(PKG_CompiledIn))
		{
			TCHAR Buffer[FName::StringBufferSize];
			if (FStringView(Buffer, Package->GetFName().ToString(Buffer)).StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
			{
				OutPackages.Add(Package);
			}
		}
	}, /*bIncludeDerivedClasses*/false);
}

#ifndef ALT2_VERIFY_LINKERLOAD_MATCHES_IMPORTSTORE
#define ALT2_VERIFY_LINKERLOAD_MATCHES_IMPORTSTORE 0
#endif

#ifndef ALT2_ENABLE_LINKERLOAD_SUPPORT
#define ALT2_ENABLE_LINKERLOAD_SUPPORT WITH_EDITOR
#endif

#ifndef ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
#define ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD 0
#endif

#ifndef ALT2_LOG_VERBOSE
#define ALT2_LOG_VERBOSE DO_CHECK
#endif

#ifndef ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING
#define ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING 0
#endif

static TSet<FPackageId> GAsyncLoading2_DebugPackageIds;
static FString GAsyncLoading2_DebugPackageNamesString;
static TSet<FPackageId> GAsyncLoading2_VerbosePackageIds;
static FString GAsyncLoading2_VerbosePackageNamesString;
static int32 GAsyncLoading2_VerboseLogFilter = 2; //None=0,Filter=1,All=2
#if !UE_BUILD_SHIPPING
static void ParsePackageNames(const FString& PackageNamesString, TSet<FPackageId>& PackageIds)
{
	TArray<FString> Args;
	const TCHAR* Delimiters[] = { TEXT(","), TEXT(" ") };
	PackageNamesString.ParseIntoArray(Args, Delimiters, UE_ARRAY_COUNT(Delimiters), true);
	PackageIds.Reserve(PackageIds.Num() + Args.Num());
	for (const FString& PackageName : Args)
	{
		if (PackageName.Len() > 0 && FChar::IsDigit(PackageName[0]))
		{
			uint64 Value;
			LexFromString(Value, *PackageName);
			PackageIds.Add(*(FPackageId*)(&Value));
		}
		else
		{
			PackageIds.Add(FPackageId::FromName(FName(*PackageName)));
		}
	}
}
static FAutoConsoleVariableRef CVar_DebugPackageNames(
	TEXT("s.DebugPackageNames"),
	GAsyncLoading2_DebugPackageNamesString,
	TEXT("Add debug breaks for all listed package names, also automatically added to s.VerbosePackageNames."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		GAsyncLoading2_DebugPackageIds.Reset();
		ParsePackageNames(Variable->GetString(), GAsyncLoading2_DebugPackageIds);
		ParsePackageNames(Variable->GetString(), GAsyncLoading2_VerbosePackageIds);
		GAsyncLoading2_VerboseLogFilter = GAsyncLoading2_VerbosePackageIds.Num() > 0 ? 1 : 2;
	}),
	ECVF_Default);
static FAutoConsoleVariableRef CVar_VerbosePackageNames(
	TEXT("s.VerbosePackageNames"),
	GAsyncLoading2_VerbosePackageNamesString,
	TEXT("Restrict verbose logging to listed package names."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		GAsyncLoading2_VerbosePackageIds.Reset();
		ParsePackageNames(Variable->GetString(), GAsyncLoading2_VerbosePackageIds);
		GAsyncLoading2_VerboseLogFilter = GAsyncLoading2_VerbosePackageIds.Num() > 0 ? 1 : 2;
	}),
	ECVF_Default);
#endif

#define UE_ASYNC_PACKAGE_DEBUG(PackageDesc) \
if (GAsyncLoading2_DebugPackageIds.Contains((PackageDesc).UPackageId)) \
{ \
	UE_DEBUG_BREAK(); \
}

#define UE_ASYNC_UPACKAGE_DEBUG(UPackage) \
if (GAsyncLoading2_DebugPackageIds.Contains((UPackage)->GetPackageId())) \
{ \
	UE_DEBUG_BREAK(); \
}

#define UE_ASYNC_PACKAGEID_DEBUG(PackageId) \
if (GAsyncLoading2_DebugPackageIds.Contains(PackageId)) \
{ \
	UE_DEBUG_BREAK(); \
}

// The ELogVerbosity::VerbosityMask is used to silence PVS,
// using constexpr gave the same warning, and the disable comment can can't be used in a macro: //-V501 
// warning V501: There are identical sub-expressions 'ELogVerbosity::Verbose' to the left and to the right of the '<' operator.
#define UE_ASYNC_PACKAGE_LOG(Verbosity, PackageDesc, LogDesc, Format, ...) \
if ((ELogVerbosity::Type(ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::Verbose) || \
	(GAsyncLoading2_VerboseLogFilter == 2) || \
	(GAsyncLoading2_VerboseLogFilter == 1 && GAsyncLoading2_VerbosePackageIds.Contains((PackageDesc).UPackageId))) \
{ \
	UE_LOG(LogStreaming, Verbosity, LogDesc TEXT(": %s (0x%llX) %s (0x%llX) - ") Format, \
		*(PackageDesc).UPackageName.ToString(), \
		(PackageDesc).UPackageId.ValueForDebugging(), \
		*(PackageDesc).PackagePathToLoad.GetPackageFName().ToString(), \
		(PackageDesc).PackageIdToLoad.ValueForDebugging(), \
		##__VA_ARGS__); \
}

#define UE_ASYNC_PACKAGE_CLOG(Condition, Verbosity, PackageDesc, LogDesc, Format, ...) \
if ((Condition)) \
{ \
	UE_ASYNC_PACKAGE_LOG(Verbosity, PackageDesc, LogDesc, Format, ##__VA_ARGS__); \
}

#if ALT2_LOG_VERBOSE
#define UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbosity, PackageDesc, LogDesc, Format, ...) \
	UE_ASYNC_PACKAGE_LOG(Verbosity, PackageDesc, LogDesc, Format, ##__VA_ARGS__)
#define UE_ASYNC_PACKAGE_CLOG_VERBOSE(Condition, Verbosity, PackageDesc, LogDesc, Format, ...) \
	UE_ASYNC_PACKAGE_CLOG(Condition, Verbosity, PackageDesc, LogDesc, Format, ##__VA_ARGS__)
#else
#define UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbosity, PackageDesc, LogDesc, Format, ...)
#define UE_ASYNC_PACKAGE_CLOG_VERBOSE(Condition, Verbosity, PackageDesc, LogDesc, Format, ...)
#endif

static bool GRemoveUnreachableObjectsOnGT = false;
static FAutoConsoleVariableRef CVarGRemoveUnreachableObjectsOnGT(
	TEXT("s.RemoveUnreachableObjectsOnGT"),
	GRemoveUnreachableObjectsOnGT,
	TEXT("Remove unreachable objects from GlobalImportStore on the GT from the GC callback NotifyUnreachableObjects (slow)."),
	ECVF_Default
	);

#if UE_BUILD_DEBUG || (UE_BUILD_DEVELOPMENT && !WITH_EDITOR)
static bool GVerifyUnreachableObjects = true;
static bool GVerifyObjectLoadFlags = true;
#else
static bool GVerifyUnreachableObjects = false;
static bool GVerifyObjectLoadFlags = false;
#endif

static FAutoConsoleVariableRef CVarGVerifyUnreachableObjects(
	TEXT("s.VerifyUnreachableObjects"),
	GVerifyUnreachableObjects,
	TEXT("Run GlobalImportStore verifications for unreachable objects from the GC callback NotifyUnreachableObjects (slow)."),
	ECVF_Default
	);

static FAutoConsoleVariableRef CVarGVerifyObjectLoadFlags(
	TEXT("s.VerifyObjectLoadFlags"),
	GVerifyObjectLoadFlags,
	TEXT("Run AsyncFlags verifications for all objects when finished loading from the GC callback NotifyUnreachableObjects (slow)."),
	ECVF_Default
	);

static bool GOnlyProcessRequiredPackagesWhenSyncLoading = true;
static FAutoConsoleVariableRef CVarGOnlyProcessRequiredPackagesWhenSyncLoading(
	TEXT("s.OnlyProcessRequiredPackagesWhenSyncLoading"),
	GOnlyProcessRequiredPackagesWhenSyncLoading,
	TEXT("When sync loading a package process only that package and its imports"),
	ECVF_Default
);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, FileIO);

TRACE_DECLARE_ATOMIC_INT_COUNTER(AsyncLoadingQueuedPackages, TEXT("AsyncLoading/PackagesQueued"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(AsyncLoadingLoadingPackages, TEXT("AsyncLoading/PackagesLoading"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(AsyncLoadingPackagesWithRemainingWork, TEXT("AsyncLoading/PackagesWithRemainingWork"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(AsyncLoadingPendingIoRequests, TEXT("AsyncLoading/PendingIoRequests"));
TRACE_DECLARE_ATOMIC_MEMORY_COUNTER(AsyncLoadingTotalLoaded, TEXT("AsyncLoading/TotalLoaded"));

FString FormatPackageId(FPackageId PackageId)
{
#if WITH_PACKAGEID_NAME_MAP
	return FString::Printf(TEXT("0x%llX (%s)"), PackageId.ValueForDebugging(), *PackageId.GetName().ToString());
#else
	return FString::Printf(TEXT("0x%llX"), PackageId.ValueForDebugging());
#endif
}

struct FAsyncPackage2;
class FAsyncLoadingThread2;

class FSimpleArchive final
	: public FArchive
{
public:
	FSimpleArchive(const uint8* BufferPtr, uint64 BufferSize)
	{
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
		ActiveFPLB = &InlineFPLB;
#endif
		ActiveFPLB->OriginalFastPathLoadBuffer = BufferPtr;
		ActiveFPLB->StartFastPathLoadBuffer = BufferPtr;
		ActiveFPLB->EndFastPathLoadBuffer = BufferPtr + BufferSize;
	}

	int64 TotalSize() override
	{
		return ActiveFPLB->EndFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer;
	}

	int64 Tell() override
	{
		return ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer;
	}

	void Seek(int64 Position) override
	{
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + Position;
		check(ActiveFPLB->StartFastPathLoadBuffer <= ActiveFPLB->EndFastPathLoadBuffer);
	}

	void Serialize(void* Data, int64 Length) override
	{
		if (!Length || IsError())
		{
			return;
		}
		check(ActiveFPLB->StartFastPathLoadBuffer + Length <= ActiveFPLB->EndFastPathLoadBuffer);
		FMemory::Memcpy(Data, ActiveFPLB->StartFastPathLoadBuffer, Length);
		ActiveFPLB->StartFastPathLoadBuffer += Length;
	}
private:
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
	FArchive::FFastPathLoadBuffer InlineFPLB;
	FArchive::FFastPathLoadBuffer* ActiveFPLB;
#endif
};

struct FExportObject
{
	UObject* Object = nullptr;
	UObject* TemplateObject = nullptr;
	UObject* SuperObject = nullptr;
	bool bFiltered = false;
	bool bExportLoadFailed = false;
	bool bWasFoundInMemory = false;
};

class FAsyncLoadingSyncLoadContext;

struct FPackageReferencer
{
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	FName ReferencerPackageName;
	FName ReferencerPackageOp;
#endif

#if WITH_EDITOR
	ECookLoadType CookLoadType = ECookLoadType::Unexpected;
#endif
};

struct FPackageRequest
{
	int32 RequestId = -1;
	int32 Priority = -1;
	EPackageFlags PackageFlags = PKG_None;
#if WITH_EDITOR
	uint32 LoadFlags = LOAD_None;
	int32 PIEInstanceID = INDEX_NONE;
#endif
	FLinkerInstancingContext InstancingContext;
	FName CustomName;
	FPackagePath PackagePath;
	TUniquePtr<FLoadPackageAsyncDelegate> PackageLoadedDelegate;
	TUniquePtr<FLoadPackageAsyncProgressDelegate> PackageProgressDelegate;
	FPackageRequest* Next = nullptr;
	FPackageReferencer PackageReferencer;

	FLinkerInstancingContext* GetInstancingContext()
	{
		return &InstancingContext;
	}

	static FPackageRequest Create(int32 RequestId, EPackageFlags PackageFlags, uint32 LoadFlags, int32 PIEInstanceID, int32 Priority, const FLinkerInstancingContext* InstancingContext, const FPackagePath& PackagePath, FName CustomName, TUniquePtr<FLoadPackageAsyncDelegate> PackageLoadedDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate> PackageProgressDelegate, FPackageReferencer PackageReferencer)
	{
		return FPackageRequest
		{
			RequestId,
			Priority,
			PackageFlags,
#if WITH_EDITOR
			LoadFlags,
			PIEInstanceID,
#endif
			InstancingContext ? *InstancingContext : FLinkerInstancingContext(),
			CustomName,
			PackagePath,
			MoveTemp(PackageLoadedDelegate),
			MoveTemp(PackageProgressDelegate),
			nullptr,
			PackageReferencer
		};
	}
};

struct FAsyncPackageDesc2
{
	// A unique request id for each external call to LoadPackage
	int32 RequestID;
	// Package priority
	int32 Priority;
	/** The flags that should be applied to the package */
	EPackageFlags PackageFlags;
#if WITH_EDITOR
	uint32 LoadFlags;
	/** PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;
#endif
	/** Instancing context, maps original package to their instanced counterpart, used to remap imports. */
	FLinkerInstancingContext InstancingContext;
	// The package id of the UPackage being loaded
	// It will be used as key when tracking active async packages
	FPackageId UPackageId;
	// The id of the package being loaded from disk
	FPackageId PackageIdToLoad; 
	// The name of the UPackage being loaded
	// Set to none for imported packages up until the package summary has been serialized
	FName UPackageName;
	// The package path of the package being loaded from disk
	// Set to none for imported packages up until the package summary has been serialized
	FPackagePath PackagePathToLoad;
	// Package referencer
	FPackageReferencer PackageReferencer;
	// Packages with a a custom name can't be imported
	bool bCanBeImported;

	static FAsyncPackageDesc2 FromPackageRequest(
		FPackageRequest& Request,
		FName UPackageName,
		FPackageId PackageIdToLoad)
	{
		return FAsyncPackageDesc2
		{
			Request.RequestId,
			Request.Priority,
			Request.PackageFlags,
#if WITH_EDITOR
			Request.LoadFlags,
			Request.PIEInstanceID,
#endif
			MoveTemp(Request.InstancingContext),
			FPackageId::FromName(UPackageName),
			PackageIdToLoad,
			UPackageName,
			MoveTemp(Request.PackagePath),
			Request.PackageReferencer,
#if WITH_EDITOR
			true
#else
			Request.CustomName.IsNone()
#endif
		};
	}

	static FAsyncPackageDesc2 FromPackageImport(
		const FAsyncPackageDesc2& ImportingPackageDesc,
		FName UPackageName,
		FPackageId ImportedPackageId,
		FPackageId PackageIdToLoad,
		FPackagePath&& PackagePathToLoad)
	{
		return FAsyncPackageDesc2
		{
			INDEX_NONE,
			ImportingPackageDesc.Priority,
			PKG_None,
#if WITH_EDITOR
			LOAD_None,
			INDEX_NONE,
#endif
			FLinkerInstancingContext(),
			ImportedPackageId,
			PackageIdToLoad,
			UPackageName,
			MoveTemp(PackagePathToLoad),
			ImportingPackageDesc.PackageReferencer,
			true
		};
	}
};

struct FUnreachableObject
{
	FPackageId PackageId;
	int32 ObjectIndex = -1;
	FName ObjectName;
};

using FUnreachableObjects = TArray<FUnreachableObject>;

class FLoadedPackageRef
{
private:
	friend class FGlobalImportStore;

	class FPublicExportMap
	{
	public:
		FPublicExportMap() = default;

		FPublicExportMap(const FPublicExportMap&) = delete;

		FPublicExportMap(FPublicExportMap&& Other)
		{
			Allocation = Other.Allocation;
			Count = Other.Count;
			SingleItemValue = Other.SingleItemValue;
			Other.Allocation = nullptr;
			Other.Count = 0;
		};

		FPublicExportMap& operator=(const FPublicExportMap&) = delete;

		FPublicExportMap& operator=(FPublicExportMap&& Other)
		{
			if (Count > 1)
			{
				FMemory::Free(Allocation);
			}
			Allocation = Other.Allocation;
			Count = Other.Count;
			SingleItemValue = Other.SingleItemValue;
			Other.Allocation = nullptr;
			Other.Count = 0;
			return *this;
		}
	
		~FPublicExportMap()
		{
			if (Count > 1)
			{
				FMemory::Free(Allocation);
			}
		}
		void Grow(int32 NewCount)
		{
			if (NewCount <= Count)
			{
				return;
			}
			if (NewCount > 1)
			{
				TArrayView<uint64> OldKeys = GetKeys();
				TArrayView<int32> OldValues = GetValues();
				const uint64 OldKeysSize = Count * sizeof(uint64);
				const uint64 NewKeysSize = NewCount * sizeof(uint64);
				const uint64 OldValuesSize = Count * sizeof(int32);
				const uint64 NewValuesSize = NewCount * sizeof(int32);
				const uint64 KeysToAddSize = NewKeysSize - OldKeysSize;
				const uint64 ValuesToAddSize = NewValuesSize - OldValuesSize;

				uint8* NewAllocation = reinterpret_cast<uint8*>(FMemory::Malloc(NewKeysSize + NewValuesSize));
				FMemory::Memzero(NewAllocation, KeysToAddSize); // Insert new keys initialized to zero
				FMemory::Memcpy(NewAllocation + KeysToAddSize, OldKeys.GetData(), OldKeysSize); // Copy old keys
				FMemory::Memset(NewAllocation + NewKeysSize, 0xFF, ValuesToAddSize); // Insert new values initialized to -1
				FMemory::Memcpy(NewAllocation + NewKeysSize + ValuesToAddSize, OldValues.GetData(), OldValuesSize); // Copy old values
				if (Count > 1)
				{
					FMemory::Free(Allocation);
				}
				Allocation = NewAllocation;
			}
			Count = NewCount;
		}

		void Store(uint64 ExportHash, UObject* Object)
		{
			TArrayView<uint64> Keys = GetKeys();
			TArrayView<int32> Values = GetValues();
			int32 Index = Algo::LowerBound(Keys, ExportHash);
			if (Index < Count && Keys[Index] == ExportHash)
			{
				// Slot already exists so reuse it
				Values[Index] = GUObjectArray.ObjectToIndex(Object);
				return;
			}
			if (Count == 0 || Keys[0] != 0)
			{
				// No free slots so we need to add one (will be inserted at the beginning of the array)
				Grow(Count + 1);
				Keys = GetKeys();
				Values = GetValues();
			}
			else
			{
				--Index; // Update insertion index to one before the lower bound item
			}
			if (Index > 0)
			{
				// Move items down
				FMemory::Memmove(Keys.GetData(), Keys.GetData() + 1, Index * sizeof(uint64));
				FMemory::Memmove(Values.GetData(), Values.GetData() + 1, Index * sizeof(int32));
			}
			Keys[Index] = ExportHash;
			Values[Index] = GUObjectArray.ObjectToIndex(Object);
		}

		bool Remove(uint64 ExportHash)
		{
			TArrayView<uint64> Keys = GetKeys();
			int32 Index = Algo::LowerBound(Keys, ExportHash);
			if (Index < Count && Keys[Index] == ExportHash)
			{
				TArrayView<int32> Values = GetValues();
				Values[Index] = -1;
				return true;
			}

			return false;
		}

		UObject* Find(uint64 ExportHash)
		{
			TArrayView<uint64> Keys = GetKeys();
			int32 Index = Algo::LowerBound(Keys, ExportHash);
			if (Index < Count && Keys[Index] == ExportHash)
			{
				TArrayView<int32> Values = GetValues();
				int32 ObjectIndex = Values[Index];
				if (ObjectIndex >= 0)
				{
					return static_cast<UObject*>(GUObjectArray.IndexToObject(ObjectIndex)->Object);
				}
			}
			return nullptr;
		}

		[[nodiscard]] bool PinForGC(TArray<int32>& OutUnreachableObjectIndices)
		{
			OutUnreachableObjectIndices.Reset();
			for (int32& ObjectIndex : GetValues())
			{
				if (ObjectIndex >= 0)
				{
					FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
					if (!ObjectItem->IsUnreachable())
					{
						UObject* Object = static_cast<UObject*>(ObjectItem->Object);
						checkf(!ObjectItem->HasAnyFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Object->GetFullName());
						ObjectItem->SetFlags(EInternalObjectFlags::LoaderImport);
					}
					else
					{
						OutUnreachableObjectIndices.Reserve(Count);
						OutUnreachableObjectIndices.Add(ObjectIndex);
						ObjectIndex = -1;
					}
				}
			}
			return OutUnreachableObjectIndices.Num() == 0;
		}

		void UnpinForGC()
		{
			for (int32 ObjectIndex : GetValues())
			{
				if (ObjectIndex >= 0)
				{
					UObject* Object = static_cast<UObject*>(GUObjectArray.IndexToObject(ObjectIndex)->Object);
					checkf(Object->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Object->GetFullName());
					Object->AtomicallyClearInternalFlags(EInternalObjectFlags::LoaderImport);
				}
			}
		}

		TArrayView<uint64> GetKeys()
		{
			if (Count == 1)
			{
				return MakeArrayView(&SingleItemKey, 1);
			}
			else
			{
				return MakeArrayView(reinterpret_cast<uint64*>(Allocation), Count);
			}
		}

		TArrayView<int32> GetValues()
		{
			if (Count == 1)
			{
				return MakeArrayView(&SingleItemValue, 1);
			}
			else
			{
				return MakeArrayView(reinterpret_cast<int32*>(Allocation + Count * sizeof(uint64)), Count);
			}
		}

	private:
		union
		{
			uint8* Allocation = nullptr;
			uint64 SingleItemKey;
		};
		int32 Count = 0;
		int32 SingleItemValue = -1;
	};

	FPublicExportMap PublicExportMap;
	FName OriginalPackageName;
	int32 PackageObjectIndex = -1;
	int32 RefCount = 0;
	bool bAreAllPublicExportsLoaded = false;
	bool bIsMissing = false;
	bool bHasFailed = false;
	bool bHasBeenLoadedDebug = false;

public:
	FLoadedPackageRef() = default;

	FLoadedPackageRef(const FLoadedPackageRef& Other) = delete;

	FLoadedPackageRef(FLoadedPackageRef&& Other) = default;

	FLoadedPackageRef& operator=(const FLoadedPackageRef& Other) = delete;

	FLoadedPackageRef& operator=(FLoadedPackageRef&& Other) = default;
	
	inline int32 GetRefCount() const
	{
		return RefCount;
	}

	inline FName GetOriginalPackageName() const
	{
		return OriginalPackageName;
	}

	inline bool HasPackage() const
	{
		return PackageObjectIndex >= 0;
	}

	inline UPackage* GetPackage() const
	{
		if (HasPackage())
		{
			return static_cast<UPackage*>(GUObjectArray.IndexToObject(PackageObjectIndex)->Object);
		}
		return nullptr;
	}

	inline void SetPackage(UPackage* InPackage)
	{
		check(!bAreAllPublicExportsLoaded);
		check(!bIsMissing);
		check(!bHasFailed);
		check(!HasPackage());
		if (InPackage)
		{
			PackageObjectIndex = GUObjectArray.ObjectToIndex(InPackage);
			OriginalPackageName = InPackage->GetFName();
		}
		else
		{
			PackageObjectIndex = -1;
			OriginalPackageName = FName();
		}
	}

	void RemoveUnreferencedObsoletePackage()
	{
		check(RefCount == 0);
		*this = FLoadedPackageRef();
	}

	void ReplaceReferencedRenamedPackage(UPackage* NewPackage)
	{
		// keep RefCount and PublicExportMap while resetting all other state,
		// the public exports will be replaced one by one from StoreGlobalObject
		bAreAllPublicExportsLoaded = false;
		bIsMissing = false;
		bHasFailed = false;
		bHasBeenLoadedDebug = false;
		PackageObjectIndex = GUObjectArray.ObjectToIndex(NewPackage);
		OriginalPackageName = NewPackage->GetFName();
	}

	inline bool AreAllPublicExportsLoaded() const
	{
		return bAreAllPublicExportsLoaded && OriginalPackageName == GetPackage()->GetFName();
	}

	inline void SetAllPublicExportsLoaded()
	{
		check(!bIsMissing);
		check(!bHasFailed);
		check(HasPackage());
		bIsMissing = false;
		bAreAllPublicExportsLoaded = true;
		bHasBeenLoadedDebug = true;
	}

	inline void SetIsMissingPackage()
	{
		check(!bAreAllPublicExportsLoaded);
		check(!HasPackage());
		bIsMissing = true;
		bAreAllPublicExportsLoaded = false;
	}

	inline void ClearErrorFlags()
	{
		bIsMissing = false;
		bHasFailed = false;
	}

	inline void SetHasFailed()
	{
		bHasFailed = true;
	}

	TArrayView<int32> GetPublicExportObjectIndices()
	{
		return PublicExportMap.GetValues();
	}

	void ReserveSpaceForPublicExports(int32 PublicExportCount)
	{
		PublicExportMap.Grow(PublicExportCount);
	}

	void StorePublicExport(uint64 ExportHash, UObject* Object)
	{
		PublicExportMap.Store(ExportHash, Object);
	}

	void RemovePublicExport(uint64 ExportHash)
	{
		check(!bIsMissing);
		check(HasPackage());
		if (PublicExportMap.Remove(ExportHash))
		{
			bAreAllPublicExportsLoaded = false;
		}
	}

	UObject* GetPublicExport(uint64 ExportHash)
	{
		return PublicExportMap.Find(ExportHash);
	}

	void PinPublicExportsForGC(TArray<int32>& OutUnreachableObjectIndices)
	{
		UPackage* Package = GetPackage();
		UE_ASYNC_UPACKAGE_DEBUG(Package);

		if (GUObjectArray.IsDisregardForGC(Package))
		{
			return;
		}
		bAreAllPublicExportsLoaded = PublicExportMap.PinForGC(OutUnreachableObjectIndices);
		checkf(!Package->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Package->GetFullName());
		Package->SetInternalFlags(EInternalObjectFlags::LoaderImport);
	}

	void UnpinPublicExportsForGC()
	{
		UPackage* Package = GetPackage();
		UE_ASYNC_UPACKAGE_DEBUG(Package);

		if (GUObjectArray.IsDisregardForGC(Package))
		{
			return;
		}
		PublicExportMap.UnpinForGC();
		checkf(Package->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Package->GetFullName());
		Package->AtomicallyClearInternalFlags(EInternalObjectFlags::LoaderImport);
	}
};

class FGlobalImportStore
{
private:
	// Packages in active loading or completely loaded packages, with Desc.UPackageId as key.
	// Does not track temp packages with custom UPackage names, since they are never imorted by other packages.
	TMap<FPackageId, FLoadedPackageRef> Packages;
	// All native script objects (structs, enums, classes, CDOs and their subobjects) from the initial load phase
	TMap<FPackageObjectIndex, UObject*> ScriptObjects;
	// All currently loaded public export objects from any loaded package
	TMap<int32, FPublicExportKey> ObjectIndexToPublicExport;

public:
	FGlobalImportStore()
	{
		Packages.Reserve(32768);
		ScriptObjects.Reserve(32768);
		ObjectIndexToPublicExport.Reserve(32768);
	}

	int32 GetStoredPackagesCount() const
	{
		return Packages.Num();
	}

	int32 GetStoredScriptObjectsCount() const
	{
		return ScriptObjects.Num();
	}

	SIZE_T GetStoredScriptObjectsAllocatedSize() const
	{
		return ScriptObjects.GetAllocatedSize();
	}

	int32 GetStoredPublicExportsCount() const
	{
		return ObjectIndexToPublicExport.Num();
	}

	inline FLoadedPackageRef* FindPackageRef(FPackageId PackageId)
	{
		return Packages.Find(PackageId);
	}

	inline FLoadedPackageRef& FindPackageRefChecked(FPackageId PackageId, FName Name = FName())
	{
		FLoadedPackageRef* PackageRef = FindPackageRef(PackageId);
		UE_CLOG(!PackageRef, LogStreaming, Fatal, TEXT("FindPackageRefChecked: Package %s (0x%llX) has been deleted"),
			*Name.ToString(), PackageId.ValueForDebugging());
		return *PackageRef;
	}

	inline FLoadedPackageRef& AddPackageRef(FPackageId PackageId, FName PackageNameIfKnown)
	{
		LLM_SCOPE_BYNAME(TEXT("AsyncLoadPackageStore"));

		FLoadedPackageRef& PackageRef = Packages.FindOrAdd(PackageId);
		// is this the first reference to a package that already exists?
		if (PackageRef.RefCount == 0)
		{
			// Remove stale package before searching below as its possible a UEDPIE package got trashed and replaced by a new one
			// and its important that we find the one that replaced it so we don't try to load it if its a PKG_InMemoryOnly package.
			if (UPackage* Package = PackageRef.GetPackage())
			{
				if (Package->IsUnreachable() || PackageRef.GetOriginalPackageName() != Package->GetFName())
				{
					RemoveUnreferencedObsoletePackage(PackageRef);
				}
			}
#if WITH_EDITOR
			if (!PackageRef.HasPackage() && !PackageNameIfKnown.IsNone())
			{
				UPackage* FoundPackage = FindObjectFast<UPackage>(nullptr, PackageNameIfKnown);
				if (FoundPackage)
				{
					// We need to maintain a 1:1 relationship between PackageId and Package object
					// to avoid having a costly 1:N lookup during GC. If we find an existing package
					// that already has a valid packageid in our table, it means it was renamed
					// after it finished loading. Since our mapping is now "stale" we remove
					// it before replacing the package ID in the package object, otherwise
					// the mapping would leak during GC and we could end up crashing when
					// trying to access the ref after the GC has run.
					FPackageId OldPackageId = FoundPackage->GetPackageId();
					if (OldPackageId.IsValid() && OldPackageId != PackageId)
					{
						FLoadedPackageRef* OldPackageRef = Packages.Find(OldPackageId);
						if (OldPackageRef)
						{
							UE_LOG(LogStreaming, Display,
								TEXT("FGlobalImportStore:AddPackageRef: Dropping stale reference to package %s (0x%llX) that has been renamed to %s (0x%llX)"),
								*OldPackageRef->GetOriginalPackageName().ToString(),
								OldPackageId.ValueForDebugging(),
								*FoundPackage->GetName(),
								PackageId.ValueForDebugging()
							);
							
							check(OldPackageRef->GetRefCount() == 0);
							RemoveUnreferencedObsoletePackage(*OldPackageRef);
							RemovePackage(OldPackageId);
						}
					}

					if (PackageRef.bIsMissing)
					{
						PackageRef.bIsMissing = false;
						UE_LOG(LogStreaming, Warning,
							TEXT("FGlobalImportStore:AddPackageRef: Found reference to previously missing package %s (0x%llX)"),
							*FoundPackage->GetName(),
							PackageId.ValueForDebugging()
						);
					}

					PackageRef.SetPackage(FoundPackage);
					FoundPackage->SetCanBeImportedFlag(true);
					FoundPackage->SetPackageId(PackageId);
				}
			}
			if (PackageRef.HasPackage())
			{
				if (PackageRef.GetPackage()->bHasBeenFullyLoaded)
				{
					PackageRef.SetAllPublicExportsLoaded();
				}
			}
#endif
			if (UPackage* Package = PackageRef.GetPackage())
			{
				if (Package->IsUnreachable() || PackageRef.GetOriginalPackageName() != Package->GetFName())
				{
					UE_CLOG(!Package->IsUnreachable(), LogStreaming, Display,
						TEXT("FGlobalImportStore:AddPackageRef: Dropping renamed package %s before reloading %s (0x%llX)"),
						*Package->GetName(),
						*PackageRef.GetOriginalPackageName().ToString(),
						Package->GetPackageId().ValueForDebugging());

					RemoveUnreferencedObsoletePackage(PackageRef);
				}
				else
				{
					TArray<int32> UnreachableObjectIndices;
					PackageRef.PinPublicExportsForGC(UnreachableObjectIndices);
					for (int32 ObjectIndex : UnreachableObjectIndices)
					{
						ObjectIndexToPublicExport.Remove(ObjectIndex);
					}
				}
			}
		}
		++PackageRef.RefCount;
		return PackageRef;
	}

	inline void ReleasePackageRef(FPackageId PackageId, FPackageId FromPackageId = FPackageId())
	{
		FLoadedPackageRef& PackageRef = FindPackageRefChecked(PackageId);

		check(PackageRef.RefCount > 0);
		--PackageRef.RefCount;

#if DO_CHECK
		ensureMsgf(!PackageRef.bHasBeenLoadedDebug || PackageRef.bAreAllPublicExportsLoaded || PackageRef.bIsMissing || PackageRef.bHasFailed,
			TEXT("LoadedPackageRef from None (0x%llX) to %s (0x%llX) should not have been released when the package is not complete.")
			TEXT("RefCount=%d, AreAllExportsLoaded=%d, IsMissing=%d, HasFailed=%d, HasBeenLoaded=%d"),
			FromPackageId.ValueForDebugging(),
			*PackageRef.GetOriginalPackageName().ToString(),
			PackageId.Value(),
			PackageRef.RefCount,
			PackageRef.bAreAllPublicExportsLoaded,
			PackageRef.bIsMissing,
			PackageRef.bHasFailed,
			PackageRef.bHasBeenLoadedDebug);

		if (PackageRef.bAreAllPublicExportsLoaded)
		{
			check(!PackageRef.bIsMissing);
		}
		if (PackageRef.bIsMissing)
		{
			check(!PackageRef.bAreAllPublicExportsLoaded);
		}
#endif
		// is this the last reference to a loaded package?
		if (PackageRef.RefCount == 0 && PackageRef.HasPackage())
		{
			PackageRef.UnpinPublicExportsForGC();
		}
	}

	void VerifyLoadedPackages()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(VerifyLoadedPackages);
		for (TPair<FPackageId, FLoadedPackageRef>& Pair : Packages)
		{
			FPackageId& PackageId = Pair.Key;
			FLoadedPackageRef& Ref = Pair.Value;
			ensureMsgf(Ref.GetRefCount() == 0,
				TEXT("PackageId '%s' with ref count %d should not have a ref count now")
				TEXT(", or this check is incorrectly reached during active loading."),
				*FormatPackageId(PackageId),
				Ref.GetRefCount());
		}
	}

	void RemoveUnreferencedObsoletePackage(FLoadedPackageRef& PackageRef)
	{
		UPackage* OldPackage = PackageRef.GetPackage();
		UE_ASYNC_UPACKAGE_DEBUG(OldPackage);

		if (GVerifyUnreachableObjects)
		{
			VerifyPackageForRemoval(PackageRef);
		}
		for (int32 ObjectIndex : PackageRef.GetPublicExportObjectIndices())
		{
			if (ObjectIndex >= 0)
			{
				ObjectIndexToPublicExport.Remove(ObjectIndex);
			}
		}
		PackageRef.RemoveUnreferencedObsoletePackage();
		// Reset PackageId to prevent a double remove from GC NotifyUnreachableObjects
		OldPackage->SetPackageId(FPackageId());
	}

	void ReplaceReferencedRenamedPackage(FLoadedPackageRef& PackageRef, UPackage* NewPackage)
	{
		UPackage* OldPackage = PackageRef.GetPackage();
		UE_ASYNC_UPACKAGE_DEBUG(OldPackage);

		PackageRef.ReplaceReferencedRenamedPackage(NewPackage);
		// Clear LoaderImport so GC may destroy this package
		OldPackage->AtomicallyClearInternalFlags(EInternalObjectFlags::LoaderImport);
		// Update PackageId to prevent us from removing our updated package ref from GC
		OldPackage->SetPackageId(FPackageId::FromName(OldPackage->GetFName()));
	}

	void RemovePackages(const FUnreachableObjects& ObjectsToRemove)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RemovePackages);
		for (const FUnreachableObject& Item : ObjectsToRemove)
		{
			const FPackageId& PackageId = Item.PackageId;
			if (PackageId.IsValid())
			{
				RemovePackage(PackageId);
			}
		}
	}

	void RemovePackage(FPackageId PackageId)
	{
		UE_ASYNC_PACKAGEID_DEBUG(PackageId);

		FLoadedPackageRef PackageRef;
		bool bRemoved = Packages.RemoveAndCopyValue(PackageId, PackageRef);
		if (bRemoved)
		{
			for (int32 ObjectIndex : PackageRef.GetPublicExportObjectIndices())
			{
				if (ObjectIndex >= 0)
				{
					ObjectIndexToPublicExport.Remove(ObjectIndex);
				}
			}
		}
	}

	void RemovePublicExports(const FUnreachableObjects& ObjectsToRemove)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RemovePublicExports);

		FPackageId LastPackageId;
		FLoadedPackageRef* PackageRef = nullptr;
		for (const FUnreachableObject& Item : ObjectsToRemove)
		{
			int32 ObjectIndex = Item.ObjectIndex;
			check(ObjectIndex >= 0);

			FPublicExportKey PublicExportKey;
			if (ObjectIndexToPublicExport.RemoveAndCopyValue(ObjectIndex, PublicExportKey))
			{
				FPackageId PackageId = PublicExportKey.GetPackageId();
				if (PackageId != LastPackageId)
				{
					UE_ASYNC_PACKAGEID_DEBUG(PackageId);
					LastPackageId = PackageId;
					PackageRef = FindPackageRef(PackageId);
				}
				if (PackageRef)
				{
					PackageRef->RemovePublicExport(PublicExportKey.GetExportHash());
				}
			}
		}
	}

	void VerifyObjectForRemoval(UObject* GCObject)
	{
		int32 ObjectIndex = GUObjectArray.ObjectToIndex(GCObject);
		FPublicExportKey* PublicExportKey = ObjectIndexToPublicExport.Find(ObjectIndex);
		if (PublicExportKey)
		{
			FPackageId PackageId = PublicExportKey->GetPackageId();
			FLoadedPackageRef* PackageRef = FindPackageRef(PackageId);
			if (PackageRef)
			{
				UObject* ExistingObject = FindPublicExportObjectUnchecked(*PublicExportKey);
				UE_CLOG(!ExistingObject, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id %s:0x%llX is missing in GlobalImportStore. ")
					TEXT("Reason unknown. Double delete? Bug or hash collision?"),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					int(GCObject->GetInternalFlags()),
					*FormatPackageId(PackageId),
					PublicExportKey->GetExportHash());

				UE_CLOG(ExistingObject != GCObject, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id %s:0x%llX is not matching the object '%s' in GlobalImportStore. ")
					TEXT("Reason unknown. Overwritten after it was added? Bug or hash collision?"),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					int(GCObject->GetInternalFlags()),
					*FormatPackageId(PackageId),
					PublicExportKey->GetExportHash(),
					*ExistingObject->GetFullName());
			}
			else
			{
				UE_LOG(LogStreaming, Warning,
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The package for the serialized GC object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id %s:0x%llX is missing in GlobalImportStore. ")
					TEXT("Most likely this object has been moved into this package after it was loaded, while the original package is still around."),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					int(GCObject->GetInternalFlags()),
					*FormatPackageId(PackageId),
					PublicExportKey->GetExportHash());
			}
		}
	}

	void VerifyPackageForRemoval(FLoadedPackageRef& PackageRef)
	{
		UPackage* Package = PackageRef.GetPackage();
		FPackageId PackageId = Package->GetPackageId();

		UE_CLOG(PackageRef.GetRefCount() > 0, LogStreaming, Fatal,
			TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (0x%llX) - ")
			TEXT("Package removed while still being referenced, RefCount %d > 0."),
			*Package->GetName(),
			PackageId.ValueForDebugging(),
			PackageRef.GetRefCount());

		for (int32 ObjectIndex : PackageRef.GetPublicExportObjectIndices())
		{
			if (ObjectIndex >= 0)
			{
				UObject* Object = static_cast<UObject*>(GUObjectArray.IndexToObject(ObjectIndex)->Object);
				ensureMsgf(!Object->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport) || GUObjectArray.IsDisregardForGC(Object),
						TEXT("FGlobalImportStore::VerifyPackageForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id %s is probably still referenced by the loader."),
						*Object->GetFullName(),
						Object->GetFlags(),
						Object->GetInternalFlags(),
						*FormatPackageId(PackageId));

				FPublicExportKey* PublicExportKey = ObjectIndexToPublicExport.Find(ObjectIndex);
				UE_CLOG(!PublicExportKey, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (%s) - ")
					TEXT("The loaded public export object '%s' is missing in GlobalImportStore."),
					*Package->GetName(),
					*FormatPackageId(PackageId),
					*Object->GetFullName());

				FPackageId ObjectPackageId = PublicExportKey->GetPackageId();
				UE_CLOG(ObjectPackageId != PackageId, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (%s) - ")
					TEXT("The loaded public export object '%s' has a mismatching package id %s in GlobalImportStore."),
					*Package->GetName(),
					*FormatPackageId(PackageId),
					*Object->GetFullName(),
					*FormatPackageId(ObjectPackageId));

				VerifyObjectForRemoval(Object);
			}
		}
	}

	inline UObject* FindPublicExportObjectUnchecked(const FPublicExportKey& Key)
	{
		FLoadedPackageRef* PackageRef = FindPackageRef(Key.GetPackageId());
		if (!PackageRef)
		{
			return nullptr;
		}
		return PackageRef->GetPublicExport(Key.GetExportHash());
	}

	inline UObject* FindPublicExportObject(const FPublicExportKey& Key)
	{
		UObject* Object = FindPublicExportObjectUnchecked(Key);
		checkf(!Object || !Object->IsUnreachable(), TEXT("%s"), Object ? *Object->GetFullName() : TEXT("null"));
		return Object;
	}

	inline UObject* FindScriptImportObject(FPackageObjectIndex GlobalIndex)
	{
		check(GlobalIndex.IsScriptImport());
		UObject* Object = nullptr;
		Object = ScriptObjects.FindRef(GlobalIndex);
		return Object;
	}

	void StoreGlobalObject(FPackageId PackageId, uint64 ExportHash, UObject* Object)
	{
		check(PackageId.IsValid());
		check(ExportHash != 0);
		int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
		FPublicExportKey Key = FPublicExportKey::MakeKey(PackageId, ExportHash);

		UObject* ExistingObject = FindPublicExportObjectUnchecked(Key);
		if (ExistingObject && ExistingObject != Object)
		{
			int32 ExistingObjectIndex = GUObjectArray.ObjectToIndex(ExistingObject);

			UE_LOG(LogStreaming, Verbose,
				TEXT("FGlobalImportStore::StoreGlobalObject: The constructed public export object '%s' with index %d and id %s:0x%llX collides with object '%s' (ObjectFlags=%X, InternalObjectFlags=%x) with index %d in GlobalImportStore. ")
				TEXT("The existing object will be replaced since it or its package was most likely renamed after it was loaded the first time."),
				Object ? *Object->GetFullName() : TEXT("null"),
				ObjectIndex,
				*FormatPackageId(Key.GetPackageId()), Key.GetExportHash(),
				*ExistingObject->GetFullName(),
				ExistingObject->GetFlags(),
				int(ExistingObject->GetInternalFlags()),
				ExistingObjectIndex);

			ExistingObject->AtomicallyClearInternalFlags(EInternalObjectFlags::LoaderImport);
			ObjectIndexToPublicExport.Remove(ExistingObjectIndex);
		}

		FPublicExportKey* ExistingKey = ObjectIndexToPublicExport.Find(ObjectIndex);
		if (ExistingKey && *ExistingKey != Key)
		{
			UE_LOG(LogStreaming, Verbose,
				TEXT("FGlobalImportStore::StoreGlobalObject: The constructed public export object '%s' with index %d and id %s:0x%llX already exists in GlobalImportStore but with a different key %s:0x%llX.")
				TEXT("The existing object will be replaced since it or its package was most likely renamed after it was loaded the first time."),
				Object ? *Object->GetFullName() : TEXT("null"),
				ObjectIndex,
				*FormatPackageId(Key.GetPackageId()), Key.GetExportHash(),
				*FormatPackageId(ExistingKey->GetPackageId()), ExistingKey->GetExportHash());

			// Break the link with the old package now because otherwise, we wouldn't be able to remove the export
			// during GC since the ObjectIndex can only be linked to a single package ref.
			if (FLoadedPackageRef* ExistingPackageRef = FindPackageRef(ExistingKey->GetPackageId()))
			{
				ExistingPackageRef->RemovePublicExport(ExistingKey->GetExportHash());
			}

			ObjectIndexToPublicExport.Remove(ObjectIndex);
		}

		FLoadedPackageRef& PackageRef = FindPackageRefChecked(Key.GetPackageId());
		PackageRef.StorePublicExport(ExportHash, Object);
		ObjectIndexToPublicExport.Add(ObjectIndex, Key);
	}

	void FindAllScriptObjects(bool bVerifyOnly);
	void RegistrationComplete();

	void AddScriptObject(FStringView PackageName, FStringView Name, UObject* Object)
	{
		TStringBuilder<FName::StringBufferSize> FullName;
		FPathViews::Append(FullName, PackageName);
		FPathViews::Append(FullName, Name);
		FPackageObjectIndex GlobalImportIndex = FPackageObjectIndex::FromScriptPath(FullName);

#if WITH_EDITOR
		FPackageObjectIndex PackageGlobalImportIndex = FPackageObjectIndex::FromScriptPath(PackageName);
		ScriptObjects.Add(PackageGlobalImportIndex, Object->GetOutermost());
#endif
		ScriptObjects.Add(GlobalImportIndex, Object);

		TStringBuilder<FName::StringBufferSize> SubObjectName;
		ForEachObjectWithOuter(Object, [this, &SubObjectName](UObject* SubObject)
			{
				if (SubObject->HasAnyFlags(RF_Public))
				{
					SubObjectName.Reset();
					SubObject->GetPathName(nullptr, SubObjectName);
					FPackageObjectIndex SubObjectGlobalImportIndex = FPackageObjectIndex::FromScriptPath(SubObjectName);
					ScriptObjects.Add(SubObjectGlobalImportIndex, SubObject);
				}
			}, /* bIncludeNestedObjects*/ true);
	}
};

struct FAsyncPackageHeaderData: public FZenPackageHeader
{
	// Backed by allocation in FAsyncPackageData
	TArrayView<FPackageId> ImportedPackageIds;
	TArrayView<FAsyncPackage2*> ImportedAsyncPackagesView;
	TArrayView<FExportObject> ExportsView;
	TArrayView<FExportBundleEntry> ExportBundleEntriesCopyForPostLoad; // TODO: Can we use ConstructedObjects or Exports instead for posloading?
};

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
struct FAsyncPackageLinkerLoadHeaderData
{
	TArray<uint64> ImportedPublicExportHashes;
	TArray<FPackageObjectIndex> ImportMap;
	TArray<FExportMapEntry> ExportMap; // Note: Currently only using the public export hash field
};
#endif

struct FPackageImportStore
{
	FGlobalImportStore& GlobalImportStore;

	FPackageImportStore(FGlobalImportStore& InGlobalImportStore)
		: GlobalImportStore(InGlobalImportStore)
	{
	}

	inline bool IsValidLocalImportIndex(const TArrayView<const FPackageObjectIndex>& ImportMap, FPackageIndex LocalIndex)
	{
		check(ImportMap.Num() > 0);
		return LocalIndex.IsImport() && LocalIndex.ToImport() < ImportMap.Num();
	}

	inline UObject* FindOrGetImportObjectFromLocalIndex(const FAsyncPackageHeaderData& Header, FPackageIndex LocalIndex)
	{
		check(LocalIndex.IsImport());
		check(Header.ImportMap.Num() > 0);
		const int32 LocalImportIndex = LocalIndex.ToImport();
		check(LocalImportIndex < Header.ImportMap.Num());
		const FPackageObjectIndex GlobalIndex = Header.ImportMap[LocalIndex.ToImport()];
		return FindOrGetImportObject(Header, GlobalIndex);
	}

	inline UObject* FindOrGetImportObject(const FAsyncPackageHeaderData& Header, FPackageObjectIndex GlobalIndex)
	{
		check(GlobalIndex.IsImport());
		UObject* Object = nullptr;
		if (GlobalIndex.IsScriptImport())
		{
			Object = GlobalImportStore.FindScriptImportObject(GlobalIndex);
		}
		else if (GlobalIndex.IsPackageImport())
		{
			Object = GlobalImportStore.FindPublicExportObject(FPublicExportKey::FromPackageImport(GlobalIndex, Header.ImportedPackageIds, Header.ImportedPublicExportHashes));
#if WITH_EDITOR
			if (UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Object))
			{
				Object = Redirector->DestinationObject;
			}
#endif
		}
		else
		{
			check(GlobalIndex.IsNull());
		}
		return Object;
	}

	void GetUnresolvedCDOs(const FAsyncPackageHeaderData& Header, TArray<UClass*, TInlineAllocator<8>>& Classes)
	{
		for (const FPackageObjectIndex& Index : Header.ImportMap)
		{
			if (!Index.IsScriptImport())
			{
				continue;
			}

			UObject* Object = GlobalImportStore.FindScriptImportObject(Index);
			if (!Object)
			{
				continue;
			}

			UClass* Class = Cast<UClass>(Object);
			if (!Class)
			{
				continue;
			}

			// Filter out CDOs that are themselves classes,
			// like Default__BlueprintGeneratedClass of type UBlueprintGeneratedClass
			if (Class->HasAnyFlags(RF_ClassDefaultObject))
			{
				continue;
			}

			// Add dependency on any script CDO that has not been created and initialized yet
			UObject* CDO = Class->GetDefaultObject(/*bCreateIfNeeded*/ false);
			if (!CDO || CDO->HasAnyFlags(RF_NeedInitialization))
			{
				UE_LOG(LogStreaming, Log, TEXT("Package %s has a dependency on pending script CDO for '%s' (0x%llX)"),
					*Header.PackageName.ToString(), *Class->GetFullName(), Index.Value());
				Classes.AddUnique(Class);
			}
		}
	}

	inline void StoreGlobalObject(FPackageId PackageId, uint64 ExportHash, UObject* Object)
	{
		GlobalImportStore.StoreGlobalObject(PackageId, ExportHash, Object);
	}

public:
	FLoadedPackageRef& AddImportedPackageReference(FPackageId ImportedPackageId, FName PackageNameIfKnown)
	{
		return GlobalImportStore.AddPackageRef(ImportedPackageId, PackageNameIfKnown);
	}

	void AddPackageReference(const FAsyncPackageDesc2& Desc)
	{
		if (Desc.bCanBeImported)
		{
			FLoadedPackageRef& PackageRef = GlobalImportStore.AddPackageRef(Desc.UPackageId, Desc.UPackageName);
			PackageRef.ClearErrorFlags();
		}
	}

	void ReleaseImportedPackageReferences(const FAsyncPackageDesc2& Desc, const TArrayView<const FPackageId>& ImportedPackageIds)
	{
		for (const FPackageId& ImportedPackageId : ImportedPackageIds)
		{
			GlobalImportStore.ReleasePackageRef(ImportedPackageId, Desc.UPackageId);
		}
	}

	void ReleasePackageReference(const FAsyncPackageDesc2& Desc)
	{
		if (Desc.bCanBeImported)
		{
			GlobalImportStore.ReleasePackageRef(Desc.UPackageId);
		}
	}
};

class FExportArchive final : public FArchive
{
public:
	FExportArchive(const FIoBuffer& IoBuffer)
		: IoDispatcher(FIoDispatcher::Get())
	{
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
		ActiveFPLB = &InlineFPLB;
#endif
		ActiveFPLB->OriginalFastPathLoadBuffer = IoBuffer.Data();
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer;
		ActiveFPLB->EndFastPathLoadBuffer = IoBuffer.Data() + IoBuffer.DataSize();
	}

	~FExportArchive()
	{
	}
	void ExportBufferBegin(UObject* Object, uint64 InExportSerialOffset, uint64 InExportSerialSize)
	{
		CurrentExport = Object;
		ExportSerialOffset = HeaderData->PackageSummary->HeaderSize + InExportSerialOffset;
		ExportSerialSize = InExportSerialSize;
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + ExportSerialOffset;
	}

	void ExportBufferEnd()
	{
		CurrentExport = nullptr;
		ExportSerialOffset = 0;
		ExportSerialSize = 0;
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer;
	}

	void CheckBufferPosition(const TCHAR* Text, uint64 Offset = 0)
	{
#if DO_CHECK
		const uint64 BufferPosition = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer) + Offset;
		const bool bIsInsideExportBuffer =
			(ExportSerialOffset <= BufferPosition) && (BufferPosition <= ExportSerialOffset + ExportSerialSize);

		UE_ASYNC_PACKAGE_CLOG(
			!bIsInsideExportBuffer,
			Error, *PackageDesc, TEXT("FExportArchive::InvalidPosition"),
			TEXT("%s: Position %llu is outside of the current export buffer (%lld,%lld)."),
			Text,
			BufferPosition,
			ExportSerialOffset, ExportSerialOffset + ExportSerialSize);
#endif
	}

	void Skip(int64 InBytes)
	{
		CheckBufferPosition(TEXT("InvalidSkip"), InBytes);
		ActiveFPLB->StartFastPathLoadBuffer += InBytes;
	}

	virtual int64 TotalSize() override
	{
		if (bExportsCookedToSeparateArchive)
		{
			return ExportSerialSize;
		}
		else
		{
			int64 CookedFileSize = (ActiveFPLB->EndFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
			CookedFileSize -= HeaderData->PackageSummary->HeaderSize;
			CookedFileSize += HeaderData->CookedHeaderSize;
			return CookedFileSize;
		}
	}

	virtual int64 Tell() override
	{
		if (bExportsCookedToSeparateArchive)
		{
			return (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer) - ExportSerialOffset;
		}
		else
		{
			int64 CookedFilePosition = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
			CookedFilePosition -= HeaderData->PackageSummary->HeaderSize;
			CookedFilePosition += HeaderData->CookedHeaderSize;
			return CookedFilePosition;
		}
	}

	virtual void Seek(int64 Position) override
	{
		if (bExportsCookedToSeparateArchive)
		{
			ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + ExportSerialOffset + Position;
		}
		else
		{
			uint64 BufferPosition = (uint64)Position;
			BufferPosition -= HeaderData->CookedHeaderSize;
			BufferPosition += HeaderData->PackageSummary->HeaderSize;
			ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + BufferPosition;
		}
		CheckBufferPosition(TEXT("InvalidSeek"));
	}

	virtual void Serialize(void* Data, int64 Length) override
	{
		if (!Length || ArIsError)
		{
			return;
		}
		CheckBufferPosition(TEXT("InvalidSerialize"), (uint64)Length);
		FMemory::Memcpy(Data, ActiveFPLB->StartFastPathLoadBuffer, Length);
		ActiveFPLB->StartFastPathLoadBuffer += Length;
	}

	virtual FString GetArchiveName() const override
	{
		return PackageDesc ? PackageDesc->UPackageName.ToString() : TEXT("FExportArchive");
	}

	void UsingCustomVersion(const FGuid& Key) override {};
	using FArchive::operator<<; // For visibility of the overloads we don't override

	/** FExportArchive will be created on the stack so we do not want BulkData objects caching references to it. */
	virtual FArchive* GetCacheableArchive()
	{
		return nullptr;
	}

	//~ Begin FArchive::FArchiveUObject Interface
	virtual FArchive& operator<<(FObjectPtr& Value) override { return FArchiveUObject::SerializeObjectPtr(*this, Value); }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override { return FArchiveUObject::SerializeWeakObjectPtr(*this, Value); }
	//~ End FArchive::FArchiveUObject Interface

	//~ Begin FArchive::FLinkerLoad Interface
	UObject* GetArchetypeFromLoader(const UObject* Obj) { return TemplateForGetArchetypeFromLoader; }

	virtual bool AttachExternalReadDependency(FExternalReadCallback& ReadCallback) override
	{
		ExternalReadDependencies->Add(ReadCallback);
		return true;
	}

	FORCENOINLINE void HandleBadExportIndex(int32 ExportIndex, UObject*& Object)
	{
		UE_ASYNC_PACKAGE_LOG(Fatal, *PackageDesc, TEXT("ObjectSerializationError"),
			TEXT("%s: Bad export index %d/%d."),
			CurrentExport ? *CurrentExport->GetFullName() : TEXT("null"), ExportIndex, HeaderData->ExportsView.Num());

		Object = nullptr;
	}

	FORCENOINLINE void HandleBadImportIndex(int32 ImportIndex, UObject*& Object)
	{
		UE_ASYNC_PACKAGE_LOG(Fatal, *PackageDesc, TEXT("ObjectSerializationError"),
			TEXT("%s: Bad import index %d/%d."),
			CurrentExport ? *CurrentExport->GetFullName() : TEXT("null"), ImportIndex, HeaderData->ImportMap.Num());
		Object = nullptr;
	}

	virtual FArchive& operator<<( UObject*& Object ) override
	{
		FPackageIndex Index;
		FArchive& Ar = *this;
		Ar << Index;

		if (Index.IsNull())
		{
			Object = nullptr;
		}
		else if (Index.IsExport())
		{
			const int32 ExportIndex = Index.ToExport();
			if (ExportIndex < HeaderData->ExportsView.Num())
			{
				Object = HeaderData->ExportsView[ExportIndex].Object;

#if ALT2_LOG_VERBOSE
				const FExportMapEntry& Export = HeaderData->ExportMap[ExportIndex];
				FName ObjectName = HeaderData->NameMap.GetName(Export.ObjectName);
				UE_ASYNC_PACKAGE_CLOG_VERBOSE(!Object, VeryVerbose, *PackageDesc,
					TEXT("FExportArchive: Object"), TEXT("Export %s at index %d is null."),
					*ObjectName.ToString(), 
					ExportIndex);
#endif
			}
			else
			{
				HandleBadExportIndex(ExportIndex, Object);
			}
		}
		else
		{
			if (ImportStore->IsValidLocalImportIndex(HeaderData->ImportMap, Index))
			{
				Object = ImportStore->FindOrGetImportObjectFromLocalIndex(*HeaderData, Index);

				UE_ASYNC_PACKAGE_CLOG_VERBOSE(!Object, Log, *PackageDesc,
					TEXT("FExportArchive: Object"), TEXT("Import index %d is null"),
					Index.ToImport());
			}
			else
			{
				HandleBadImportIndex(Index.ToImport(), Object);
			}
		}
		return *this;
	}

	inline virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
	{
		FArchive& Ar = *this;
		FUniqueObjectGuid ID;
		Ar << ID;
		LazyObjectPtr = ID;
		return Ar;
	}

	inline virtual FArchive& operator<<(FSoftObjectPtr& Value) override
	{
		FArchive& Ar = *this;
		FSoftObjectPath ID;
		ID.Serialize(Ar);
		Value = ID;
		return Ar;
	}

	inline virtual FArchive& operator<<(FSoftObjectPath& Value) override
	{
		FArchive& Ar = FArchiveUObject::SerializeSoftObjectPath(*this, Value);
		FixupSoftObjectPathForInstancedPackage(Value);
		return Ar;
	}

	FORCENOINLINE void HandleBadNameIndex(int32 NameIndex, FName& Name)
	{
		UE_ASYNC_PACKAGE_LOG(Fatal, *PackageDesc, TEXT("ObjectSerializationError"),
			TEXT("%s: Bad name index %d/%d."),
			CurrentExport ? *CurrentExport->GetFullName() : TEXT("null"), NameIndex, HeaderData->NameMap.Num());
		Name = FName();
		SetCriticalError();
	}

	inline virtual FArchive& operator<<(FName& Name) override
	{
		FArchive& Ar = *this;
		uint32 NameIndex;
		Ar << NameIndex;
		uint32 Number = 0;
		Ar << Number;

		FMappedName MappedName = FMappedName::Create(NameIndex, Number, FMappedName::EType::Package);
		if (!HeaderData->NameMap.TryGetName(MappedName, Name))
		{
			HandleBadNameIndex(NameIndex, Name);
		}
		return *this;
	}

	inline virtual bool SerializeBulkData(FBulkData& BulkData, const FBulkDataSerializationParams& Params) override
	{
		const FPackageId& PackageId = PackageDesc->PackageIdToLoad;
		const uint16 ChunkIndex = bIsOptionalSegment ? 1 : 0; 

		UE::BulkData::Private::FBulkMetaData& Meta = BulkData.BulkMeta;
		int64 DuplicateSerialOffset = -1;
		SerializeBulkMeta(Meta, DuplicateSerialOffset, Params.ElementSize);

		const bool bIsInline = Meta.HasAnyFlags(BULKDATA_PayloadAtEndOfFile) == false;
		if (bIsInline)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			if (int64 PayloadSize = Meta.GetSize(); PayloadSize > 0 && Meta.HasAnyFlags(BULKDATA_Unused) == false)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			{
				// Set the offset from the beginning of the I/O chunk in order to make inline bulk data reloadable
				const int64 ExportBundleChunkOffset = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
				Meta.SetOffset(ExportBundleChunkOffset);
				BulkData.BulkChunkId = CreateIoChunkId(PackageId.Value(), ChunkIndex, EIoChunkType::ExportBundleData);
				Serialize(BulkData.ReallocateData(PayloadSize), PayloadSize);
			}
		}
		else
		{
			const EIoChunkType ChunkType = Meta.HasAnyFlags(BULKDATA_OptionalPayload) ? EIoChunkType::OptionalBulkData : EIoChunkType::BulkData;
			BulkData.BulkChunkId = CreateIoChunkId(PackageId.Value(), ChunkIndex, ChunkType);

			if (Meta.HasAnyFlags(BULKDATA_DuplicateNonOptionalPayload))
			{
				const FIoChunkId OptionalChunkId = CreateIoChunkId(PackageId.Value(), ChunkIndex, EIoChunkType::OptionalBulkData);
				if (IoDispatcher.DoesChunkExist(OptionalChunkId))
				{
					BulkData.BulkChunkId = OptionalChunkId;
					Meta.ClearFlags(BULKDATA_DuplicateNonOptionalPayload);
					Meta.AddFlags(BULKDATA_OptionalPayload);
					Meta.SetOffset(DuplicateSerialOffset);
				}
			}
			else if (Meta.HasAnyFlags(BULKDATA_MemoryMappedPayload))
			{
				BulkData.BulkChunkId = CreateIoChunkId(PackageId.Value(), ChunkIndex, EIoChunkType::MemoryMappedBulkData);
				if (Params.bAttemptMemoryMapping)
				{
					TIoStatusOr<FIoMappedRegion> Status = IoDispatcher.OpenMapped(BulkData.BulkChunkId, FIoReadOptions(Meta.GetOffset(), Meta.GetSize()));

					if (Status.IsOk())
					{
						FIoMappedRegion Mapping = Status.ConsumeValueOrDie(); 
						BulkData.DataAllocation.SetMemoryMappedData(&BulkData, Mapping.MappedFileHandle, Mapping.MappedFileRegion);
					}
					else
					{
						UE_LOG(LogSerialization, Warning, TEXT("Memory map bulk data from chunk '%s', offset '%lld', size '%lld' FAILED"),
							*LexToString(BulkData.BulkChunkId), Meta.GetOffset(), Meta.GetSize());

						BulkData.ForceBulkDataResident();
					}
				}
			}
		}

		return true;
	}
	//~ End FArchive::FLinkerLoad Interface

private:
	inline void SerializeBulkMeta(UE::BulkData::Private::FBulkMetaData& Meta, int64& DuplicateSerialOffset, int32 ElementSize)
	{
		using namespace UE::BulkData::Private;
		FArchive& Ar = *this;

		if (UNLIKELY(HeaderData->BulkDataMap.IsEmpty()))
		{
			FBulkMetaData::FromSerialized(Ar, ElementSize,  Meta, DuplicateSerialOffset);
		}
		else
		{
			int32 EntryIndex = INDEX_NONE;
			Ar << EntryIndex;
			const FBulkDataMapEntry& Entry = HeaderData->BulkDataMap[EntryIndex];
			Meta.SetFlags(static_cast<EBulkDataFlags>(Entry.Flags));
			Meta.SetOffset(Entry.SerialOffset);
			Meta.SetSize(Entry.SerialSize);

#if !USE_RUNTIME_BULKDATA
			// If the payload was compressed at package level then we will not be able to decompress it properly as that requires
			// us to know the compressed size (SizeOnDisk) which we do not keep track of when the package is stored by the IoDispatcher.
			// The BULKDATA_SerializeCompressed flag is removed during cooking/staging so the flag should never be set at this point,
			// the assert is just a paranoid safety check.
			checkf(Meta.HasAnyFlags(BULKDATA_SerializeCompressed) == false, TEXT("Package level compression is not supported by the IoDispatcher: '%s'"), *PackageDesc->UPackageName.ToString());

			// Since we know that the payload is not compressed there is no difference between the in memory size and the size of disk
			Meta.SetSizeOnDisk(Entry.SerialSize);
#endif //!USE_RUNTIME_BULKDATA

			DuplicateSerialOffset = Entry.DuplicateSerialOffset;
		}

		Meta.AddFlags(static_cast<EBulkDataFlags>(BULKDATA_UsesIoDispatcher | BULKDATA_LazyLoadable));
#if WITH_EDITOR
		if (GIsEditor)
		{
			Meta.ClearFlags(BULKDATA_SingleUse);
		}
#endif
	}

	friend FAsyncPackage2;
	FIoDispatcher& IoDispatcher;
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
	FArchive::FFastPathLoadBuffer InlineFPLB;
	FArchive::FFastPathLoadBuffer* ActiveFPLB;
#endif

	UObject* TemplateForGetArchetypeFromLoader = nullptr;

	FAsyncPackageDesc2* PackageDesc = nullptr;
	FPackageImportStore* ImportStore = nullptr;
	TArray<FExternalReadCallback>* ExternalReadDependencies;
	const FAsyncPackageHeaderData* HeaderData = nullptr;
	const FLinkerInstancingContext* InstanceContext = nullptr;
	UObject* CurrentExport = nullptr;
	uint64 ExportSerialOffset = 0;
	uint64 ExportSerialSize = 0;
	bool bIsOptionalSegment = false;
	bool bExportsCookedToSeparateArchive = false;

	void FixupSoftObjectPathForInstancedPackage(FSoftObjectPath& InOutSoftObjectPath)
	{
		if (InstanceContext)
		{
			InstanceContext->FixupSoftObjectPath(InOutSoftObjectPath);
		}
	}
};

enum class EAsyncPackageLoadingState2 : uint8
{
	NewPackage,
	WaitingForIo,
	ProcessPackageSummary,
	WaitingForDependencies,
	DependenciesReady,
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
// This is the path taken by LinkerLoad packages
	CreateLinkerLoadExports,
	WaitingForLinkerLoadDependencies,
	ResolveLinkerLoadImports,
	PreloadLinkerLoadExports,
#endif
// This is the path taken by Runtime/cooked packages
	ProcessExportBundles,
// Both LinkerLoad and Cooked packages should converge at this point
	WaitingForExternalReads,
	ExportsDone,
	PostLoad,
	DeferredPostLoad,
	DeferredPostLoadDone,
	Finalize,
	PostLoadInstances,
	CreateClusters,
	Complete,
	DeferredDelete,
};

const TCHAR* LexToString(EAsyncPackageLoadingState2 AsyncPackageLoadingState)
{
	switch(AsyncPackageLoadingState)
	{
	case EAsyncPackageLoadingState2::NewPackage: return TEXT("NewPackage");
	case EAsyncPackageLoadingState2::WaitingForIo: return TEXT("WaitingForIo");
	case EAsyncPackageLoadingState2::ProcessPackageSummary: return TEXT("ProcessPackageSummary");
	case EAsyncPackageLoadingState2::WaitingForDependencies: return TEXT("WaitingForDependencies");
	case EAsyncPackageLoadingState2::DependenciesReady: return TEXT("DependenciesReady");
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	case EAsyncPackageLoadingState2::CreateLinkerLoadExports: return TEXT("CreateLinkerLoadExports");
	case EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies: return TEXT("WaitingForLinkerLoadDependencies");
	case EAsyncPackageLoadingState2::ResolveLinkerLoadImports: return TEXT("ResolveLinkerLoadImports");
	case EAsyncPackageLoadingState2::PreloadLinkerLoadExports: return TEXT("PreloadLinkerLoadExports");
#endif
	case EAsyncPackageLoadingState2::ProcessExportBundles: return TEXT("ProcessExportBundles");
	case EAsyncPackageLoadingState2::WaitingForExternalReads: return TEXT("WaitingForExternalReads");
	case EAsyncPackageLoadingState2::ExportsDone: return TEXT("ExportsDone");
	case EAsyncPackageLoadingState2::PostLoad: return TEXT("PostLoad");
	case EAsyncPackageLoadingState2::DeferredPostLoad: return TEXT("DeferredPostLoad");
	case EAsyncPackageLoadingState2::DeferredPostLoadDone: return TEXT("DeferredPostLoadDone");
	case EAsyncPackageLoadingState2::Finalize: return TEXT("Finalize");
	case EAsyncPackageLoadingState2::PostLoadInstances: return TEXT("PostLoadInstances");
	case EAsyncPackageLoadingState2::CreateClusters: return TEXT("CreateClusters");
	case EAsyncPackageLoadingState2::Complete: return TEXT("Complete");
	case EAsyncPackageLoadingState2::DeferredDelete: return TEXT("DeferredDelete");
	default:
		checkNoEntry();
		return TEXT("Unknown");
	}
};

class FEventLoadGraphAllocator;
struct FAsyncLoadEventSpec;
struct FAsyncLoadingThreadState2;
class FAsyncLoadEventQueue2;

enum class EEventLoadNodeExecutionResult : uint8
{
	Timeout,
	Complete,
};

/** [EDL] Event Load Node */
class FEventLoadNode2
{
public:
	FEventLoadNode2(const FAsyncLoadEventSpec* InSpec, FAsyncPackage2* InPackage, int32 InImportOrExportIndex, int32 InBarrierCount);
	void DependsOn(FEventLoadNode2* Other);
	void AddBarrier();
	void AddBarrier(int32 Count);
	void ReleaseBarrier(FAsyncLoadingThreadState2* ThreadState);
	EEventLoadNodeExecutionResult Execute(FAsyncLoadingThreadState2& ThreadState);

	FAsyncPackage2* GetPackage() const
	{
		return Package;
	}

	uint64 GetSyncLoadContextId() const;

private:
	friend class FAsyncLoadEventQueue2;
	friend TIoPriorityQueue<FEventLoadNode2>;

	FEventLoadNode2()
	{

	}

	void ProcessDependencies(FAsyncLoadingThreadState2& ThreadState);
	void Fire(FAsyncLoadingThreadState2* ThreadState);

	const FAsyncLoadEventSpec* Spec = nullptr;
	FAsyncPackage2* Package = nullptr;
	union
	{
		FEventLoadNode2* SingleDependent;
		FEventLoadNode2** MultipleDependents;
	};
	FEventLoadNode2* Prev = nullptr;
	FEventLoadNode2* Next = nullptr;
	uint32 DependenciesCount = 0;
	uint32 DependenciesCapacity = 0;
	int32 Priority = 0;
	int32 ImportOrExportIndex = -1;
	std::atomic<int32> BarrierCount { 0 };
	enum EQueueStatus
	{
		QueueStatus_None = 0,
		QueueStatus_Local = 1,
		QueueStatus_External = 2
	};
	uint8 QueueStatus = QueueStatus_None;
	std::atomic<bool> bIsUpdatingDependencies { false };
	std::atomic<bool> bIsDone { false };
};

class FAsyncLoadEventGraphAllocator
{
public:
	FEventLoadNode2** AllocArcs(uint32 Count)
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(AllocArcs);
		SIZE_T Size = Count * sizeof(FEventLoadNode2*);
		TotalArcCount += Count;
		TotalAllocated += Size;
		return reinterpret_cast<FEventLoadNode2**>(FMemory::Malloc(Size));
	}

	void FreeArcs(FEventLoadNode2** Arcs, uint32 Count)
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(FreeArcs);
		FMemory::Free(Arcs);
		SIZE_T Size = Count * sizeof(FEventLoadNode2*);
		TotalAllocated -= Size;
		TotalArcCount -= Count;
	}

	TAtomic<int64> TotalArcCount { 0 };
	TAtomic<int64> TotalAllocated { 0 };
};

/** Queue based on DepletableMpscQueue.h 
	Producers will add nodes to the linked list defined by Sentinel->Tail
	Nodes will be executed from the linked list defined by LocalHead->LocalTail
	All the nodes from the producer queue will be moved to the local queue when needed */
class FAsyncLoadEventQueue2
{
public:
	FAsyncLoadEventQueue2();
	~FAsyncLoadEventQueue2();

	void SetZenaphore(FZenaphore* InZenaphore)
	{
		Zenaphore = InZenaphore;
	}

	void SetWakeEvent(UE::FManualResetEvent* InEvent)
	{
		WakeEvent = InEvent;
	}

	void SetOwnerThread(const FAsyncLoadingThreadState2* InOwnerThread)
	{
		OwnerThread = InOwnerThread;
	}

	bool PopAndExecute(FAsyncLoadingThreadState2& ThreadState);
	void Push(FAsyncLoadingThreadState2* ThreadState, FEventLoadNode2* Node);
	bool ExecuteSyncLoadEvents(FAsyncLoadingThreadState2& ThreadState);
	void UpdatePackagePriority(FAsyncPackage2* Package);

private:
	void PushLocal(FEventLoadNode2* Node);
	void PushExternal(FEventLoadNode2* Node);
	bool GetMaxPriorityInExternalQueue(int32& OutMaxPriority)
	{
		int64 StateValue = ExternalQueueState.load();
		if (StateValue == MIN_int64)
		{
			return false;
		}
		else
		{
			OutMaxPriority = static_cast<int32>(StateValue);
			return true;
		}
	}
	void UpdateExternalQueueState()
	{
		if (ExternalQueue.IsEmpty())
		{
			ExternalQueueState.store(MIN_int64);
		}
		else
		{
			ExternalQueueState.store(ExternalQueue.GetMaxPriority());
		}
	}

	const FAsyncLoadingThreadState2* OwnerThread = nullptr;
	FZenaphore* Zenaphore = nullptr;
	UE::FManualResetEvent* WakeEvent = nullptr;
	TIoPriorityQueue<FEventLoadNode2> LocalQueue;
	FCriticalSection ExternalCritical;
	TIoPriorityQueue<FEventLoadNode2> ExternalQueue;
	std::atomic<int64> ExternalQueueState = MIN_int64;
	FEventLoadNode2* TimedOutEventNode = nullptr;
	int32 ExecuteSyncLoadEventsCallCounter = 0;
};

struct FAsyncLoadEventSpec
{
	typedef EEventLoadNodeExecutionResult(*FAsyncLoadEventFunc)(FAsyncLoadingThreadState2&, FAsyncPackage2*, int32);
	FAsyncLoadEventFunc Func = nullptr;
	FAsyncLoadEventQueue2* EventQueue = nullptr;
	bool bExecuteImmediately = false;
};

class FAsyncLoadingSyncLoadContext
{
public:
	FAsyncLoadingSyncLoadContext(TConstArrayView<int32> InRequestIDs)
		: RequestIDs(InRequestIDs)
	{
		RequestedPackages.AddZeroed(RequestIDs.Num());
		ContextId = NextContextId++;
		if (NextContextId == 0)
		{
			NextContextId = 1;
		}
	}

	~FAsyncLoadingSyncLoadContext()
	{
	}

	void AddRef()
	{
		++RefCount;
	}

	void ReleaseRef()
	{
		int32 NewRefCount = --RefCount;
		check(NewRefCount >= 0);
		if (NewRefCount == 0)
		{
			delete this;
		}
	}

	uint64 ContextId;
	TArray<int32, TInlineAllocator<4>> RequestIDs;
	TArray<FAsyncPackage2*, TInlineAllocator<4>> RequestedPackages;
	FAsyncPackage2* RequestingPackage = nullptr;
	std::atomic<bool> bHasFoundRequestedPackages { false };

private:
	std::atomic<int32> RefCount = 1;

	static std::atomic<uint64> NextContextId;
};

std::atomic<uint64> FAsyncLoadingSyncLoadContext::NextContextId { 1 };

struct FAsyncLoadingThreadState2
{
	struct FTimeLimitScope
	{
		bool bUseTimeLimit = false;
		double TimeLimit = 0.0;
		double StartTime = 0.0;

		FTimeLimitScope(bool bInUseTimeLimit, double InTimeLimit)
		{
			FAsyncLoadingThreadState2* ThreadState = FAsyncLoadingThreadState2::Get();

			bUseTimeLimit = ThreadState->bUseTimeLimit;
			TimeLimit = ThreadState->TimeLimit;
			StartTime = ThreadState->StartTime;

			ThreadState->bUseTimeLimit = bInUseTimeLimit;
			ThreadState->TimeLimit = InTimeLimit;
			ThreadState->StartTime = FPlatformTime::Seconds();
		}
		
		~FTimeLimitScope()
		{
			FAsyncLoadingThreadState2* ThreadState = FAsyncLoadingThreadState2::Get();

			ThreadState->bUseTimeLimit = bUseTimeLimit;
			ThreadState->TimeLimit = TimeLimit;
			ThreadState->StartTime = StartTime;
		}
	};

	static void Set(FAsyncLoadingThreadState2* State)
	{
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));
		check(!FPlatformTLS::GetTlsValue(TlsSlot));
		FPlatformTLS::SetTlsValue(TlsSlot, State);
	}

	static FAsyncLoadingThreadState2* Get()
	{
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));
		return static_cast<FAsyncLoadingThreadState2*>(FPlatformTLS::GetTlsValue(TlsSlot));
	}

	FAsyncLoadingThreadState2(FAsyncLoadEventGraphAllocator& InGraphAllocator, FIoDispatcher& InIoDispatcher)
		: GraphAllocator(InGraphAllocator)
	{
	}

	bool IsTimeLimitExceeded(const TCHAR* InLastTypeOfWorkPerformed = nullptr, UObject* InLastObjectWorkWasPerformedOn = nullptr)
	{
		bool bTimeLimitExceeded = false;

		if (bUseTimeLimit)
		{
			double CurrentTime = FPlatformTime::Seconds();
			bTimeLimitExceeded = CurrentTime - StartTime > TimeLimit;

			if (bTimeLimitExceeded && GWarnIfTimeLimitExceeded)
			{
				IsTimeLimitExceededPrint(StartTime, CurrentTime, LastTestTime, TimeLimit, InLastTypeOfWorkPerformed, InLastObjectWorkWasPerformedOn);
			}

			LastTestTime = CurrentTime;
		}

		if (!bTimeLimitExceeded)
		{
			bTimeLimitExceeded = IsGarbageCollectionWaiting();
			UE_CLOG(bTimeLimitExceeded, LogStreaming, Verbose, TEXT("Timing out async loading due to Garbage Collection request"));
		}

		return bTimeLimitExceeded;
	}

	bool UseTimeLimit()
	{
		return bUseTimeLimit;
	}

	FAsyncLoadEventGraphAllocator& GraphAllocator;
	TArray<FEventLoadNode2*> NodesToFire;
	TArray<FEventLoadNode2*> CurrentlyExecutingEventNodeStack;
	TArray<FAsyncLoadingSyncLoadContext*> SyncLoadContextStack;
	TArray<FAsyncPackage2*> PackagesOnStack;
	TSpscQueue<FAsyncLoadingSyncLoadContext*> SyncLoadContextsCreatedOnGameThread;
	TSpscQueue<FAsyncPackage2*> PackagesToReprioritize;
	bool bIsAsyncLoadingThread = false;
	bool bCanAccessAsyncLoadingThreadData = true;
	bool bShouldFireNodes = true;
	bool bUseTimeLimit = false;
	double TimeLimit = 0.0;
	double StartTime = 0.0;
	double LastTestTime = -1.0;
	static uint32 TlsSlot;
};

uint32 FAsyncLoadingThreadState2::TlsSlot;

/**
 * Event node.
 */
enum EEventLoadNode2 : uint8
{
	Package_ProcessSummary,
	Package_DependenciesReady,
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	Package_CreateLinkerLoadExports,
	Package_ResolveLinkerLoadImports,
	Package_PreloadLinkerLoadExports,
#endif
	Package_ExportsSerialized,
	Package_NumPhases,

	ExportBundle_Process = 0,
	ExportBundle_PostLoad,
	ExportBundle_DeferredPostLoad,
	ExportBundle_NumPhases,
};

struct FAsyncPackageData
{
	uint8* MemoryBuffer0 = nullptr;
	uint8* MemoryBuffer1 = nullptr;
	TArrayView<FExportObject> Exports;
	TArrayView<FAsyncPackage2*> ImportedAsyncPackages;
	TArrayView<FEventLoadNode2> ExportBundleNodes;
	TArrayView<const FSHAHash> ShaderMapHashes;
	int32 TotalExportBundleCount = 0;
};

struct FAsyncPackageSerializationState
{
	FIoRequest IoRequest;

	void ReleaseIoRequest()
	{
		IoRequest.Release();
	}
};

#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
class FLinkerLoadArchive2 final
	: public FArchive
{
public:
	FLinkerLoadArchive2(const FPackagePath& InPackagePath)
	{
		FOpenAsyncPackageResult UassetOpenResult = IPackageResourceManager::Get().OpenAsyncReadPackage(InPackagePath, EPackageSegment::Header);
		UassetFileHandle = UassetOpenResult.Handle.Release();
		check(UassetFileHandle); // OpenAsyncReadPackage guarantees a non-null return value; the handle will fail to read later if the path does not exist
		if (UassetOpenResult.Format != EPackageFormat::Binary)
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Only binary assets are supported")); // TODO
			SetError();
			return;
		}
		bNeedsEngineVersionChecks = UassetOpenResult.bNeedsEngineVersionChecks;

		if (FPlatformProperties::RequiresCookedData())
		{
			FOpenAsyncPackageResult UexpOpenResult = IPackageResourceManager::Get().OpenAsyncReadPackage(InPackagePath, EPackageSegment::Exports);
			UexpFileHandle = UexpOpenResult.Handle.Release();
			check(UexpFileHandle); // OpenAsyncReadPackage guarantees a non-null return value; the handle will fail to read later if the path does not exist
			if (UexpOpenResult.Format != EPackageFormat::Binary)
			{
				UE_LOG(LogStreaming, Fatal, TEXT("Only binary assets are supported")); // TODO
				SetError();
				return;
			}
		}
		else
		{
			UexpSize = 0;
		}
	}

	virtual ~FLinkerLoadArchive2()
	{
		WaitForRequests();
		delete UassetFileHandle;
		delete UexpFileHandle;
	}

	bool NeedsEngineVersionChecks() const
	{
		return bNeedsEngineVersionChecks;
	}

	void BeginRead(FEventLoadNode2* InDependentNode)
	{
		check(PendingSizeRequests == 0);
		check(PendingReadRequests == 0);
		check(!DependentNode);
		if (UexpFileHandle)
		{
			PendingSizeRequests = 2;
			PendingReadRequests = 2;
		}
		else
		{
			PendingSizeRequests = 1;
			PendingReadRequests = 1;
		}
		DependentNode = InDependentNode;
		StartSizeRequests();
	}

	virtual int64 TotalSize() override
	{
		check(bDone);
		return ActiveFPLB->EndFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer;
	}

	virtual int64 Tell() override
	{
		check(bDone);
		return ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer;
	}

	virtual void Seek(int64 InPos) override
	{
		check(bDone);
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + InPos;
		check(ActiveFPLB->StartFastPathLoadBuffer <= ActiveFPLB->EndFastPathLoadBuffer);
	}

	virtual void Serialize(void* Data, int64 Length) override
	{
		check(bDone);
		if (!Length || IsError())
		{
			return;
		}
		check(ActiveFPLB->StartFastPathLoadBuffer + Length <= ActiveFPLB->EndFastPathLoadBuffer);
		FMemory::Memcpy(Data, ActiveFPLB->StartFastPathLoadBuffer, Length);
		ActiveFPLB->StartFastPathLoadBuffer += Length;
	}

	/*virtual bool Close() override;
	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize) override;
	virtual void FlushCache() override;*/

private:
	void StartSizeRequests()
	{
		FAsyncFileCallBack UassetSizeRequestCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
		{
			if (!bWasCancelled)
			{
				UassetSize = Request->GetSizeResults();
			}
			if (--PendingSizeRequests == 0)
			{
				StartReadRequests();
			}
		};
		UassetSizeRequest = UassetFileHandle->SizeRequest(&UassetSizeRequestCallbackFunction);
		if (UexpFileHandle)
		{
			FAsyncFileCallBack UexpSizeRequestCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
			{
				if (!bWasCancelled)
				{
					UexpSize = Request->GetSizeResults();
				}
				if (--PendingSizeRequests == 0)
				{
					StartReadRequests();
				}
			};
			UexpSizeRequest = UexpFileHandle->SizeRequest(&UexpSizeRequestCallbackFunction);
		}
	}

	void StartReadRequests()
	{
		FAsyncFileCallBack ReadRequestCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
		{
			if (bWasCancelled)
			{
				bFailed = true;
			}
			if (--PendingReadRequests == 0)
			{
				FinishedReading();
			}
		};
		if (UassetSize <= 0 || (UexpFileHandle && UexpSize <= 0))
		{
			SetError();
			FinishedReading();
			return;
		}
		IoBuffer = FIoBuffer(UassetSize + UexpSize);
		UassetReadRequest = UassetFileHandle->ReadRequest(0, UassetSize, AIOP_Normal, &ReadRequestCallbackFunction, IoBuffer.Data());
		if (UexpFileHandle)
		{
			UexpReadRequest = UexpFileHandle->ReadRequest(0, UexpSize, AIOP_Normal, &ReadRequestCallbackFunction, IoBuffer.Data() + UassetSize);
		}
	}

	void FinishedReading()
	{
		ActiveFPLB->OriginalFastPathLoadBuffer = IoBuffer.Data();
		ActiveFPLB->StartFastPathLoadBuffer = IoBuffer.Data();
		ActiveFPLB->EndFastPathLoadBuffer = IoBuffer.Data() + IoBuffer.DataSize();
		bDone = true;
		DependentNode->ReleaseBarrier();
		DependentNode = nullptr;
	}

	void WaitForRequests()
	{
		if (UassetSizeRequest)
		{
			UassetSizeRequest->WaitCompletion();
			delete UassetSizeRequest;
			UassetSizeRequest = nullptr;
		}
		if (UexpSizeRequest)
		{
			UexpSizeRequest->WaitCompletion();
			delete UexpSizeRequest;
			UexpSizeRequest = nullptr;
		}
		if (UassetReadRequest)
		{
			UassetReadRequest->WaitCompletion();
			delete UassetReadRequest;
			UassetReadRequest = nullptr;
		}
		if (UexpReadRequest)
		{
			UexpReadRequest->WaitCompletion();
			delete UexpReadRequest;
			UexpReadRequest = nullptr;
		}
	}

#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
	FArchive::FFastPathLoadBuffer InlineFPLB;
	FArchive::FFastPathLoadBuffer* ActiveFPLB = &InlineFPLB;
#endif
	FEventLoadNode2* DependentNode = nullptr;
	FIoBuffer IoBuffer;
	int64 Offset = 0;
	IAsyncReadFileHandle* UassetFileHandle = nullptr;
	IAsyncReadFileHandle* UexpFileHandle = nullptr;
	IAsyncReadRequest* UassetSizeRequest = nullptr;
	IAsyncReadRequest* UexpSizeRequest = nullptr;
	IAsyncReadRequest* UassetReadRequest = nullptr;
	IAsyncReadRequest* UexpReadRequest = nullptr;
	int64 UassetSize = -1;
	int64 UexpSize = -1;
	std::atomic<int8> PendingSizeRequests = 0;
	std::atomic<int8> PendingReadRequests = 0;
	std::atomic<bool> bDone = false;
	std::atomic<bool> bFailed = false;
	bool bNeedsEngineVersionChecks = false;
};
#endif // ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD

struct FAsyncLoadingPostLoadGroup;

/**
* Structure containing intermediate data required for async loading of all exports of a package.
*/

struct FAsyncPackage2
{
	friend struct FAsyncPackageScope2;
	friend class FAsyncLoadingThread2;
	friend class FAsyncLoadEventQueue2;

	FAsyncPackage2(
		FAsyncLoadingThreadState2& ThreadState,
		const FAsyncPackageDesc2& InDesc,
		FAsyncLoadingThread2& InAsyncLoadingThread,
		FAsyncLoadEventGraphAllocator& InGraphAllocator,
		const FAsyncLoadEventSpec* EventSpecs);
	virtual ~FAsyncPackage2();


	void AddRef()
	{
		RefCount.fetch_add(1);
	}

	bool TryAddRef()
	{
		for (;;)
		{
			int32 CurrentRefCount = RefCount.load();
			if (CurrentRefCount == 0)
			{
				return false;
			}
			if (RefCount.compare_exchange_strong(CurrentRefCount, CurrentRefCount + 1))
			{
				return true;
			}
		}
	}

	void ReleaseRef();

	void ClearImportedPackages();

	/**
	 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
	 */
	double GetLoadStartTime() const;

	void AddCompletionCallback(TUniquePtr<FLoadPackageAsyncDelegate>&& Callback);
	void AddProgressCallback(TUniquePtr<FLoadPackageAsyncProgressDelegate>&& Callback);

	FORCEINLINE UPackage* GetLinkerRoot() const
	{
		return LinkerRoot;
	}

	/** Returns true if loading has failed */
	FORCEINLINE bool HasLoadFailed() const
	{
		return bLoadHasFailed;
	}

	/** Adds new request ID to the existing package */
	void AddRequestID(FAsyncLoadingThreadState2& ThreadState, int32 Id);

	uint64 GetSyncLoadContextId() const
	{
		return SyncLoadContextId;
	}

	void AddConstructedObject(UObject* Object, bool bSubObjectThatAlreadyExists)
	{
		UE_MT_SCOPED_WRITE_ACCESS(ConstructedObjectsAccessDetector);

		if (bSubObjectThatAlreadyExists)
		{
			ConstructedObjects.AddUnique(Object);
		}
		else
		{
			// Mark objects created during async loading process (e.g. from within PostLoad or CreateExport) as async loaded so they 
			// cannot be found. This requires also keeping track of them so we can remove the async loading flag later one when we 
			// finished routing PostLoad to all objects.
			Object->SetInternalFlags(EInternalObjectFlags::AsyncLoading);

			ConstructedObjects.Add(Object);
		}
	}

	void ClearConstructedObjects();

	/** Class specific callback for initializing non-native objects */
	EAsyncPackageState::Type PostLoadInstances(FAsyncLoadingThreadState2& ThreadState);

	/** Creates GC clusters from loaded objects */
	EAsyncPackageState::Type CreateClusters(FAsyncLoadingThreadState2& ThreadState);

	void ImportPackagesRecursive(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore);
	void StartLoading(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	void InitializeLinkerLoadState(const FLinkerInstancingContext* InstancingContext);
	void CreateLinker(const FLinkerInstancingContext* InstancingContext);
	void DetachLinker();
#endif

private:
	void ImportPackagesRecursiveInner(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore, FAsyncPackageHeaderData& Header);

	uint8 PackageNodesMemory[EEventLoadNode2::Package_NumPhases * sizeof(FEventLoadNode2)];
	TArrayView<FEventLoadNode2> PackageNodes;
	/** Basic information associated with this package */
	FAsyncPackageDesc2 Desc;
	FAsyncPackageData Data;
	FAsyncPackageHeaderData HeaderData;
	FAsyncPackageSerializationState SerializationState;
#if WITH_EDITOR
	TOptional<FAsyncPackageHeaderData> OptionalSegmentHeaderData;
	TOptional<FAsyncPackageSerializationState> OptionalSegmentSerializationState;
#endif
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	struct FLinkerLoadState
	{
		FLinkerLoad* Linker = nullptr;
		int32 ProcessingImportedPackageIndex = 0;
		int32 CreateImportIndex = 0;
		int32 CreateExportIndex = 0;
		int32 SerializeExportIndex = 0;
		int32 PostLoadExportIndex = 0;
#if WITH_EDITORONLY_DATA
		int32 MetaDataIndex = -1;
#endif
		bool bIsCurrentlyResolvingImports = false;
		bool bIsCurrentlyCreatingExports = false;
		bool bContainsClasses = false;

		FAsyncPackageLinkerLoadHeaderData LinkerLoadHeaderData;
	};
	TOptional<FLinkerLoadState> LinkerLoadState;
#endif
	/** Cached async loading thread object this package was created by */
	FAsyncLoadingThread2& AsyncLoadingThread;
	FAsyncLoadEventGraphAllocator& GraphAllocator;
	FPackageImportStore ImportStore;
	/** Package which is going to have its exports and imports loaded */
	UPackage* LinkerRoot = nullptr;
	// The sync load context associated with this package
	std::atomic<uint64>			SyncLoadContextId = 0;
	FAsyncLoadingPostLoadGroup* PostLoadGroup = nullptr;
	FAsyncLoadingPostLoadGroup* DeferredPostLoadGroup = nullptr;
	/** Time load begun. This is NOT the time the load was requested in the case of pending requests.	*/
	double						LoadStartTime = 0.0;
	std::atomic<int32>			RefCount = 0;
	bool						bHasStartedImportingPackages = false;
	int32						ProcessedExportBundlesCount = 0;
	/** Current bundle entry index in the current export bundle */
	int32						ExportBundleEntryIndex = 0;
	/** Current index into ExternalReadDependencies array used to spread wating for external reads over several frames			*/
	int32						ExternalReadIndex = 0;
	/** Current index into DeferredClusterObjects array used to spread routing CreateClusters over several frames			*/
	int32						DeferredClusterIndex = 0;
	/** Current index into Export objects array used to spread routing PostLoadInstances over several frames			*/
	int32						PostLoadInstanceIndex = 0;
	/** Current loading state of a package. */
	std::atomic<EAsyncPackageLoadingState2> AsyncPackageLoadingState { EAsyncPackageLoadingState2::NewPackage };

	struct FAllDependenciesState
	{
		FAsyncPackage2* WaitingForPackage = nullptr;
		FAsyncPackage2* PackagesWaitingForThisHead = nullptr;
		FAsyncPackage2* PackagesWaitingForThisTail = nullptr;
		FAsyncPackage2* PrevLink = nullptr;
		FAsyncPackage2* NextLink = nullptr;
		uint32 LastTick = 0;
		int32 PreOrderNumber = -1;
		bool bAssignedToStronglyConnectedComponent = false;
		bool bAllDone = false;

		void UpdateTick(int32 CurrentTick)
		{
			if (LastTick != CurrentTick)
			{
				LastTick = CurrentTick;
				PreOrderNumber = -1;
				bAssignedToStronglyConnectedComponent = false;
			}
		}

		static void AddToWaitList(FAllDependenciesState FAsyncPackage2::* StateMemberPtr, FAsyncPackage2* WaitListPackage, FAsyncPackage2* PackageToAdd)
		{
			check(WaitListPackage);
			check(PackageToAdd);
			check(WaitListPackage != PackageToAdd);
			FAllDependenciesState& WaitListPackageState = WaitListPackage->*StateMemberPtr;
			FAllDependenciesState& PackageToAddState = PackageToAdd->*StateMemberPtr;
			
			if (PackageToAddState.WaitingForPackage == WaitListPackage)
			{
				return;
			}
			if (PackageToAddState.WaitingForPackage)
			{
				FAllDependenciesState::RemoveFromWaitList(StateMemberPtr, PackageToAddState.WaitingForPackage, PackageToAdd);
			}

			check(!PackageToAddState.PrevLink);
			check(!PackageToAddState.NextLink);
			if (WaitListPackageState.PackagesWaitingForThisTail)
			{
				FAllDependenciesState& WaitListTailState = WaitListPackageState.PackagesWaitingForThisTail->*StateMemberPtr;
				check(!WaitListTailState.NextLink);
				WaitListTailState.NextLink = PackageToAdd;
				PackageToAddState.PrevLink = WaitListPackageState.PackagesWaitingForThisTail;
			}
			else
			{
				check(!WaitListPackageState.PackagesWaitingForThisHead);
				WaitListPackageState.PackagesWaitingForThisHead = PackageToAdd;
			}
			WaitListPackageState.PackagesWaitingForThisTail = PackageToAdd;
			PackageToAddState.WaitingForPackage = WaitListPackage;
		}

		static void RemoveFromWaitList(FAllDependenciesState FAsyncPackage2::* StateMemberPtr, FAsyncPackage2* WaitListPackage, FAsyncPackage2* PackageToRemove)
		{
			check(WaitListPackage);
			check(PackageToRemove);

			FAllDependenciesState& WaitListPackageState = WaitListPackage->*StateMemberPtr;
			FAllDependenciesState& PackageToRemoveState = PackageToRemove->*StateMemberPtr;

			check(PackageToRemoveState.WaitingForPackage == WaitListPackage);
			if (PackageToRemoveState.PrevLink)
			{
				FAllDependenciesState& PrevLinkState = PackageToRemoveState.PrevLink->*StateMemberPtr;
				PrevLinkState.NextLink = PackageToRemoveState.NextLink;
			}
			else
			{
				check(WaitListPackageState.PackagesWaitingForThisHead == PackageToRemove);
				WaitListPackageState.PackagesWaitingForThisHead = PackageToRemoveState.NextLink;
			}
			if (PackageToRemoveState.NextLink)
			{
				FAllDependenciesState& NextLinkState = PackageToRemoveState.NextLink->*StateMemberPtr;
				NextLinkState.PrevLink = PackageToRemoveState.PrevLink;
			}
			else
			{
				check(WaitListPackageState.PackagesWaitingForThisTail == PackageToRemove);
				WaitListPackageState.PackagesWaitingForThisTail = PackageToRemoveState.PrevLink;
			}
			PackageToRemoveState.PrevLink = nullptr;
			PackageToRemoveState.NextLink = nullptr;
			PackageToRemoveState.WaitingForPackage = nullptr;
		}
	};
	FAllDependenciesState		AllDependenciesSetupState;
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	FAllDependenciesState		AllDependenciesImportState;
#endif
	FAllDependenciesState		AllDependenciesFullyLoadedState;

	/** True if our load has failed */
	bool						bLoadHasFailed = false;
	/** True if this package was created by this async package */
	bool						bCreatedLinkerRoot = false;
	/** List of all request handles. */
	TArray<int32> RequestIDs;
	/** List of ConstructedObjects = Exports + UPackage + ObjectsCreatedFromExports */
	TArray<UObject*> ConstructedObjects;
	/** Detects if the constructed objects are improperly accessed by different threads at the same time. */
	UE_MT_DECLARE_MRSW_RECURSIVE_ACCESS_DETECTOR(ConstructedObjectsAccessDetector);
	TArray<FExternalReadCallback> ExternalReadDependencies;
	/** Callbacks called when we finished loading this package */
	TArray<TUniquePtr<FLoadPackageAsyncDelegate>, TInlineAllocator<2>> CompletionCallbacks;
	/** Callbacks called for the different loading phase of this package */
	TArray<TUniquePtr<FLoadPackageAsyncProgressDelegate>, TInlineAllocator<2>> ProgressCallbacks;

public:

	FAsyncLoadingThread2& GetAsyncLoadingThread()
	{
		return AsyncLoadingThread;
	}

	FAsyncLoadEventGraphAllocator& GetGraphAllocator()
	{
		return GraphAllocator;
	}

	static FAsyncPackage2* GetCurrentlyExecutingPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* PackageToFilter = nullptr);

	/** [EDL] Begin Event driven loader specific stuff */

	static EEventLoadNodeExecutionResult Event_ProcessExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	static EEventLoadNodeExecutionResult Event_CreateLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_ResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_PreloadLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
#endif
	static EEventLoadNodeExecutionResult Event_ProcessPackageSummary(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_DependenciesReady(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_ExportsDone(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_PostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);
	static EEventLoadNodeExecutionResult Event_DeferredPostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);

	void EventDrivenCreateExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex);
	bool EventDrivenSerializeExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex, FExportArchive* Ar);

	UObject* EventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, FPackageObjectIndex Index, bool bCheckSerialized);
	template<class T>
	T* CastEventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, FPackageObjectIndex Index, bool bCheckSerialized)
	{
		UObject* Result = EventDrivenIndexToObject(Header, Index, bCheckSerialized);
		if (!Result)
		{
			return nullptr;
		}
		return CastChecked<T>(Result);
	}

	FEventLoadNode2& GetPackageNode(EEventLoadNode2 Phase);
	FEventLoadNode2& GetExportBundleNode(EEventLoadNode2 Phase, uint32 ExportBundleIndex);

	/** [EDL] End Event driven loader specific stuff */

	void CallProgressCallbacks(EAsyncLoadingProgress ProgressType);
private:
	void InitializeExportArchive(FExportArchive& Ar, bool bIsOptionalSegment);
	void CreatePackageNodes(const FAsyncLoadEventSpec* EventSpecs);
	void CreateExportBundleNodes(const FAsyncLoadEventSpec* EventSpecs);
	void SetupScriptDependencies();
#if ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING
	bool HasDependencyToPackageDebug(FAsyncPackage2* Package);
	void CheckThatAllDependenciesHaveReachedStateDebug(FAsyncLoadingThreadState2& ThreadState, EAsyncPackageLoadingState2 PackageState, EAsyncPackageLoadingState2 PackageStateForCircularDependencies);
#endif
	struct FUpdateDependenciesStateRecursiveContext
	{
		FUpdateDependenciesStateRecursiveContext(FAllDependenciesState FAsyncPackage2::* InStateMemberPtr, EAsyncPackageLoadingState2 InWaitForPackageState, uint32 InCurrentTick, TFunctionRef<void(FAsyncPackage2*)> InOnStateReached)
			: StateMemberPtr(InStateMemberPtr)
			, WaitForPackageState(InWaitForPackageState)
			, OnStateReached(InOnStateReached)
			, CurrentTick(InCurrentTick)
		{

		}

		FAllDependenciesState FAsyncPackage2::* StateMemberPtr;
		EAsyncPackageLoadingState2 WaitForPackageState;
		TFunctionRef<void(FAsyncPackage2*)> OnStateReached;
		TArray<FAsyncPackage2*, TInlineAllocator<512>> S;
		TArray<FAsyncPackage2*, TInlineAllocator<512>> P;
		uint32 CurrentTick;
		int32 C = 0;
	};
	FAsyncPackage2* UpdateDependenciesStateRecursive(FAsyncLoadingThreadState2& ThreadState, FUpdateDependenciesStateRecursiveContext& Context);
	void WaitForAllDependenciesToReachState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32& CurrentTickVariable, TFunctionRef<void(FAsyncPackage2*)> OnStateReached);
	void ConditionalBeginProcessPackageExports(FAsyncLoadingThreadState2& ThreadState);
	void ConditionalFinishLoading(FAsyncLoadingThreadState2& ThreadState);
	void ConditionalReleasePartialRequests(FAsyncLoadingThreadState2& ThreadState);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	EEventLoadNodeExecutionResult ProcessLinkerLoadPackageSummary(FAsyncLoadingThreadState2& ThreadState);
	bool CreateLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState);
	void ConditionalBeginResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState);
	bool ResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState);
	bool PreloadLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState);
	EEventLoadNodeExecutionResult ExecutePostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState);
	EEventLoadNodeExecutionResult ExecuteDeferredPostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState);
#endif

	void ProcessExportDependencies(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex, FExportBundleEntry::EExportCommandType CommandType);
	int32 GetPublicExportIndex(uint64 ExportHash, FAsyncPackageHeaderData*& OutHeader);
	UObject* ConditionalCreateExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex);
	UObject* ConditionalSerializeExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex);
	UObject* ConditionalCreateImport(const FAsyncPackageHeaderData& Header, int32 LocalImportIndex);
	UObject* ConditionalSerializeImport(const FAsyncPackageHeaderData& Header, int32 LocalImportIndex);

	/**
	 * Begin async loading process. Simulates parts of BeginLoad.
	 *
	 * Objects created during BeginAsyncLoad and EndAsyncLoad will have EInternalObjectFlags::AsyncLoading set
	 */
	void BeginAsyncLoad();
	/**
	 * End async loading process. Simulates parts of EndLoad().
	 */
	void EndAsyncLoad();
	/**
	 * Create UPackage
	 *
	 * @return true
	 */
	void CreateUPackage();

	/**
	 * Finish up UPackage
	 *
	 * @return true
	 */
	void FinishUPackage();

	/**
	 * Finalizes external dependencies till time limit is exceeded
	 *
	 * @return Complete if all dependencies are finished, TimeOut otherwise
	 */
	enum EExternalReadAction { ExternalReadAction_Poll, ExternalReadAction_Wait };
	EAsyncPackageState::Type ProcessExternalReads(FAsyncLoadingThreadState2& ThreadState, EExternalReadAction Action);

	/**
	* Updates load percentage stat
	*/
	void UpdateLoadPercentage();

public:

	/** Serialization context for this package */
	FUObjectSerializeContext* GetSerializeContext();
};

struct FAsyncLoadingPostLoadGroup
{
	uint64 SyncLoadContextId = 0;
	TArray<FAsyncPackage2*> Packages;
	int32 PackagesWithExportsToSerializeCount = 0;
	int32 PackagesWithExportsToPostLoadCount = 0;
};

class FAsyncLoadingThread2 final
	: public FRunnable
	, public IAsyncPackageLoader
{
	friend struct FAsyncPackage2;
public:
	FAsyncLoadingThread2(FIoDispatcher& IoDispatcher, IAsyncPackageLoader* InUncookedPackageLoader);
	virtual ~FAsyncLoadingThread2();

	virtual ELoaderType GetLoaderType() const override
	{
		return ELoaderType::ZenLoader;
	}

private:
	/** Thread to run the worker FRunnable on */
	static constexpr int32 DefaultAsyncPackagesReserveCount = 512;
	FRunnableThread* Thread;
	std::atomic<bool> bStopRequested = false;
	std::atomic<int32> SuspendRequestedCount = 0;
	bool bHasRegisteredAllScriptObjects = false;
	/** [ASYNC/GAME THREAD] true if the async thread is actually started. We don't start it until after we boot because the boot process on the game thread can create objects that are also being created by the loader */
	bool bThreadStarted = false;

#if !UE_BUILD_SHIPPING
	FPlatformFileOpenLog* FileOpenLogWrapper = nullptr;
#endif

	/** [ASYNC/GAME THREAD] Event used to signal loading should be cancelled */
	FEvent* CancelLoadingEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread should be suspended */
	FEvent* ThreadSuspendedEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread has resumed */
	FEvent* ThreadResumedEvent;
	TArray<FAsyncPackage2*> LoadedPackagesToProcess;
	/** [ASYNC/GAME THREAD] Event used to signal the main thread that new processing is needed. Only used when ALT is active. */
	UE::FManualResetEvent MainThreadWakeEvent;

#if WITH_EDITOR
	/** [GAME THREAD] */
	TArray<UObject*> EditorLoadedAssets;
	TArray<UPackage*> EditorCompletedUPackages;
#endif
	/** [ASYNC/GAME THREAD] Packages to be deleted from async thread */
	TMpscQueue<FAsyncPackage2*> DeferredDeletePackages;
	
	struct FCompletedPackageRequest
	{
		FName PackageName;
		EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Succeeded;
		UPackage* UPackage = nullptr;
		FAsyncPackage2* AsyncPackage = nullptr;
		TArray<TUniquePtr<FLoadPackageAsyncDelegate>, TInlineAllocator<2>> CompletionCallbacks;
		TArray<TUniquePtr<FLoadPackageAsyncProgressDelegate>, TInlineAllocator<2>> ProgressCallbacks;
		TArray<int32, TInlineAllocator<2>> RequestIDs;

		static FCompletedPackageRequest FromMissingPackage(
			const FAsyncPackageDesc2& Desc,
			TUniquePtr<FLoadPackageAsyncDelegate>&& CompletionCallback)
		{
			FCompletedPackageRequest Res = {Desc.UPackageName, EAsyncLoadingResult::Failed};
			Res.CompletionCallbacks.Add(MoveTemp(CompletionCallback));
			Res.RequestIDs.Add(Desc.RequestID);
			return Res;
		}

		static FCompletedPackageRequest FromLoadedPackage(
			FAsyncPackage2* Package)
		{
			FCompletedPackageRequest Request
			{
				Package->Desc.UPackageName,
				Package->HasLoadFailed() ? EAsyncLoadingResult::Failed : EAsyncLoadingResult::Succeeded,
				Package->LinkerRoot,
				Package,
				MoveTemp(Package->CompletionCallbacks),
				MoveTemp(Package->ProgressCallbacks)
			};

			Request.RequestIDs.Append(Package->RequestIDs);
			return Request;
		}

		void CallCompletionCallbacks()
		{
			checkSlow(IsInGameThread());

#if WITH_EDITOR
			UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(AsyncPackage ? AsyncPackage->Desc.PIEInstanceID : INDEX_NONE);
#endif
			
			if (CompletionCallbacks.Num() != 0)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(PackageCompletionCallbacks);
				for (TUniquePtr<FLoadPackageAsyncDelegate>& CompletionCallback : CompletionCallbacks)
				{
					CompletionCallback->ExecuteIfBound(PackageName, UPackage, Result);
				}
				CompletionCallbacks.Empty();
			}

			if (ProgressCallbacks.Num() != 0)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(PackageProgressCallbacks_Completion);

				const FLoadPackageAsyncProgressParams Params
				{
					.PackageName = PackageName,
					.LoadedPackage = UPackage,
					.ProgressType = Result == EAsyncLoadingResult::Succeeded ? EAsyncLoadingProgress::FullyLoaded : EAsyncLoadingProgress::Failed
				};
				
				for (TUniquePtr<FLoadPackageAsyncProgressDelegate>& ProgressCallback : ProgressCallbacks)
				{
					ProgressCallback->ExecuteIfBound(Params);
				}
				ProgressCallbacks.Empty();
			}
		}
	};
	TArray<FCompletedPackageRequest> CompletedPackageRequests;
	TArray<FCompletedPackageRequest> FailedPackageRequests;
	FCriticalSection FailedPackageRequestsCritical;

	FCriticalSection AsyncPackagesCritical;
	/** Packages in active loading with GetAsyncPackageId() as key */
	TMap<FPackageId, FAsyncPackage2*> AsyncPackageLookup;

	TMpscQueue<FAsyncPackage2*> ExternalReadQueue;
	TAtomic<int32> PendingIoRequestsCounter{ 0 };
	
	/** List of all pending package requests */
	TSet<int32> PendingRequests;
	/** Synchronization object for PendingRequests list */
	FCriticalSection PendingRequestsCritical;
	TMap<int32, FAsyncPackage2*> RequestIdToPackageMap; // Only accessed from the async loading thread

	/** [ASYNC/GAME THREAD] Number of package load requests in the async loading queue */
	TAtomic<int32> QueuedPackagesCounter { 0 };
	/** [ASYNC/GAME THREAD] Number of packages being loaded on the async thread and post loaded on the game thread */
	TAtomic<int32> LoadingPackagesCounter { 0 };
	/** Encapsulate our counter to make sure all decrements are monitored. */
	class FPackagesWithRemainingWorkCounter
	{
		UE::FManualResetEvent* WakeEvent = nullptr;
		TAtomic<int32> PackagesWithRemainingWorkCounter {0};
	public:
		void SetWakeEvent(UE::FManualResetEvent* InWakeEvent) {	WakeEvent = InWakeEvent; }
		int32 operator++() { return ++PackagesWithRemainingWorkCounter; }
		int32 operator++(int) { return PackagesWithRemainingWorkCounter++; }
		operator int32() const { return PackagesWithRemainingWorkCounter; }

		// Only implement prefix
		int32 operator--() 
		{ 
			int32 newValue = --PackagesWithRemainingWorkCounter;
			if (newValue == 0)
			{
				if (WakeEvent)
				{
					WakeEvent->Notify();
				}
			}
			return newValue;
		}
	};
	/** [ASYNC/GAME THREAD] While this is non-zero there's work left to do */
	FPackagesWithRemainingWorkCounter PackagesWithRemainingWorkCounter;

	FThreadSafeCounter AsyncThreadReady;

	/** When cancelling async loading: list of package requests to cancel */
	TArray<FAsyncPackageDesc2*> QueuedPackagesToCancel;
	/** When cancelling async loading: list of packages to cancel */
	TSet<FAsyncPackage2*> PackagesToCancel;

	/** Async loading thread ID */
	std::atomic<uint32> AsyncLoadingThreadID;

	/** I/O Dispatcher */
	FIoDispatcher& IoDispatcher;

	IAsyncPackageLoader* UncookedPackageLoader;

	FPackageStore& PackageStore;
	FGlobalImportStore GlobalImportStore;
	TMpscQueue<FPackageRequest> PackageRequestQueue;
	TArray<FAsyncPackage2*> PendingPackages;

	/** [GAME THREAD] Initial load pending CDOs */
	TMap<UClass*, TArray<FEventLoadNode2*>> PendingCDOs;
	TArray<UClass*> PendingCDOsRecursiveStack;

	/** [ASYNC/GAME THREAD] Unreachable objects from last NotifyUnreachableObjects callback from GC. */
	FCriticalSection UnreachableObjectsCritical;
	FUnreachableObjects UnreachableObjects;

	TUniquePtr<FAsyncLoadingThreadState2> GameThreadState;
	TUniquePtr<FAsyncLoadingThreadState2> AsyncLoadingThreadState;

	uint32 ConditionalBeginProcessExportsTick = 0;
	uint32 ConditionalBeginResolveImportsTick = 0;
	uint32 ConditionalFinishLoadingTick = 0;

public:

	//~ Begin FRunnable Interface.
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	//~ End FRunnable Interface

	/** Start the async loading thread */
	virtual void StartThread() override;

	/** [EDL] Event queue */
	FZenaphore AltZenaphore;
	FAsyncLoadEventGraphAllocator GraphAllocator;
	FAsyncLoadEventQueue2 EventQueue;
	FAsyncLoadEventQueue2 MainThreadEventQueue;
	TArray<FAsyncLoadEventSpec> EventSpecs;

	/** True if multithreaded async loading is currently being used. */
	inline virtual bool IsMultithreaded() override
	{
		return bThreadStarted;
	}

	/** Sets the current state of async loading */
	void EnterAsyncLoadingTick()
	{
		AsyncLoadingTickCounter++;
	}

	void LeaveAsyncLoadingTick()
	{
		AsyncLoadingTickCounter--;
		check(AsyncLoadingTickCounter >= 0);
	}

	/** Gets the current state of async loading */
	bool GetIsInAsyncLoadingTick() const
	{
		return !!AsyncLoadingTickCounter;
	}

	/** Returns true if packages are currently being loaded on the async thread */
	inline virtual bool IsAsyncLoadingPackages() override
	{
		return PackagesWithRemainingWorkCounter != 0;
	}

	/** Returns true this codes runs on the async loading thread */
	virtual bool IsInAsyncLoadThread() override
	{
		if (IsMultithreaded())
		{
			// We still need to report we're in async loading thread even if 
			// we're on game thread but inside of async loading code (PostLoad mostly)
			// to make it behave exactly like the non-threaded version
			uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
			if (CurrentThreadId == AsyncLoadingThreadID ||
				(IsInGameThread() && GetIsInAsyncLoadingTick()))
			{
				return true;
			}
			return false;
		}
		else
		{
			return IsInGameThread() && GetIsInAsyncLoadingTick();
		}
	}

	/** Returns true if async loading is suspended */
	inline virtual bool IsAsyncLoadingSuspended() override
	{
		return SuspendRequestedCount.load(std::memory_order_relaxed) > 0;
	}

	virtual void NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObjectThatAlreadyExists) override;

	virtual void NotifyUnreachableObjects(const TArrayView<FUObjectItem*>& UnreachableObjects) override;

	virtual void NotifyRegistrationEvent(const TCHAR* PackageName, const TCHAR* Name, ENotifyRegistrationType NotifyRegistrationType, ENotifyRegistrationPhase NotifyRegistrationPhase, UObject* (*InRegister)(), bool InbDynamic, UObject* FinishedObject) override;

	virtual void NotifyRegistrationComplete() override;

	FORCEINLINE FAsyncPackage2* FindAsyncPackage(FPackageId PackageId)
	{
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
		//checkSlow(IsInAsyncLoadThread());
		return AsyncPackageLookup.FindRef(PackageId);
	}

	FORCEINLINE FAsyncPackage2* GetAsyncPackage(const FPackageId& PackageId)
	{
		// TRACE_CPUPROFILER_EVENT_SCOPE(GetAsyncPackage);
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
		return AsyncPackageLookup.FindRef(PackageId);
	}

	void UpdatePackagePriority(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package);
	void UpdatePackagePriorityRecursive(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 NewPriority);

	FAsyncPackage2* FindOrInsertPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackageDesc2& InDesc, bool& bInserted, FAsyncPackage2* ImportedByPackage, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate = TUniquePtr<FLoadPackageAsyncDelegate>(), TUniquePtr<FLoadPackageAsyncProgressDelegate>&& PackageProgressDelegate = TUniquePtr<FLoadPackageAsyncProgressDelegate>());
	void QueueMissingPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackageDesc2& PackageDesc, TUniquePtr<FLoadPackageAsyncDelegate>&& LoadPackageAsyncDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate>&& LoadPackageAsyncProgressDelegate);

	/**
	* [ASYNC* THREAD] Loads all packages
	*
	* @param OutPackagesProcessed Number of packages processed in this call.
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething);

	/**
	* [GAME THREAD] Ticks game thread side of async loading.
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushTree Package dependency tree to be flushed
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type TickAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, TConstArrayView<int32> FlushRequestIDs, bool& bDidSomething);

	/**
	* [ASYNC THREAD] Main thread loop
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushTree Package dependency tree to be flushed
	*/
	EAsyncPackageState::Type TickAsyncThreadFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething);

	/** Initializes async loading thread */
	virtual void InitializeLoading() override;

	virtual void ShutdownLoading() override;

	virtual bool ShouldAlwaysLoadPackageAsync(const FPackagePath& PackagePath) override
	{
		return true;
	}

	int32 LoadPackageInternal(const FPackagePath& InPackagePath, FName InCustomName, TUniquePtr<FLoadPackageAsyncDelegate>&& InCompletionDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate>&& InProgressDelegate, EPackageFlags InPackageFlags, int32 InPIEInstanceID, int32 InPackagePriority, const FLinkerInstancingContext* InInstancingContext, uint32 InLoadFlags);

	virtual int32 LoadPackage(
		const FPackagePath& InPackagePath,
		FName InCustomName,
		FLoadPackageAsyncDelegate InCompletionDelegate,
		EPackageFlags InPackageFlags,
		int32 InPIEInstanceID,
		int32 InPackagePriority,
		const FLinkerInstancingContext* InstancingContext = nullptr,
		uint32 InLoadFlags = LOAD_None) override;

	virtual int32 LoadPackage(
		const FPackagePath& InPackagePath,
		FLoadPackageAsyncOptionalParams Params) override;

	EAsyncPackageState::Type ProcessLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit);

	inline virtual EAsyncPackageState::Type ProcessLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit) override
	{
		FAsyncLoadingThreadState2& ThreadState = *FAsyncLoadingThreadState2::Get();
		return ProcessLoadingFromGameThread(ThreadState, bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
	}

	EAsyncPackageState::Type ProcessLoadingUntilCompleteFromGameThread(FAsyncLoadingThreadState2& ThreadState, TFunctionRef<bool()> CompletionPredicate, double TimeLimit);

	inline virtual EAsyncPackageState::Type ProcessLoadingUntilComplete(TFunctionRef<bool()> CompletionPredicate, double TimeLimit) override
	{
		FAsyncLoadingThreadState2& ThreadState = *FAsyncLoadingThreadState2::Get();
		return ProcessLoadingUntilCompleteFromGameThread(ThreadState, CompletionPredicate, TimeLimit);
	}

	virtual void CancelLoading() override;

	virtual void SuspendLoading() override;

	virtual void ResumeLoading() override;

	virtual void FlushLoading(TConstArrayView<int32> RequestIds) override;

	void FlushLoadingFromLoadingThread(FAsyncLoadingThreadState2& ThreadState, TConstArrayView<int32> RequestIds);
	void WarnAboutPotentialSyncLoadStall(FAsyncLoadingSyncLoadContext* SyncLoadContext);

	virtual int32 GetNumQueuedPackages() override
	{
		return QueuedPackagesCounter;
	}

	virtual int32 GetNumAsyncPackages() override
	{
		return LoadingPackagesCounter;
	}

	/**
	 * [GAME THREAD] Gets the load percentage of the specified package
	 * @param PackageName Name of the package to return async load percentage for
	 * @return Percentage (0-100) of the async package load or -1 of package has not been found
	 */
	virtual float GetAsyncLoadPercentage(const FName& PackageName) override;

	/**
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsRequestID(int32 RequestID)
	{
		FScopeLock Lock(&PendingRequestsCritical);
		return PendingRequests.Contains(RequestID);
	}

	/**
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsAnyRequestID(TConstArrayView<int32> RequestIDs)
	{
		FScopeLock Lock(&PendingRequestsCritical);
		return Algo::AnyOf(RequestIDs, [this](int32 RequestID) { return PendingRequests.Contains(RequestID); });
	}

	/**
	 * [ASYNC/GAME THREAD] Adds a request ID to the list of pending requests
	 */
	void AddPendingRequest(int32 RequestID)
	{
		FScopeLock Lock(&PendingRequestsCritical);
		PendingRequests.Add(RequestID);
	}

	/**
	 * [ASYNC/GAME THREAD] Removes a request ID from the list of pending requests
	 */
	void RemovePendingRequests(FAsyncLoadingThreadState2& ThreadState, TConstArrayView<int32> RequestIDs)
	{
		int32 RemovedCount = 0;
		{
			FScopeLock Lock(&PendingRequestsCritical);
			for (int32 ID : RequestIDs)
			{
				RemovedCount += PendingRequests.Remove(ID);
				TRACE_LOADTIME_END_REQUEST(ID);
			}
			if (PendingRequests.IsEmpty())
			{
				PendingRequests.Empty(DefaultAsyncPackagesReserveCount);
			}
		}

		// Any removed pending request is of interest to main thread as it might unblock a flush.
		if (RemovedCount > 0 && ThreadState.bIsAsyncLoadingThread)
		{
			MainThreadWakeEvent.Notify();
		}
	}

	void AddPendingCDOs(FAsyncPackage2* Package, TArray<UClass*, TInlineAllocator<8>>& Classes)
	{
		for (UClass* Class : Classes)
		{
			TArray<FEventLoadNode2*>& Nodes = PendingCDOs.FindOrAdd(Class);
			FEventLoadNode2& Node = Package->GetPackageNode(Package_DependenciesReady);
			Node.AddBarrier();
			Nodes.Add(&Node);
		}
	}

private:
#if ALT2_ENABLE_LINKERLOAD_SUPPORT || WITH_EDITOR
	bool TryGetPackagePathFromFileSystem(FName& PackageNameToLoad, FName& UPackageName, FPackagePath& OutPackagePath)
	{
#if WITH_EDITORONLY_DATA
		// In editor, set MatchCaseOnDisk=true so that we set the capitalization of the Package's FName to match the
		// capitalization on disk. Different capitalizations can arise from imports of the package that were somehow
		// constructed with a different captialization (most often because the disk captialization changed).
		// We need the captialization to match so that source control operations in case-significant source control
		// depots succeed, and to avoid indetermism in the cook.
		constexpr bool bMatchCaseOnDisk = true;
#else
		constexpr bool bMatchCaseOnDisk = false;
#endif

		if (!PackageNameToLoad.IsNone() &&
			FPackagePath::TryFromPackageName(PackageNameToLoad, OutPackagePath) &&
			FPackageName::DoesPackageExistEx(OutPackagePath, FPackageName::EPackageLocationFilter::FileSystem,
				bMatchCaseOnDisk, &OutPackagePath) != FPackageName::EPackageLocationFilter::None)
		{
			FName CaseCorrectedPackageName = OutPackagePath.GetPackageFName();
			if (PackageNameToLoad == UPackageName)
			{
				UPackageName = CaseCorrectedPackageName;
			}
			PackageNameToLoad = CaseCorrectedPackageName;
			return true;
		}
		return false;
	}
#endif

#if WITH_EDITOR
	void ConditionalProcessEditorCallbacks();
#endif
	void ConditionalBeginPostLoad(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* PostLoadGroup);
	void ConditionalBeginDeferredPostLoad(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* DeferredPostLoadGroup);
	void MergePostLoadGroups(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* Target, FAsyncLoadingPostLoadGroup* Source, bool bUpdateSyncLoadContext = true);

	bool ProcessDeferredDeletePackagesQueue(int32 MaxCount = MAX_int32)
	{
		bool bDidSomething = false;
		if (!DeferredDeletePackages.IsEmpty())
		{
			FAsyncPackage2* Package = nullptr;
			int32 Count = 0;
			while (++Count <= MaxCount && DeferredDeletePackages.Dequeue(Package))
			{
				DeleteAsyncPackage(Package);
				bDidSomething = true;
			}
		}
		return bDidSomething;
	}

	void OnPreGarbageCollect();

	void CollectUnreachableObjects(TArrayView<FUObjectItem*> UnreachableObjectItems, FUnreachableObjects& OutUnreachableObjects);

	void RemoveUnreachableObjects(FUnreachableObjects& ObjectsToRemove);

	bool ProcessPendingCDOs(FAsyncLoadingThreadState2& ThreadState)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ProcessPendingCDOs);

		bool bDidSomething = false;
		UClass* Class = nullptr;
		const uint64 SyncLoadContextId = ThreadState.SyncLoadContextStack.Num() > 0 ? ThreadState.SyncLoadContextStack.Top()->ContextId : 0;
		for (TMap<UClass*, TArray<FEventLoadNode2*>>::TIterator It = PendingCDOs.CreateIterator(); It; ++It)
		{
			UClass* CurrentClass = It.Key();
			if (PendingCDOsRecursiveStack.Num() > 0)
			{
				bool bAnyParentOnStack = false;
				UClass* Super = CurrentClass;
				while (Super)
				{
					if (PendingCDOsRecursiveStack.Contains(Super))
					{
						bAnyParentOnStack = true;
						break;
					}
					Super = Super->GetSuperClass();
				}
				if (bAnyParentOnStack)
				{
					continue;
				}
			}

			const TArray<FEventLoadNode2*>& Nodes = It.Value();
			for (const FEventLoadNode2* Node : Nodes)
			{
				const uint64 NodeContextId = Node->GetSyncLoadContextId();
				if (NodeContextId >= SyncLoadContextId)
				{
					Class = CurrentClass;
					break;
				}
			}

			if (Class != nullptr)
			{
				break;
			}
		}

		if (Class)
		{
			TArray<FEventLoadNode2*> Nodes;
			PendingCDOs.RemoveAndCopyValue(Class, Nodes);

			UE_LOG(LogStreaming, Log,
				TEXT("ProcessPendingCDOs: Creating CDO for '%s' for SyncLoadContextId %llu, releasing %d nodes. %d CDOs remaining."),
				*Class->GetFullName(), SyncLoadContextId, Nodes.Num(), PendingCDOs.Num());
			PendingCDOsRecursiveStack.Push(Class);
			UObject* CDO = Class->GetDefaultObject(/*bCreateIfNeeded*/ true);
			verify(PendingCDOsRecursiveStack.Pop() == Class);

			ensureAlwaysMsgf(CDO, TEXT("Failed to create CDO for %s"), *Class->GetFullName());
			UE_LOG(LogStreaming, Verbose, TEXT("ProcessPendingCDOs: Created CDO for '%s'."), *Class->GetFullName());

			for (FEventLoadNode2* Node : Nodes)
			{
				Node->ReleaseBarrier(&ThreadState);
			}

			bDidSomething = true;
		}
		return bDidSomething;
	}

	/**
	* [GAME THREAD] Performs game-thread specific operations on loaded packages (not-thread-safe PostLoad, callbacks)
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushTree Package dependency tree to be flushed
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessLoadedPackagesFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething, TConstArrayView<int32> FlushRequestIDs = {});

	void IncludePackageInSyncLoadContextRecursive(FAsyncLoadingThreadState2& ThreadState, uint64 ContextId, FAsyncPackage2* Package);
	void UpdateSyncLoadContext(FAsyncLoadingThreadState2& ThreadState, bool bAutoHandleSyncLoadContext = true);

	bool CreateAsyncPackagesFromQueue(FAsyncLoadingThreadState2& ThreadState);

	FAsyncPackage2* CreateAsyncPackage(FAsyncLoadingThreadState2& ThreadState, const FAsyncPackageDesc2& Desc)
	{
		UE_ASYNC_PACKAGE_DEBUG(Desc);

		return new FAsyncPackage2(ThreadState, Desc, *this, GraphAllocator, EventSpecs.GetData());
	}

	void InitializeAsyncPackageFromPackageStore(FAsyncLoadingThreadState2& ThreadState, FIoBatch* IoBatch, FAsyncPackage2* AsyncPackage, const FPackageStoreEntry& PackageStoreEntry)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(InitializeAsyncPackageFromPackageStore);
		UE_ASYNC_PACKAGE_DEBUG(AsyncPackage->Desc);
		
		FAsyncPackageData& Data = AsyncPackage->Data;

		const int32 ImportedPackagesCount = PackageStoreEntry.ImportedPackageIds.Num();
		const uint64 ImportedPackageIdsMemSize = Align(sizeof(FPackageId) * ImportedPackagesCount, 8);
#if WITH_EDITOR
		const bool bHasOptionalSegment = PackageStoreEntry.bHasOptionalSegment;
		const int32 OptionalSegmentImportedPackagesCount = PackageStoreEntry.OptionalSegmentImportedPackageIds.Num();
		const uint64 OptionalSegmentImportedPackageIdsMemSize = Align(sizeof(FPackageId) * OptionalSegmentImportedPackagesCount, 8);

		const int32 TotalImportedPackagesCount = ImportedPackagesCount + OptionalSegmentImportedPackagesCount;
#else
		const int32 TotalImportedPackagesCount = ImportedPackagesCount;
#endif
		const int32 ShaderMapHashesCount = PackageStoreEntry.ShaderMapHashes.Num();

		const uint64 ImportedPackagesMemSize = Align(sizeof(FAsyncPackage2*) * TotalImportedPackagesCount, 8);
		const uint64 ShaderMapHashesMemSize = Align(sizeof(FSHAHash) * ShaderMapHashesCount, 8);
		const uint64 MemoryBufferSize =
#if WITH_EDITOR
			OptionalSegmentImportedPackageIdsMemSize +
#endif
			ImportedPackageIdsMemSize +
			ImportedPackagesMemSize +
			ShaderMapHashesMemSize;

#if PLATFORM_32BITS
		if (MemoryBufferSize > MAX_Int32)
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Memory buffer size overflow"));
			return;
		}
#endif
		Data.MemoryBuffer0 = reinterpret_cast<uint8*>(FMemory::Malloc(MemoryBufferSize));

		uint8* DataPtr = Data.MemoryBuffer0;

		Data.ShaderMapHashes = MakeArrayView(reinterpret_cast<const FSHAHash*>(DataPtr), ShaderMapHashesCount);
		FMemory::Memcpy((void*)Data.ShaderMapHashes.GetData(), PackageStoreEntry.ShaderMapHashes.GetData(), sizeof(FSHAHash) * ShaderMapHashesCount);
		DataPtr += ShaderMapHashesMemSize;
		Data.ImportedAsyncPackages = MakeArrayView(reinterpret_cast<FAsyncPackage2**>(DataPtr), TotalImportedPackagesCount);
		FMemory::Memzero(DataPtr, ImportedPackagesMemSize);
		DataPtr += ImportedPackagesMemSize;

		FAsyncPackageHeaderData& HeaderData = AsyncPackage->HeaderData;
		HeaderData.ImportedPackageIds = MakeArrayView(reinterpret_cast<FPackageId*>(DataPtr), ImportedPackagesCount);
		FMemory::Memcpy((void*)HeaderData.ImportedPackageIds.GetData(), PackageStoreEntry.ImportedPackageIds.GetData(), sizeof(FPackageId) * ImportedPackagesCount);
		DataPtr += ImportedPackageIdsMemSize;

		Data.TotalExportBundleCount = 1;
		HeaderData.ImportedAsyncPackagesView = Data.ImportedAsyncPackages;
#if WITH_EDITOR
		if (bHasOptionalSegment)
		{
			AsyncPackage->OptionalSegmentSerializationState.Emplace();
			FAsyncPackageHeaderData& OptionalSegmentHeaderData = AsyncPackage->OptionalSegmentHeaderData.Emplace();
			OptionalSegmentHeaderData.ImportedPackageIds = MakeArrayView(reinterpret_cast<FPackageId*>(DataPtr), OptionalSegmentImportedPackagesCount);
			FMemory::Memcpy((void*)OptionalSegmentHeaderData.ImportedPackageIds.GetData(), PackageStoreEntry.OptionalSegmentImportedPackageIds.GetData(), sizeof(FPackageId) * OptionalSegmentImportedPackagesCount);
			DataPtr += OptionalSegmentImportedPackageIdsMemSize;

			++Data.TotalExportBundleCount;
			HeaderData.ImportedAsyncPackagesView = Data.ImportedAsyncPackages.Left(ImportedPackagesCount);
			OptionalSegmentHeaderData.ImportedAsyncPackagesView = Data.ImportedAsyncPackages.Right(OptionalSegmentImportedPackagesCount);
		}
#endif
		check(DataPtr - Data.MemoryBuffer0 == MemoryBufferSize);

#if WITH_EDITOR
		const bool bCanImportPackagesWithIdsOnly = false;
#elif ALT2_ENABLE_LINKERLOAD_SUPPORT
		const bool bIsZenPackage = !AsyncPackage->LinkerLoadState.IsSet();
		bool bCanImportPackagesWithIdsOnly = bIsZenPackage;
#else
		const bool bCanImportPackagesWithIdsOnly = true;
#endif
		if (bCanImportPackagesWithIdsOnly)
		{
			check(IoBatch);
			AsyncPackage->ImportPackagesRecursive(ThreadState, *IoBatch, PackageStore);
		}
	}

	void FinishInitializeAsyncPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* AsyncPackage)
	{
		FAsyncPackageData& Data = AsyncPackage->Data;
		FAsyncPackageHeaderData& HeaderData = AsyncPackage->HeaderData;
		int32 TotalExportCount = HeaderData.ExportMap.Num();
		const uint64 ExportBundleEntriesCopyMemSize = Align(HeaderData.ExportBundleEntries.Num() * sizeof(FExportBundleEntry), 8);
#if WITH_EDITOR
		FAsyncPackageHeaderData* OptionalSegmentHeaderData = AsyncPackage->OptionalSegmentHeaderData.GetPtrOrNull();
		uint64 OptionalSegmentExportBundleEntriesCopyMemSize = 0;
		if (OptionalSegmentHeaderData)
		{
			TotalExportCount += OptionalSegmentHeaderData->ExportMap.Num();
			OptionalSegmentExportBundleEntriesCopyMemSize = Align(OptionalSegmentHeaderData->ExportBundleEntries.Num() * sizeof(FExportBundleEntry), 8);
		}
#endif
		const int32 ExportBundleNodeCount = Data.TotalExportBundleCount * EEventLoadNode2::ExportBundle_NumPhases;
		const uint64 ExportBundleNodesMemSize = Align(sizeof(FEventLoadNode2) * ExportBundleNodeCount, 8);
		const uint64 ExportsMemSize = Align(sizeof(FExportObject) * TotalExportCount, 8);

		const uint64 MemoryBufferSize =
			ExportBundleNodesMemSize +
			ExportsMemSize +
#if WITH_EDITOR
			OptionalSegmentExportBundleEntriesCopyMemSize +
#endif
			ExportBundleEntriesCopyMemSize;

#if PLATFORM_32BITS
		if (MemoryBufferSize > MAX_Int32)
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Memory buffer size overflow"));
			return;
		}
#endif
		Data.MemoryBuffer1 = reinterpret_cast<uint8*>(FMemory::Malloc(MemoryBufferSize));
		
		uint8* DataPtr = Data.MemoryBuffer1;

		Data.Exports = MakeArrayView(reinterpret_cast<FExportObject*>(DataPtr), TotalExportCount);
		DataPtr += ExportsMemSize;
		Data.ExportBundleNodes = MakeArrayView(reinterpret_cast<FEventLoadNode2*>(DataPtr), ExportBundleNodeCount);
		DataPtr += ExportBundleNodesMemSize;
		HeaderData.ExportBundleEntriesCopyForPostLoad = MakeArrayView(reinterpret_cast<FExportBundleEntry*>(DataPtr), HeaderData.ExportBundleEntries.Num());
		FMemory::Memcpy(DataPtr, HeaderData.ExportBundleEntries.GetData(), HeaderData.ExportBundleEntries.Num() * sizeof(FExportBundleEntry));
		DataPtr += ExportBundleEntriesCopyMemSize;

		HeaderData.ExportsView = Data.Exports;

#if WITH_EDITOR
		if (OptionalSegmentHeaderData)
		{
			OptionalSegmentHeaderData->ExportBundleEntriesCopyForPostLoad = MakeArrayView(reinterpret_cast<FExportBundleEntry*>(DataPtr), OptionalSegmentHeaderData->ExportBundleEntries.Num());
			FMemory::Memcpy(DataPtr, OptionalSegmentHeaderData->ExportBundleEntries.GetData(), OptionalSegmentHeaderData->ExportBundleEntries.Num() * sizeof(FExportBundleEntry));
			DataPtr += OptionalSegmentExportBundleEntriesCopyMemSize;

			HeaderData.ExportsView = Data.Exports.Left(HeaderData.ExportCount);
			OptionalSegmentHeaderData->ExportsView = Data.Exports.Right(OptionalSegmentHeaderData->ExportCount);
		}
#endif

		check(DataPtr - Data.MemoryBuffer1 == MemoryBufferSize);
		AsyncPackage->CreateExportBundleNodes(EventSpecs.GetData());

		AsyncPackage->ConstructedObjects.Reserve(Data.Exports.Num() + 1); // +1 for UPackage, may grow dynamically beyond that
		for (FExportObject& Export : Data.Exports)
		{
			Export = FExportObject();
		}

		if (!AsyncPackage->bHasStartedImportingPackages)
		{
			FIoBatch IoBatch = IoDispatcher.NewBatch();
			{
				FPackageStoreReadScope _(PackageStore);
				AsyncPackage->ImportPackagesRecursive(ThreadState, IoBatch, PackageStore);
			}
			IoBatch.Issue();
		}
	}

	void DeleteAsyncPackage(FAsyncPackage2* Package)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DeleteAsyncPackage);
		UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
		for (int32 RequestId : Package->RequestIDs)
		{
			RequestIdToPackageMap.Remove(RequestId);
		}
		if (RequestIdToPackageMap.IsEmpty())
		{
			RequestIdToPackageMap.Empty(DefaultAsyncPackagesReserveCount);
		}
		delete Package;
		--PackagesWithRemainingWorkCounter;
		TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);
	}

	/** Number of times we re-entered the async loading tick, mostly used by singlethreaded ticking. Debug purposes only. */
	int32 AsyncLoadingTickCounter;
};

/**
 * Updates FUObjectThreadContext with the current package when processing it.
 * FUObjectThreadContext::AsyncPackage is used by NotifyConstructedDuringAsyncLoading.
 */
struct FAsyncPackageScope2
{
	/** Outer scope package */
	void* PreviousPackage;
	IAsyncPackageLoader* PreviousAsyncPackageLoader;

	/** Cached ThreadContext so we don't have to access it again */
	FUObjectThreadContext& ThreadContext;

	FAsyncPackageScope2(FAsyncPackage2* InPackage)
		: ThreadContext(FUObjectThreadContext::Get())
	{
		PreviousPackage = ThreadContext.AsyncPackage;
		ThreadContext.AsyncPackage = InPackage;
		PreviousAsyncPackageLoader = ThreadContext.AsyncPackageLoader;
		ThreadContext.AsyncPackageLoader = &InPackage->AsyncLoadingThread;
	}
	~FAsyncPackageScope2()
	{
		ThreadContext.AsyncPackage = PreviousPackage;
		ThreadContext.AsyncPackageLoader = PreviousAsyncPackageLoader;
	}
};

/** Just like TGuardValue for FAsyncLoadingThread::AsyncLoadingTickCounter but only works for the game thread */
struct FAsyncLoadingTickScope2
{
	FAsyncLoadingThread2& AsyncLoadingThread;
	bool bNeedsToLeaveAsyncTick;

	FAsyncLoadingTickScope2(FAsyncLoadingThread2& InAsyncLoadingThread)
		: AsyncLoadingThread(InAsyncLoadingThread)
		, bNeedsToLeaveAsyncTick(false)
	{
		if (IsInGameThread())
		{
			AsyncLoadingThread.EnterAsyncLoadingTick();
			bNeedsToLeaveAsyncTick = true;
		}
	}
	~FAsyncLoadingTickScope2()
	{
		if (bNeedsToLeaveAsyncTick)
		{
			AsyncLoadingThread.LeaveAsyncLoadingTick();
		}
	}
};

void FAsyncLoadingThread2::InitializeLoading()
{
#if !UE_BUILD_SHIPPING
	{
		FString DebugPackageNamesString;
		FParse::Value(FCommandLine::Get(), TEXT("-s.DebugPackageNames="), DebugPackageNamesString);
		ParsePackageNames(DebugPackageNamesString, GAsyncLoading2_DebugPackageIds);
		FString VerbosePackageNamesString;
		FParse::Value(FCommandLine::Get(), TEXT("-s.VerbosePackageNames="), VerbosePackageNamesString);
		ParsePackageNames(VerbosePackageNamesString, GAsyncLoading2_VerbosePackageIds);
		ParsePackageNames(DebugPackageNamesString, GAsyncLoading2_VerbosePackageIds);
		GAsyncLoading2_VerboseLogFilter = GAsyncLoading2_VerbosePackageIds.Num() > 0 ? 1 : 2;
	}

	FileOpenLogWrapper = (FPlatformFileOpenLog*)(FPlatformFileManager::Get().FindPlatformFile(FPlatformFileOpenLog::GetTypeName()));
#endif

	PackageStore.OnPendingEntriesAdded().AddLambda([this]()
	{
		AltZenaphore.NotifyOne();
	});
	
	AsyncThreadReady.Increment();

	UE_LOG(LogStreaming, Display, TEXT("AsyncLoading2 - Initialized"));
}

void FAsyncLoadingThread2::UpdatePackagePriority(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdatePackagePriority);
	EAsyncPackageLoadingState2 LoadingState = Package->AsyncPackageLoadingState;
	check(ThreadState.bCanAccessAsyncLoadingThreadData);

	// TODO: Shader requests are not tracked so they won't be reprioritized correctly here
	if (LoadingState <= EAsyncPackageLoadingState2::WaitingForIo)
	{
		Package->SerializationState.IoRequest.UpdatePriority(Package->Desc.Priority);
#if WITH_EDITOR
		if (Package->OptionalSegmentSerializationState.IsSet())
		{
			Package->OptionalSegmentSerializationState->IoRequest.UpdatePriority(Package->Desc.Priority);
		}
#endif
	}
	if (LoadingState <= EAsyncPackageLoadingState2::PostLoad)
	{
		EventQueue.UpdatePackagePriority(Package);
	}
	if (LoadingState == EAsyncPackageLoadingState2::DeferredPostLoad)
	{
		if (ThreadState.bIsAsyncLoadingThread)
		{
			if (Package->TryAddRef())
			{
				GameThreadState->PackagesToReprioritize.Enqueue(Package);

				// Repriorization of packages is of interest to main thread as it could unblock a flush.
				MainThreadWakeEvent.Notify();
			}
		}
		else
		{
			MainThreadEventQueue.UpdatePackagePriority(Package);
		}
	}
}

void FAsyncLoadingThread2::UpdatePackagePriorityRecursive(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 NewPriority)
{
	if (Package->Desc.Priority >= NewPriority)
	{
		return;
	}
	Package->Desc.Priority = NewPriority;
	for (FAsyncPackage2* ImportedPackage : Package->Data.ImportedAsyncPackages)
	{
		if (ImportedPackage)
		{
			UpdatePackagePriorityRecursive(ThreadState, ImportedPackage, NewPriority);
		}
	}
	UpdatePackagePriority(ThreadState, Package);
}

void FAsyncLoadingThread2::ConditionalBeginPostLoad(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* PostLoadGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginPostLoad);
	check(PostLoadGroup);
	check(ThreadState.bCanAccessAsyncLoadingThreadData);
	if (PostLoadGroup->PackagesWithExportsToSerializeCount == 0)
	{
		// Release the post load node of packages in the post load group in reverse order that they were added to the group
		// This usually means that dependencies will be post load first, similarly to how they are also serialized first
		for (int32 Index = PostLoadGroup->Packages.Num() - 1; Index >= 0; --Index)
		{
			FAsyncPackage2* Package = PostLoadGroup->Packages[Index];
			check(Package->PostLoadGroup == PostLoadGroup);
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ExportsDone);

			// Move the PostLoadGroup to the DeferredPostLoadGroup so that we do not mistakenly consider post load as not having being triggered yet
			Package->PostLoadGroup = nullptr;
			Package->DeferredPostLoadGroup = PostLoadGroup;
			
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::PostLoad;
			Package->ConditionalReleasePartialRequests(ThreadState);
			for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.TotalExportBundleCount; ++ExportBundleIndex)
			{
				Package->GetExportBundleNode(EEventLoadNode2::ExportBundle_PostLoad, ExportBundleIndex).ReleaseBarrier(&ThreadState);
			}
		}
		PostLoadGroup->PackagesWithExportsToPostLoadCount = PostLoadGroup->Packages.Num();
	}
}

void FAsyncLoadingThread2::ConditionalBeginDeferredPostLoad(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* DeferredPostLoadGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginDeferredPostLoad);
	check(DeferredPostLoadGroup);
	check(ThreadState.bCanAccessAsyncLoadingThreadData);
	if (DeferredPostLoadGroup->PackagesWithExportsToPostLoadCount == 0)
	{
		// Release the post load node of packages in the post load group in reverse order that they were added to the group
		// This usually means that dependencies will be post load first, similarly to how they are also serialized first
		for (int32 Index = DeferredPostLoadGroup->Packages.Num() - 1; Index >= 0; --Index)
		{
			FAsyncPackage2* Package = DeferredPostLoadGroup->Packages[Index];
			check(Package->DeferredPostLoadGroup == DeferredPostLoadGroup);
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PostLoad);
			Package->DeferredPostLoadGroup = nullptr;
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoad;
			Package->ConditionalReleasePartialRequests(ThreadState);
			for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.TotalExportBundleCount; ++ExportBundleIndex)
			{
				Package->GetExportBundleNode(EEventLoadNode2::ExportBundle_DeferredPostLoad, ExportBundleIndex).ReleaseBarrier(&ThreadState);
			}
		}
		delete DeferredPostLoadGroup;
	}
}

void FAsyncLoadingThread2::MergePostLoadGroups(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingPostLoadGroup* Target, FAsyncLoadingPostLoadGroup* Source, bool bUpdateSyncLoadContext)
{
	if (Target == Source)
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(MergePostLoadGroups);
	check(ThreadState.bCanAccessAsyncLoadingThreadData);
	for (FAsyncPackage2* Package : Source->Packages)
	{
		check(Package->PostLoadGroup == Source);
		Package->PostLoadGroup = Target;
	}
	Target->Packages.Append(MoveTemp(Source->Packages));
	Target->PackagesWithExportsToSerializeCount += Source->PackagesWithExportsToSerializeCount;
	check(Target->PackagesWithExportsToPostLoadCount == 0 && Source->PackagesWithExportsToPostLoadCount == 0);

	// If the intention of the caller of this function is to merge postloads into the caller package so they are executed later after
	// a partial flush, then we can't update the synccontext of the caller during the merge as it would make the whole hierarchy
	// flush in a single swoop which is in direct contradiction with the partial flush feature.
	if (bUpdateSyncLoadContext)
	{
		const uint64 SyncLoadContextId = FMath::Max(Source->SyncLoadContextId, Target->SyncLoadContextId);
		if (SyncLoadContextId)
		{
			Target->SyncLoadContextId = SyncLoadContextId;
			for (FAsyncPackage2* Package : Target->Packages)
			{
				Package->SyncLoadContextId = SyncLoadContextId;
				if (Package->Desc.Priority < MAX_int32)
				{
					Package->Desc.Priority = MAX_int32;
					UpdatePackagePriority(ThreadState, Package);
				}
			}
		}
	}
	delete Source;
}

FAsyncPackage2* FAsyncLoadingThread2::FindOrInsertPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackageDesc2& Desc, bool& bInserted, FAsyncPackage2* ImportedByPackage, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate>&& PackageProgressDelegate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindOrInsertPackage);
	check(ThreadState.bCanAccessAsyncLoadingThreadData);
	FAsyncPackage2* Package = nullptr;
	bInserted = false;
	{
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
		Package = AsyncPackageLookup.FindRef(Desc.UPackageId);
		if (!Package)
		{
			Package = CreateAsyncPackage(ThreadState, Desc);
			checkf(Package, TEXT("Failed to create async package %s"), *Desc.UPackageName.ToString());
			Package->AddRef();
			++LoadingPackagesCounter;
			TRACE_COUNTER_SET(AsyncLoadingLoadingPackages, LoadingPackagesCounter);
			AsyncPackageLookup.Add(Desc.UPackageId, Package);
			bInserted = true;
		}
		else
		{
			if (Desc.RequestID > 0)
			{
				Package->AddRequestID(ThreadState, Desc.RequestID);
			}
			if (Desc.Priority > Package->Desc.Priority)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UpdatePackagePriority);
				UpdatePackagePriorityRecursive(ThreadState, Package, Desc.Priority);
			}
		}
		if (PackageLoadedDelegate.IsValid())
		{
			Package->AddCompletionCallback(MoveTemp(PackageLoadedDelegate));
		}
		if (PackageProgressDelegate.IsValid())
		{
			Package->AddProgressCallback(MoveTemp(PackageProgressDelegate));
		}
	}

	// PostLoadGroup are to protect some race conditions happening in runtime when ALT is active but
	// it is causing crashes and soft-locks in editor when FlushAsyncLoading is called on specific requests
	// that are in the same postload group as their outer, preventing fine grained flush. So we disable this
	// in editor for the time being since ALT is not yet activated by default and we'll sort out the race conditions instead.
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	constexpr bool bIsPostLoadGroupFeatureActive = false;
#else
	// Prevents activating postload groups during boot because it was causing deadlock into the 
	// highly recursive InitDefaultMaterials function on some platforms.
	// Since postload groups are there to protect against race conditions between postloads and serialize,
	// no such race can exists until the loading thread is started.
	const bool bIsPostLoadGroupFeatureActive = bThreadStarted;
#endif

	if (bInserted)
	{
		// Created a new package, either create a new post load group or use the one from the importing package
		FAsyncLoadingPostLoadGroup* PostLoadGroup = (bIsPostLoadGroupFeatureActive && ImportedByPackage) ? ImportedByPackage->PostLoadGroup : new FAsyncLoadingPostLoadGroup();
		++PostLoadGroup->PackagesWithExportsToSerializeCount;
		PostLoadGroup->Packages.Add(Package);
		check(!Package->PostLoadGroup);
		Package->PostLoadGroup = PostLoadGroup;
	}
	else if (ImportedByPackage && bIsPostLoadGroupFeatureActive)
	{
		// Importing a package that was already being loaded
		if (!Package->PostLoadGroup)
		{
			// The imported package has started postloading, wait for it to finish postloading before serializing any exports
			for (int32 DependentExportBundleIndex = 0; DependentExportBundleIndex < ImportedByPackage->Data.TotalExportBundleCount; ++DependentExportBundleIndex)
			{
				for (int32 DependsOnExportBundleIndex = 0; DependsOnExportBundleIndex < Package->Data.TotalExportBundleCount; ++DependsOnExportBundleIndex)
				{
					ImportedByPackage->GetExportBundleNode(ExportBundle_Process, DependentExportBundleIndex).DependsOn(&Package->GetExportBundleNode(ExportBundle_DeferredPostLoad, DependsOnExportBundleIndex));
				}
			}
		}
		else if (ImportedByPackage->PostLoadGroup != Package->PostLoadGroup)
		{
			// The imported package hasn't started postloading yet, merge its post load group with the one for the importing package
			check(ImportedByPackage->PostLoadGroup);
			MergePostLoadGroups(ThreadState, ImportedByPackage->PostLoadGroup, Package->PostLoadGroup);
		}
	}
	return Package;
}

void FAsyncLoadingThread2::IncludePackageInSyncLoadContextRecursive(FAsyncLoadingThreadState2& ThreadState, uint64 ContextId, FAsyncPackage2* Package)
{
	if (Package->SyncLoadContextId >= ContextId)
	{
		return;
	}

	if (Package->AsyncPackageLoadingState >= EAsyncPackageLoadingState2::Complete)
	{
		return;
	}

	UE_ASYNC_PACKAGE_LOG(VeryVerbose, Package->Desc, TEXT("IncludePackageInSyncLoadContextRecursive"), TEXT("Setting SyncLoadContextId to %d"), ContextId);

	Package->SyncLoadContextId = ContextId;

	// When using the partial loading feature, don't try to upgrade postload groups into higher priority sync context as
	// this is exactly what we're trying to prevent. Package that are merged together into a postload group are supposed
	// to stay at the lower syncloadcontextid, that way, it allows to exit the flush when serialization is done and let
	// postload run with the ones of the caller.
#if !WITH_PARTIAL_REQUEST_DURING_RECURSION
	FAsyncLoadingPostLoadGroup* PostLoadGroup = Package->PostLoadGroup ? Package->PostLoadGroup : Package->DeferredPostLoadGroup;
	if (PostLoadGroup && PostLoadGroup->SyncLoadContextId < ContextId)
	{
		PostLoadGroup->SyncLoadContextId = ContextId;
		for (FAsyncPackage2* PackageInPostLoadGroup : PostLoadGroup->Packages)
		{
			if (PackageInPostLoadGroup->SyncLoadContextId < ContextId)
			{
				IncludePackageInSyncLoadContextRecursive(ThreadState, ContextId, PackageInPostLoadGroup);
			}
		}
	}
#endif
	for (FAsyncPackage2* ImportedPackage : Package->Data.ImportedAsyncPackages)
	{
		if (ImportedPackage && ImportedPackage->SyncLoadContextId < ContextId)
		{
			IncludePackageInSyncLoadContextRecursive(ThreadState, ContextId, ImportedPackage);
		}
	}
	if (Package->Desc.Priority < MAX_int32)
	{
		Package->Desc.Priority = MAX_int32;
		UpdatePackagePriority(ThreadState, Package);
	}
}

UE_TRACE_EVENT_BEGIN(CUSTOM_LOADTIMER_LOG, CreateAsyncPackage, NoSync)
	UE_TRACE_EVENT_FIELD(uint64, PackageId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, PackageName)
UE_TRACE_EVENT_END()

bool FAsyncLoadingThread2::CreateAsyncPackagesFromQueue(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateAsyncPackagesFromQueue);
	
	bool bPackagesCreated = false;
	const int32 TimeSliceGranularity = ThreadState.UseTimeLimit() ? 4 : MAX_int32;

	FIoBatch IoBatch = IoDispatcher.NewBatch();

	FPackageStoreReadScope _(PackageStore);

	for (auto It = PendingPackages.CreateIterator(); It; ++It)
	{
		FAsyncPackage2* PendingPackage = *It;
		FPackageStoreEntry PackageEntry;
		EPackageStoreEntryStatus PendingPackageStatus = PackageStore.GetPackageStoreEntry(PendingPackage->Desc.PackageIdToLoad,
			PendingPackage->Desc.UPackageName, PackageEntry);
		if (PendingPackageStatus == EPackageStoreEntryStatus::Ok)
		{
			SCOPED_CUSTOM_LOADTIMER(CreateAsyncPackage)
				ADD_CUSTOM_LOADTIMER_META(CreateAsyncPackage, PackageId, PendingPackage->Desc.UPackageId.ValueForDebugging())
				ADD_CUSTOM_LOADTIMER_META(CreateAsyncPackage, PackageName, *WriteToString<256>(PendingPackage->Desc.UPackageName));
			InitializeAsyncPackageFromPackageStore(ThreadState, &IoBatch, PendingPackage, PackageEntry);
			PendingPackage->StartLoading(ThreadState, IoBatch);
			It.RemoveCurrent();
		}
		else if (PendingPackageStatus == EPackageStoreEntryStatus::Missing)
		{
			// Initialize package with a fake package store entry
			FPackageStoreEntry FakePackageEntry;
			InitializeAsyncPackageFromPackageStore(ThreadState, &IoBatch, PendingPackage, FakePackageEntry);
			// Simulate StartLoading() getting back a failed IoRequest and let it go through all package states
			PendingPackage->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForIo;
			PendingPackage->bLoadHasFailed = true;
			PendingPackage->GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier(&ThreadState);
			// Remove from PendingPackages
			It.RemoveCurrent();
		}
	}
	for (;;)
	{
		int32 NumDequeued = 0;
		while (NumDequeued < TimeSliceGranularity)
		{
			TOptional<FPackageRequest> OptionalRequest = PackageRequestQueue.Dequeue();
			if (!OptionalRequest.IsSet())
			{
				break;
			}

			--QueuedPackagesCounter;
			++NumDequeued;
			TRACE_COUNTER_SET(AsyncLoadingQueuedPackages, QueuedPackagesCounter);
		
			FPackageRequest& Request = OptionalRequest.GetValue();
			EPackageStoreEntryStatus PackageStatus = EPackageStoreEntryStatus::Missing;
			FPackageStoreEntry PackageEntry;
			FName PackageNameToLoad = Request.PackagePath.GetPackageFName();
			TCHAR NameBuffer[FName::StringBufferSize];
			uint32 NameLen = PackageNameToLoad.ToString(NameBuffer);
			FStringView PackageNameStr = FStringView(NameBuffer, NameLen);
			if (!FPackageName::IsValidLongPackageName(PackageNameStr))
			{
				FString NewPackageNameStr;
				if (FPackageName::TryConvertFilenameToLongPackageName(FString(PackageNameStr), NewPackageNameStr))
				{
					PackageNameToLoad = *NewPackageNameStr;
				}
			}

#if WITH_EDITOR
			const FCoreRedirectObjectName RedirectedPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, PackageNameToLoad));
			if (RedirectedPackageName.PackageName != PackageNameToLoad)
			{
				PackageNameToLoad = RedirectedPackageName.PackageName;
			}
#endif

			FPackageId PackageIdToLoad = FPackageId::FromName(PackageNameToLoad);
			FName UPackageName = PackageNameToLoad;
			{
				FName SourcePackageName;
				FPackageId RedirectedToPackageId;
				if (PackageStore.GetPackageRedirectInfo(PackageIdToLoad, SourcePackageName, RedirectedToPackageId))
				{
					PackageIdToLoad = RedirectedToPackageId;
					Request.PackagePath.Empty(); // We no longer know the path but it will be set again when serializing the package summary
					PackageNameToLoad = NAME_None;
					UPackageName = SourcePackageName;
				}
			}

			PackageStatus = PackageStore.GetPackageStoreEntry(PackageIdToLoad, UPackageName, PackageEntry);
			if (PackageStatus == EPackageStoreEntryStatus::Missing)
			{
				// While there is an active load request for (InName=/Temp/PackageABC_abc, InPackageToLoadFrom=/Game/PackageABC), then allow these requests too:
				// (InName=/Temp/PackageA_abc, InPackageToLoadFrom=/Temp/PackageABC_abc) and (InName=/Temp/PackageABC_xyz, InPackageToLoadFrom=/Temp/PackageABC_abc)
				FAsyncPackage2* Package = GetAsyncPackage(PackageIdToLoad);
				if (Package)
				{
					PackageIdToLoad = Package->Desc.PackageIdToLoad;
					Request.PackagePath = Package->Desc.PackagePathToLoad;
					PackageNameToLoad = Request.PackagePath.GetPackageFName();
					PackageStatus = PackageStore.GetPackageStoreEntry(PackageIdToLoad, UPackageName, PackageEntry);
				}
			}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
			bool bIsZenPackage = true;
			FName CorrectedPackageName;
			if (TryGetPackagePathFromFileSystem(PackageNameToLoad, UPackageName, Request.PackagePath))
			{
				bIsZenPackage = false;
				PackageStatus = EPackageStoreEntryStatus::Ok;
			}
#endif

			// Fixup CustomName to handle any input string that can be converted to a long package name.
			if (!Request.CustomName.IsNone())
			{
				NameLen = Request.CustomName.ToString(NameBuffer);
				PackageNameStr = FStringView(NameBuffer, NameLen);
				if (!FPackageName::IsValidLongPackageName(PackageNameStr))
				{
					FString NewPackageNameStr;
					if (FPackageName::TryConvertFilenameToLongPackageName(FString(PackageNameStr), NewPackageNameStr))
					{
						Request.CustomName = *NewPackageNameStr;
					}
				}
				UPackageName = Request.CustomName;
			}

			FAsyncPackageDesc2 PackageDesc = FAsyncPackageDesc2::FromPackageRequest(Request, UPackageName, PackageIdToLoad);
			if (PackageStatus == EPackageStoreEntryStatus::Missing)
			{
				QueueMissingPackage(ThreadState, PackageDesc, MoveTemp(Request.PackageLoadedDelegate), MoveTemp(Request.PackageProgressDelegate));
			}
			else
			{
				bool bInserted;
				FAsyncPackage2* Package = FindOrInsertPackage(ThreadState, PackageDesc, bInserted, nullptr, MoveTemp(Request.PackageLoadedDelegate), MoveTemp(Request.PackageProgressDelegate));
				checkf(Package, TEXT("Failed to find or insert package %s"), *PackageDesc.UPackageName.ToString());

				if (bInserted)
				{
					UE_ASYNC_PACKAGE_LOG(Verbose, PackageDesc, TEXT("CreateAsyncPackages: AddPackage"),
						TEXT("Start loading package."));
#if !UE_BUILD_SHIPPING
					if (FileOpenLogWrapper)
					{
						FileOpenLogWrapper->AddPackageToOpenLog(*PackageDesc.UPackageName.ToString());
					}
#endif
					if (PackageStatus == EPackageStoreEntryStatus::Pending)
					{
						PendingPackages.Add(Package);
					}
					else
					{
						SCOPED_CUSTOM_LOADTIMER(CreateAsyncPackage)
							ADD_CUSTOM_LOADTIMER_META(CreateAsyncPackage, PackageId, PackageDesc.UPackageId.ValueForDebugging())
							ADD_CUSTOM_LOADTIMER_META(CreateAsyncPackage, PackageName, NameBuffer);

						check(PackageStatus == EPackageStoreEntryStatus::Ok);
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
						if (!bIsZenPackage)
						{
							Package->InitializeLinkerLoadState(&PackageDesc.InstancingContext);
						}
						else
#endif
						{
							InitializeAsyncPackageFromPackageStore(ThreadState, &IoBatch, Package, PackageEntry);
						}
						Package->StartLoading(ThreadState, IoBatch);
					}
				}
				else
				{
					UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbose, PackageDesc, TEXT("CreateAsyncPackages: UpdatePackage"),
						TEXT("Package is already being loaded."));
					--PackagesWithRemainingWorkCounter;
					TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);
				}

				RequestIdToPackageMap.Add(PackageDesc.RequestID, Package);
			}
		}
		
		bPackagesCreated |= NumDequeued > 0;

		if (!NumDequeued || ThreadState.IsTimeLimitExceeded(TEXT("CreateAsyncPackagesFromQueue")))
		{
			break;
		}
	}

	IoBatch.Issue();
	
	return bPackagesCreated;
}

FEventLoadNode2::FEventLoadNode2(const FAsyncLoadEventSpec* InSpec, FAsyncPackage2* InPackage, int32 InImportOrExportIndex, int32 InBarrierCount)
	: Spec(InSpec)
	, Package(InPackage)
	, ImportOrExportIndex(InImportOrExportIndex)
	, BarrierCount(InBarrierCount)
{
	check(Spec);
	check(Package);
}

void FEventLoadNode2::DependsOn(FEventLoadNode2* Other)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DependsOn);
	check(!bIsDone.load(std::memory_order_relaxed));
	bool bExpected = false;
	// Set modification flag before checking the done flag
	// If we're currently in ProcessDependencies the done flag will have been set and we won't do anything
	// If ProcessDependencies is called during this call it will wait for the modification flag to be cleared
	while (!Other->bIsUpdatingDependencies.compare_exchange_strong(bExpected, true))
	{
		// Note: Currently only the async loading thread is calling DependsOn so this will never be contested
		bExpected = false;
	}
	if (!Other->bIsDone.load())
	{
		++BarrierCount;
		if (Other->DependenciesCount == 0)
		{
			Other->SingleDependent = this;
			Other->DependenciesCount = 1;
		}
		else
		{
			if (Other->DependenciesCount == 1)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DependsOnAlloc);
				FEventLoadNode2* FirstDependency = Other->SingleDependent;
				uint32 NewDependenciesCapacity = 4;
				Other->DependenciesCapacity = NewDependenciesCapacity;
				Other->MultipleDependents = Package->GetGraphAllocator().AllocArcs(NewDependenciesCapacity);
				Other->MultipleDependents[0] = FirstDependency;
			}
			else if (Other->DependenciesCount == Other->DependenciesCapacity)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DependsOnRealloc);
				FEventLoadNode2** OriginalDependents = Other->MultipleDependents;
				uint32 OldDependenciesCapcity = Other->DependenciesCapacity;
				SIZE_T OldDependenciesSize = OldDependenciesCapcity * sizeof(FEventLoadNode2*);
				uint32 NewDependenciesCapacity = OldDependenciesCapcity * 2;
				Other->DependenciesCapacity = NewDependenciesCapacity;
				Other->MultipleDependents = Package->GetGraphAllocator().AllocArcs(NewDependenciesCapacity);
				FMemory::Memcpy(Other->MultipleDependents, OriginalDependents, OldDependenciesSize);
				Package->GetGraphAllocator().FreeArcs(OriginalDependents, OldDependenciesCapcity);
			}
			Other->MultipleDependents[Other->DependenciesCount++] = this;
		}
	}
	Other->bIsUpdatingDependencies.store(false);
}

void FEventLoadNode2::AddBarrier()
{
	check(!bIsDone.load(std::memory_order_relaxed));
	++BarrierCount;
}

void FEventLoadNode2::AddBarrier(int32 Count)
{
	check(!bIsDone.load(std::memory_order_relaxed));
	BarrierCount += Count;
}

void FEventLoadNode2::ReleaseBarrier(FAsyncLoadingThreadState2* ThreadState)
{
	check(BarrierCount > 0);
	if (--BarrierCount == 0)
	{
		Fire(ThreadState);
	}
}

void FEventLoadNode2::Fire(FAsyncLoadingThreadState2* ThreadState)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(Fire);
	if (Spec->bExecuteImmediately && ThreadState)
	{
		EEventLoadNodeExecutionResult Result = Execute(*ThreadState);
		check(Result == EEventLoadNodeExecutionResult::Complete);
	}
	else
	{
		Spec->EventQueue->Push(ThreadState, this);
	}
}

EEventLoadNodeExecutionResult FEventLoadNode2::Execute(FAsyncLoadingThreadState2& ThreadState)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteEvent);
	check(BarrierCount.load(std::memory_order_relaxed) == 0);
	EEventLoadNodeExecutionResult Result;
	{
		ThreadState.CurrentlyExecutingEventNodeStack.Push(this);
		Result = Spec->Func(ThreadState, Package, ImportOrExportIndex);
		ThreadState.CurrentlyExecutingEventNodeStack.Pop();
	}
	if (Result == EEventLoadNodeExecutionResult::Complete)
	{
		ProcessDependencies(ThreadState);
	}
	return Result;
}

void FEventLoadNode2::ProcessDependencies(FAsyncLoadingThreadState2& ThreadState)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(ProcessDependencies);
	// Set the done flag before checking the modification flag
	bIsDone.store(true);
	while (bIsUpdatingDependencies.load())
	{
		FPlatformProcess::Sleep(0);
	}

	if (DependenciesCount == 1)
	{
		check(SingleDependent->BarrierCount > 0);
		if (--SingleDependent->BarrierCount == 0)
		{
			ThreadState.NodesToFire.Push(SingleDependent);
		}
	}
	else if (DependenciesCount != 0)
	{
		FEventLoadNode2** Current = MultipleDependents;
		FEventLoadNode2** End = MultipleDependents + DependenciesCount;
		for (; Current < End; ++Current)
		{
			FEventLoadNode2* Dependent = *Current;
			check(Dependent->BarrierCount > 0);
			if (--Dependent->BarrierCount == 0)
			{
				ThreadState.NodesToFire.Push(Dependent);
			}
		}
		Package->GetGraphAllocator().FreeArcs(MultipleDependents, DependenciesCapacity);
	}
	if (ThreadState.bShouldFireNodes)
	{
		ThreadState.bShouldFireNodes = false;
		while (ThreadState.NodesToFire.Num())
		{
			ThreadState.NodesToFire.Pop(EAllowShrinking::No)->Fire(&ThreadState);
		}
		ThreadState.bShouldFireNodes = true;
	}
}

uint64 FEventLoadNode2::GetSyncLoadContextId() const
{
	return Package->GetSyncLoadContextId();
}

FAsyncLoadEventQueue2::FAsyncLoadEventQueue2()
{
}

FAsyncLoadEventQueue2::~FAsyncLoadEventQueue2()
{
}

void FAsyncLoadEventQueue2::Push(FAsyncLoadingThreadState2* ThreadState, FEventLoadNode2* Node)
{
	if (OwnerThread == ThreadState)
	{
		PushLocal(Node);
	}
	else
	{
		PushExternal(Node);
	}
}

void FAsyncLoadEventQueue2::PushLocal(FEventLoadNode2* Node)
{
	check(!Node->QueueStatus);
	int32 Priority = Node->Package->Desc.Priority;
	Node->QueueStatus = FEventLoadNode2::QueueStatus_Local;
	LocalQueue.Push(Node, Priority);
}

void FAsyncLoadEventQueue2::PushExternal(FEventLoadNode2* Node)
{
	{
		int32 Priority = Node->Package->Desc.Priority;
		FScopeLock Lock(&ExternalCritical);
		check(!Node->QueueStatus);
		Node->QueueStatus = FEventLoadNode2::QueueStatus_External;
		ExternalQueue.Push(Node, Priority);
		UpdateExternalQueueState();
	}
	if (Zenaphore)
	{
		Zenaphore->NotifyOne();
	}
	if (WakeEvent)
	{
		WakeEvent->Notify();
	}
}

bool FAsyncLoadEventQueue2::PopAndExecute(FAsyncLoadingThreadState2& ThreadState)
{
	if (TimedOutEventNode)
	{
		// Backup and reset the node before executing it in case we end up with a recursive flush call, we don't want the same node run multiple time.
		FEventLoadNode2* LocalTimedOutEventNode = TimedOutEventNode;
		TimedOutEventNode = nullptr;

		EEventLoadNodeExecutionResult Result = LocalTimedOutEventNode->Execute(ThreadState);
		if (Result == EEventLoadNodeExecutionResult::Timeout)
		{
			TimedOutEventNode = LocalTimedOutEventNode;
		}
		return true;
	}

	bool bPopFromExternalQueue = false;
	int32 MaxPriorityInExternalQueue;
	if (GetMaxPriorityInExternalQueue(MaxPriorityInExternalQueue))
	{
		if (LocalQueue.IsEmpty() || MaxPriorityInExternalQueue > LocalQueue.GetMaxPriority())
		{
			bPopFromExternalQueue = true;
		}
	}
	FEventLoadNode2* Node;
	if (bPopFromExternalQueue)
	{
		FScopeLock Lock(&ExternalCritical);
		Node = ExternalQueue.Pop();
		check(Node);
		UpdateExternalQueueState();
	}
	else
	{
		Node = LocalQueue.Pop();
	}
	if (!Node)
	{
		return false;
	}
	Node->QueueStatus = FEventLoadNode2::QueueStatus_None;
	
	EEventLoadNodeExecutionResult Result = Node->Execute(ThreadState);
	if (Result == EEventLoadNodeExecutionResult::Timeout)
	{
		TimedOutEventNode = Node;
	}
	return true;
}

bool FAsyncLoadEventQueue2::ExecuteSyncLoadEvents(FAsyncLoadingThreadState2& ThreadState)
{
	check(!ThreadState.SyncLoadContextStack.IsEmpty());
	FAsyncLoadingSyncLoadContext& SyncLoadContext = *ThreadState.SyncLoadContextStack.Top();

	int32 ThisCallCounter = ++ExecuteSyncLoadEventsCallCounter;

	auto ShouldExecuteNode = [&SyncLoadContext](FEventLoadNode2& Node) -> bool
	{
		return Node.Package->SyncLoadContextId >= SyncLoadContext.ContextId;
	};

	bool bDidSomething = false;
	if (TimedOutEventNode && ShouldExecuteNode(*TimedOutEventNode))
	{
		// Backup and reset the node before executing it in case we end up with a recursive flush call, we don't want the same node run multiple time.
		FEventLoadNode2* LocalTimedOutEventNode = TimedOutEventNode;
		TimedOutEventNode = nullptr;

		EEventLoadNodeExecutionResult Result = LocalTimedOutEventNode->Execute(ThreadState);
		check(Result == EEventLoadNodeExecutionResult::Complete); // we can't timeout during a sync load operation
		bDidSomething = true;
	}

	int32 MaxPriorityInExternalQueue;
	bool bTakeFromExternalQueue = GetMaxPriorityInExternalQueue(MaxPriorityInExternalQueue) && MaxPriorityInExternalQueue == MAX_int32;
	if (bTakeFromExternalQueue)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MergeIntoLocalQueue);
		// Take all the max prio items from the external queue and put in the local queue. This breaks the queue status value
		// of the items in the external queue but we know that we'll never reprioritize them again so it doesn't matter
		FScopeLock Lock(&ExternalCritical);
		ExternalQueue.MergeInto(LocalQueue, MAX_int32);
		UpdateExternalQueueState();
	}
	for (auto It = LocalQueue.CreateIterator(MAX_int32); It; ++It)
	{
		FEventLoadNode2& Node = *It;
		if (ShouldExecuteNode(Node))
		{
			It.RemoveCurrent();
			Node.QueueStatus = FEventLoadNode2::QueueStatus_None;
			EEventLoadNodeExecutionResult Result = Node.Execute(ThreadState);
			check(Result == EEventLoadNodeExecutionResult::Complete); // we can't timeout during a sync load operation
			if (ExecuteSyncLoadEventsCallCounter != ThisCallCounter)
			{
				// ExecuteSyncLoadEvents was called recursively and our view of the list might have been compromised, start over
				return true;
			}
			bDidSomething = true;
		}
	}
	if (!bDidSomething && ThreadState.bIsAsyncLoadingThread)
	{
		return PopAndExecute(ThreadState);
	}
	return bDidSomething;
}

void FAsyncLoadEventQueue2::UpdatePackagePriority(FAsyncPackage2* Package)
{
	FScopeLock Lock(&ExternalCritical);
	auto ReprioritizeNode = [this](FEventLoadNode2& Node)
	{
		if (Node.Spec->EventQueue == this && Node.Priority < Node.Package->Desc.Priority)
		{
			if (Node.QueueStatus == FEventLoadNode2::QueueStatus_Local)
			{
				LocalQueue.Reprioritize(&Node, Node.Package->Desc.Priority);
			}
			else if (Node.QueueStatus == FEventLoadNode2::QueueStatus_External)
			{
				ExternalQueue.Reprioritize(&Node, Node.Package->Desc.Priority);
			}
		}
	};

	for (FEventLoadNode2& Node : Package->PackageNodes)
	{
		ReprioritizeNode(Node);
	}
	for (FEventLoadNode2& Node : Package->Data.ExportBundleNodes)
	{
		ReprioritizeNode(Node);
	}
	UpdateExternalQueueState();
}

FUObjectSerializeContext* FAsyncPackage2::GetSerializeContext()
{
	return FUObjectThreadContext::Get().GetSerializeContext();
}

void FAsyncPackage2::SetupScriptDependencies()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SetupScriptDependencies);

	// UObjectLoadAllCompiledInDefaultProperties is creating CDOs from a flat list.
	// During initial laod, if a CDO called LoadObject for this package it may depend on other CDOs later in the list.
	// Then collect them here, and wait for them to be created before allowing this package to proceed.
	TArray<UClass*, TInlineAllocator<8>> UnresolvedCDOs;
	ImportStore.GetUnresolvedCDOs(HeaderData, UnresolvedCDOs);
#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		ImportStore.GetUnresolvedCDOs(*OptionalSegmentHeaderData, UnresolvedCDOs);
	}
#endif
	if (!UnresolvedCDOs.IsEmpty())
	{
		AsyncLoadingThread.AddPendingCDOs(this, UnresolvedCDOs);
	}
}


void FGlobalImportStore::FindAllScriptObjects(bool bVerifyOnly)
{
	TStringBuilder<FName::StringBufferSize> Name;
	TArray<UPackage*> ScriptPackages;
	TArray<UObject*> Objects;
	FindAllRuntimeScriptPackages(ScriptPackages);

	for (UPackage* Package : ScriptPackages)
	{
#if WITH_EDITOR
		Name.Reset();
		Package->GetPathName(nullptr, Name);
		FPackageObjectIndex PackageGlobalImportIndex = FPackageObjectIndex::FromScriptPath(Name);
		if (!ScriptObjects.Contains(PackageGlobalImportIndex))
		{
			if (bVerifyOnly)
			{
				UE_LOG(LogStreaming, Display, TEXT("Script package %s (0x%016llX) is missing a NotifyRegistrationEvent from the initial load phase."),
					*Package->GetFullName(),
					PackageGlobalImportIndex.Value());
			}
			else
			{
				ScriptObjects.Add(PackageGlobalImportIndex, Package);
			}
		}
#endif
		Objects.Reset();
		GetObjectsWithOuter(Package, Objects, /*bIncludeNestedObjects*/true);
		for (UObject* Object : Objects)
		{
			if (Object->HasAnyFlags(RF_Public))
			{
				Name.Reset();
				Object->GetPathName(nullptr, Name);
				FPackageObjectIndex GlobalImportIndex = FPackageObjectIndex::FromScriptPath(Name);
				if (!ScriptObjects.Contains(GlobalImportIndex))
				{
					if (bVerifyOnly)
					{
						UE_LOG(LogStreaming, Warning, TEXT("Script object %s (0x%016llX) is missing a NotifyRegistrationEvent from the initial load phase."),
							*Object->GetFullName(),
							GlobalImportIndex.Value());
					}
					else
					{
						ScriptObjects.Add(GlobalImportIndex, Object);
					}
				}
			}
		}
	}
}

void FGlobalImportStore::RegistrationComplete()
{
#if WITH_EDITOR
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindAllScriptObjects);
		FindAllScriptObjects(/*bVerifyOnly*/false);
	}
#elif DO_CHECK
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindAllScriptObjectsVerify);
		FindAllScriptObjects(/*bVerifyOnly*/true);
	}
#endif
	ScriptObjects.Shrink();
}

void FAsyncPackage2::ImportPackagesRecursiveInner(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore, FAsyncPackageHeaderData& Header)
{
	const TArrayView<FPackageId>& ImportedPackageIds = Header.ImportedPackageIds;
	const int32 ImportedPackageCount = ImportedPackageIds.Num();
	if (!ImportedPackageCount)
	{
		return;
	}
	TArray<FName>& ImportedPackageNames = Header.ImportedPackageNames;
	bool bHasImportedPackageNames = !ImportedPackageNames.IsEmpty();
	check(ImportedPackageNames.Num() == 0 || ImportedPackageNames.Num() == ImportedPackageCount);
	for (int32 LocalImportedPackageIndex = 0; LocalImportedPackageIndex < ImportedPackageCount; ++LocalImportedPackageIndex)
	{
		FPackageId ImportedPackageId = ImportedPackageIds[LocalImportedPackageIndex];
		EPackageStoreEntryStatus ImportedPackageStatus = EPackageStoreEntryStatus::Missing;
		FPackageStoreEntry ImportedPackageEntry;
		FName ImportedPackageUPackageName = bHasImportedPackageNames ? ImportedPackageNames[LocalImportedPackageIndex] : NAME_None;
		FName ImportedPackageNameToLoad = ImportedPackageUPackageName;
		FPackageId ImportedPackageIdToLoad = ImportedPackageId;

#if WITH_EDITOR
		if (!ImportedPackageNameToLoad.IsNone())
		{
			const FCoreRedirectObjectName RedirectedPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, ImportedPackageNameToLoad));
			if (RedirectedPackageName.PackageName != ImportedPackageNameToLoad)
			{
				ImportedPackageNameToLoad = RedirectedPackageName.PackageName;
				ImportedPackageIdToLoad = FPackageId::FromName(ImportedPackageNameToLoad);
				ImportedPackageUPackageName = ImportedPackageNameToLoad;
				ImportedPackageId = ImportedPackageIdToLoad;
				// Rewrite the import table
				ImportedPackageIds[LocalImportedPackageIndex] = ImportedPackageId;
				ImportedPackageNames[LocalImportedPackageIndex] = ImportedPackageUPackageName;
			}
		}
#endif

		bool bIsInstanced = false;
#if WITH_EDITORONLY_DATA && ALT2_ENABLE_LINKERLOAD_SUPPORT
		if (bHasImportedPackageNames && LinkerLoadState.IsSet())
		{
			// Use the instancing context remap from disk package name to upackage name
			const FLinkerInstancingContext& InstancingContext = LinkerLoadState->Linker->GetInstancingContext();
			ImportedPackageUPackageName = InstancingContext.RemapPackage(ImportedPackageNameToLoad);
			if (ImportedPackageUPackageName != ImportedPackageNameToLoad)
			{
				bIsInstanced = true;
				if (ImportedPackageUPackageName.IsNone())
				{
					ImportedPackageIdToLoad = FPackageId::FromName(NAME_None);
					ImportedPackageNameToLoad = NAME_None;
				}

				ImportedPackageId = FPackageId::FromName(ImportedPackageUPackageName);
				// Rewrite the import table
				ImportedPackageIds[LocalImportedPackageIndex] = ImportedPackageId;
				ImportedPackageNames[LocalImportedPackageIndex] = ImportedPackageUPackageName;
			}
		}
#endif

		{
			FName SourcePackageName;
			FPackageId RedirectedToPackageId;
			if (PackageStore.GetPackageRedirectInfo(ImportedPackageIdToLoad, SourcePackageName, RedirectedToPackageId))
			{
				if (ImportedPackageUPackageName.IsNone())
				{
					ImportedPackageUPackageName = SourcePackageName;
				}
				ImportedPackageIdToLoad = RedirectedToPackageId;
				ImportedPackageNameToLoad = NAME_None;
			}
		}
		ImportedPackageStatus = PackageStore.GetPackageStoreEntry(ImportedPackageIdToLoad, ImportedPackageUPackageName,
			ImportedPackageEntry);

		FPackagePath ImportedPackagePath;
#if ALT2_ENABLE_LINKERLOAD_SUPPORT || WITH_EDITOR
		bool bIsZenPackage = !LinkerLoadState.IsSet();
		bool bIsZenPackageImport = true;
		FName CorrectedPackageName;
		if (AsyncLoadingThread.TryGetPackagePathFromFileSystem(ImportedPackageNameToLoad, ImportedPackageUPackageName, ImportedPackagePath))
		{
			bIsZenPackageImport = false;
			ImportedPackageStatus = EPackageStoreEntryStatus::Ok;
		}
#else
		constexpr bool bIsZenPackage = true;
		constexpr bool bIsZenPackageImport = true;
#endif

		FLoadedPackageRef& ImportedPackageRef = ImportStore.AddImportedPackageReference(ImportedPackageId, ImportedPackageUPackageName);
#if WITH_EDITOR
		if (AsyncLoadingThread.UncookedPackageLoader && ImportedPackageStatus == EPackageStoreEntryStatus::Ok && !bIsZenPackageImport)
		{
			UPackage* UncookedPackage = ImportedPackageRef.GetPackage();
			if (!ImportedPackageRef.AreAllPublicExportsLoaded())
			{
				UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ImportPackages: LoadUncookedImport"), TEXT("Loading imported uncooked package '%s' '0x%llX'"), *ImportedPackageNameToLoad.ToString(), ImportedPackageId.ValueForDebugging());
				check(IsInGameThread());
				IoBatch.Issue(); // The batch might already contain requests for packages being imported from the uncooked one we're going to load so make sure that those are started before blocking
				int32 ImportRequestId = AsyncLoadingThread.UncookedPackageLoader->LoadPackage(ImportedPackagePath, NAME_None, FLoadPackageAsyncDelegate(), PKG_None, INDEX_NONE, 0, nullptr, LOAD_None);
				AsyncLoadingThread.UncookedPackageLoader->FlushLoading({ImportRequestId});
				UncookedPackage = FindObjectFast<UPackage>(nullptr, ImportedPackagePath.GetPackageFName());
				ImportedPackageRef.SetPackage(UncookedPackage);
				if (UncookedPackage)
				{
					UncookedPackage->SetCanBeImportedFlag(true);
					UncookedPackage->SetPackageId(ImportedPackageId);
					UncookedPackage->SetInternalFlags(EInternalObjectFlags::LoaderImport);
					ImportedPackageRef.SetAllPublicExportsLoaded();
				}
			}
			if (UncookedPackage)
			{
				ForEachObjectWithOuter(UncookedPackage, [this, ImportedPackageId](UObject* Object)
				{
					if (Object->HasAllFlags(RF_Public))
					{
						Object->SetInternalFlags(EInternalObjectFlags::LoaderImport);

						TArray<FName, TInlineAllocator<64>> FullPath;
						FullPath.Add(Object->GetFName());
						UObject* Outer = Object->GetOuter();
						while (Outer)
						{
							FullPath.Add(Outer->GetFName());
							Outer = Outer->GetOuter();
						}
						TStringBuilder<256> PackageRelativeExportPath;
						for (int32 PathIndex = FullPath.Num() - 2; PathIndex >= 0; --PathIndex)
						{
							TCHAR NameStr[FName::StringBufferSize];
							uint32 NameLen = FullPath[PathIndex].ToString(NameStr);
							for (uint32 I = 0; I < NameLen; ++I)
							{
								NameStr[I] = TChar<TCHAR>::ToLower(NameStr[I]);
							}
							PackageRelativeExportPath.AppendChar('/');
							PackageRelativeExportPath.Append(FStringView(NameStr, NameLen));
						}
						uint64 ExportHash = CityHash64(reinterpret_cast<const char*>(PackageRelativeExportPath.GetData() + 1), (PackageRelativeExportPath.Len() - 1) * sizeof(TCHAR));
						ImportStore.StoreGlobalObject(ImportedPackageId, ExportHash, Object);
					}
				}, /* bIncludeNestedObjects*/ true);
			}
			else
			{
				ImportedPackageRef.SetHasFailed();
				UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("ImportPackages: SkipPackage"),
					TEXT("Failed to load uncooked imported package with id '0x%llX' ('%s')"), ImportedPackageId.ValueForDebugging(), *ImportedPackageNameToLoad.ToString());
			}
			continue;
		}
#endif

		FAsyncPackage2* ImportedPackage = nullptr;
		bool bInserted = false;
		bool bIsFullyLoaded = ImportedPackageRef.AreAllPublicExportsLoaded();
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
		if (!bIsZenPackageImport && (!ImportedPackageRef.HasPackage() || (!ImportedPackageRef.GetPackage()->GetLinker() && !ImportedPackageRef.GetPackage()->HasAnyPackageFlags(PKG_InMemoryOnly))))
		{
			// If we're importing a linker load package and it doesn't have its linker we need to reload it, otherwise we can't reliably link to its imports
			// Note: Legacy loader path appears to do this only for uncooked packages in the editor?
			bIsFullyLoaded = false;
		}
#endif
		if (bIsFullyLoaded)
		{
			ImportedPackage = AsyncLoadingThread.FindAsyncPackage(ImportedPackageId);
			if (!ImportedPackage)
			{
				continue;
			}
			bInserted = false;
		}
		else if (ImportedPackageStatus == EPackageStoreEntryStatus::Missing)
		{
			if (!ImportedPackageRef.HasPackage()) // If we found a package it's not actually missing but we can't load it anyway
			{
				UE_ASYNC_PACKAGE_CLOG(!ImportedPackageUPackageName.IsNone(), Display, Desc, TEXT("ImportPackages: SkipPackage"),
					TEXT("Skipping non mounted imported package %s (0x%llX)"), *ImportedPackageNameToLoad.ToString(), ImportedPackageId.ValueForDebugging());
				ImportedPackageRef.SetIsMissingPackage();
			}
			continue;
		}
		else
		{
			FAsyncPackageDesc2 PackageDesc = FAsyncPackageDesc2::FromPackageImport(Desc, ImportedPackageUPackageName, ImportedPackageId, ImportedPackageIdToLoad, MoveTemp(ImportedPackagePath));
			ImportedPackage = AsyncLoadingThread.FindOrInsertPackage(ThreadState, PackageDesc, bInserted, this);
		}

		checkf(ImportedPackage, TEXT("Failed to find or insert imported package with id '%s'"), *FormatPackageId(ImportedPackageId));
		TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(this, ImportedPackage);

		if (bInserted)
		{
			UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ImportPackages: AddPackage"),
			TEXT("Start loading imported package with id '%s'"), *FormatPackageId(ImportedPackageId));
			++AsyncLoadingThread.PackagesWithRemainingWorkCounter;
			TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, AsyncLoadingThread.PackagesWithRemainingWorkCounter);
		}
		else
		{
			UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("ImportPackages: UpdatePackage"),
				TEXT("Imported package with id '%s' is already being loaded."), *FormatPackageId(ImportedPackageId));
		}
		ImportedPackage->AddRef();
		Header.ImportedAsyncPackagesView[LocalImportedPackageIndex] = ImportedPackage;

		if (bIsZenPackage != bIsZenPackageImport)
		{
			UE_ASYNC_PACKAGE_LOG(VeryVerbose, Desc, TEXT("ImportPackages: AddDependency"),
				TEXT("Adding package dependency to %s import '%s'."), bIsZenPackageImport ? TEXT("cooked") : TEXT("non-cooked"), *ImportedPackage->Desc.UPackageName.ToString());

			// When importing a linker load package from a zen package or vice versa we need to wait for all the exports in the imported package to be created
			// and serialized before we can start processing our own exports
			GetPackageNode(Package_DependenciesReady).DependsOn(&ImportedPackage->GetPackageNode(Package_ExportsSerialized));
		}

		if (bInserted)
		{
			if (ImportedPackageStatus == EPackageStoreEntryStatus::Pending)
			{
				AsyncLoadingThread.PendingPackages.Add(ImportedPackage);
			}
			else
			{
				check(ImportedPackageStatus == EPackageStoreEntryStatus::Ok);
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
				if (!bIsZenPackageImport)
				{
					// Only propagate the instancing context if the imported package is also instanced and it isn't a zen package
					ImportedPackage->InitializeLinkerLoadState(bIsZenPackage || !bIsInstanced ? nullptr : &LinkerLoadState->Linker->GetInstancingContext());
				}
				else
#endif
				{
					// TODO: Here we should probably also propagate the instancing context if the imported package is also instanced (similar to the call to InitializeLinkerLoadState done above)
					AsyncLoadingThread.InitializeAsyncPackageFromPackageStore(ThreadState, &IoBatch, ImportedPackage, ImportedPackageEntry);
				}
				ImportedPackage->StartLoading(ThreadState, IoBatch);
			}
		}
	}
}

void FAsyncPackage2::ImportPackagesRecursive(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore)
{
	if (bHasStartedImportingPackages)
	{
		return;
	}
	bHasStartedImportingPackages = true;

	if (Data.ImportedAsyncPackages.IsEmpty())
	{
		return;
	}

	ImportPackagesRecursiveInner(ThreadState, IoBatch, PackageStore, HeaderData);
#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		ImportPackagesRecursiveInner(ThreadState, IoBatch, PackageStore, OptionalSegmentHeaderData.GetValue());
	}
#endif

	if (SyncLoadContextId)
	{
		for (FAsyncPackage2* ImportedPackage : Data.ImportedAsyncPackages)
		{
			if (ImportedPackage)
			{
				AsyncLoadingThread.IncludePackageInSyncLoadContextRecursive(ThreadState, SyncLoadContextId, ImportedPackage);
			}
		}
	}

	UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("ImportPackages: ImportsDone"),
		TEXT("All imported packages are now being loaded."));
}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
void FAsyncPackage2::InitializeLinkerLoadState(const FLinkerInstancingContext* InstancingContext)
{
	LinkerLoadState.Emplace();
	CreateUPackage();
	CreateLinker(InstancingContext);
}

void FAsyncPackage2::CreateLinker(const FLinkerInstancingContext* InstancingContext)
{
	uint32 LinkerFlags = (LOAD_Async | LOAD_NoVerify | LOAD_SkipLoadImportedPackages | Desc.LoadFlags);
#if WITH_EDITOR
	if ((!FApp::IsGame() || GIsEditor) && (Desc.PackageFlags & PKG_PlayInEditor) != 0)
	{
		LinkerFlags |= LOAD_PackageForPIE;
	}
#endif
	FLinkerLoad* Linker = FLinkerLoad::FindExistingLinkerForPackage(LinkerRoot);
	if (!Linker)
	{
		FUObjectSerializeContext* LoadContext = GetSerializeContext();
#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
		Linker = new FLinkerLoad(LinkerRoot, Desc.PackagePathToLoad, LinkerFlags, InstancingContext ? *InstancingContext : FLinkerInstancingContext());
		Linker->SetSerializeContext(LoadContext);
		LinkerRoot->SetLinker(Linker);
		FLinkerLoadArchive2* Loader = new FLinkerLoadArchive2(Desc.PackagePathToLoad);
		Linker->SetLoader(Loader, Loader->NeedsEngineVersionChecks());
#else
		Linker = FLinkerLoad::CreateLinkerAsync(LoadContext, LinkerRoot, Desc.PackagePathToLoad, LinkerFlags, InstancingContext, TFunction<void()>([]() {}));
#endif
	}
	else
	{
		Linker->LoadFlags |= LinkerFlags;
	}
	check(Linker);
	check(Linker->LinkerRoot == LinkerRoot);
	check(!Linker->AsyncRoot);
	TRACE_LOADTIME_ASYNC_PACKAGE_LINKER_ASSOCIATION(this, Linker);
	Linker->AsyncRoot = this;
	LinkerLoadState->Linker = Linker;
#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
	Linker->ResetStatusInfo();
#endif
}

void FAsyncPackage2::DetachLinker()
{
	if (LinkerLoadState.IsSet() && LinkerLoadState->Linker)
	{
		// We're no longer keeping the imports alive so clear them from the linker
		for (FObjectImport& ObjectImport : LinkerLoadState->Linker->ImportMap)
		{
			ObjectImport.XObject = nullptr;
			ObjectImport.SourceLinker = nullptr;
			ObjectImport.SourceIndex = INDEX_NONE;
		}
		check(LinkerLoadState->Linker->AsyncRoot == this);
		LinkerLoadState->Linker->AsyncRoot = nullptr;
		LinkerLoadState->Linker = nullptr;
	}
}
#endif

void FAsyncPackage2::StartLoading(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StartLoading);

	LoadStartTime = FPlatformTime::Seconds();

	CallProgressCallbacks(EAsyncLoadingProgress::Started);

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForIo;

	FIoReadOptions ReadOptions;
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (LinkerLoadState.IsSet())
	{
#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
		static_cast<FLinkerLoadArchive2*>(LinkerLoadState->Linker->GetLoader())->BeginRead(&GetPackageNode(EEventLoadNode2::Package_ProcessSummary));
#else
		GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier(&ThreadState);
#endif
		return;
	}
#endif

#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.IncrementExchange() + 1;
		TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);

		GetPackageNode(EEventLoadNode2::Package_ProcessSummary).AddBarrier();
		OptionalSegmentSerializationState->IoRequest = IoBatch.ReadWithCallback(CreateIoChunkId(Desc.PackageIdToLoad.Value(), 1, EIoChunkType::ExportBundleData),
			ReadOptions,
			Desc.Priority,
			[this](TIoStatusOr<FIoBuffer> Result)
			{
				if (!Result.IsOk())
				{
					UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("StartBundleIoRequests: FailedRead"),
						TEXT("Failed reading optional chunk for package: %s"), *Result.Status().ToString());
					bLoadHasFailed = true;
				}
				int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.DecrementExchange() - 1;
				TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);
				FAsyncLoadingThread2& LocalAsyncLoadingThread = AsyncLoadingThread;
				GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier(nullptr);
				if (LocalPendingIoRequestsCounter == 0)
				{
					LocalAsyncLoadingThread.AltZenaphore.NotifyOne();
				}
			});
	}
#endif

	int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.IncrementExchange() + 1;
	TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);

	SerializationState.IoRequest = IoBatch.ReadWithCallback(CreatePackageDataChunkId(Desc.PackageIdToLoad),
		ReadOptions,
		Desc.Priority,
		[this](TIoStatusOr<FIoBuffer> Result)
		{
			if (Result.IsOk())
			{
				TRACE_COUNTER_ADD(AsyncLoadingTotalLoaded, Result.ValueOrDie().DataSize());
				CSV_CUSTOM_STAT_DEFINED(FrameCompletedExportBundleLoadsKB, float((double)Result.ValueOrDie().DataSize() / 1024.0), ECsvCustomStatOp::Accumulate);
			}
			else
			{
				UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("StartBundleIoRequests: FailedRead"),
					TEXT("Failed reading chunk for package: %s"), *Result.Status().ToString());
				bLoadHasFailed = true;
			}
			int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.DecrementExchange() - 1;
			TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);
			FAsyncLoadingThread2& LocalAsyncLoadingThread = AsyncLoadingThread;
			GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier(nullptr);
			if (LocalPendingIoRequestsCounter == 0)
			{
				LocalAsyncLoadingThread.AltZenaphore.NotifyOne();
			}
		});

	if (!Data.ShaderMapHashes.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(StartShaderMapRequests);
		auto ReadShaderMapFunc = [this, &IoBatch](const FIoChunkId& ChunkId, FGraphEventRef GraphEvent)
		{
			GetPackageNode(Package_ExportsSerialized).AddBarrier();
			int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.IncrementExchange() + 1;
			TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);
			return IoBatch.ReadWithCallback(ChunkId, FIoReadOptions(), Desc.Priority,
				[this, GraphEvent](TIoStatusOr<FIoBuffer> Result)
				{
					GraphEvent->DispatchSubsequents();
					int32 LocalPendingIoRequestsCounter = AsyncLoadingThread.PendingIoRequestsCounter.DecrementExchange() - 1;
					TRACE_COUNTER_SET(AsyncLoadingPendingIoRequests, LocalPendingIoRequestsCounter);
					FAsyncLoadingThread2& LocalAsyncLoadingThread = AsyncLoadingThread;
					GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(nullptr);
					if (LocalPendingIoRequestsCounter == 0)
					{
						LocalAsyncLoadingThread.AltZenaphore.NotifyOne();
					}
				});
		};
		FCoreDelegates::PreloadPackageShaderMaps.ExecuteIfBound(Data.ShaderMapHashes, ReadShaderMapFunc);
	}
}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT

EEventLoadNodeExecutionResult FAsyncPackage2::ProcessLinkerLoadPackageSummary(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessLinkerLoadPackageSummary);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(LinkerLoadState->Linker->GetPackagePath().GetPackageFName(), ELLMTagSet::Assets);
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(NAME_None, NAME_None, LinkerLoadState->Linker->GetPackagePath().GetPackageFName());
	SCOPED_LOADTIMER_ASSET_TEXT(*LinkerLoadState->Linker->GetDebugName());

#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
	FLinkerLoad::ELinkerStatus LinkerResult = FLinkerLoad::LINKER_Failed;
	if (!LinkerLoadState->Linker->GetLoader()->IsError())
	{
		LinkerLoadState->Linker->bUseTimeLimit = false;
		TRACE_LOADTIME_PROCESS_SUMMARY_SCOPE(LinkerLoadState->Linker);
		LinkerResult = LinkerLoadState->Linker->ProcessPackageSummary(nullptr);
	}
#else
	TRACE_LOADTIME_PROCESS_SUMMARY_SCOPE(this);
	FLinkerLoad::ELinkerStatus LinkerResult = LinkerLoadState->Linker->Tick(/* RemainingTimeLimit */ 0.0, /* bUseTimeLimit */ false, /* bUseFullTimeLimit */ false, nullptr);
#endif
	check(LinkerResult != FLinkerLoad::LINKER_TimedOut); // TODO: Add support for timeouts here
	if (LinkerResult == FLinkerLoad::LINKER_Failed)
	{
		bLoadHasFailed = true;
	}
	check(LinkerLoadState->Linker->HasFinishedInitialization() || bLoadHasFailed);

	LinkerLoadState->LinkerLoadHeaderData.ImportMap.SetNum(LinkerLoadState->Linker->ImportMap.Num());
	TArray<FName, TInlineAllocator<128>> ImportedPackageNames;
	TArray<FPackageId, TInlineAllocator<128>> ImportedPackageIds;
	for (int32 ImportIndex = 0; ImportIndex < LinkerLoadState->Linker->ImportMap.Num(); ++ImportIndex)
	{
		const FObjectImport& LinkerImport = LinkerLoadState->Linker->ImportMap[ImportIndex];
		TArray<int32, TInlineAllocator<128>> PathComponents;
		int32 PathIndex = ImportIndex;
		PathComponents.Push(PathIndex);
		while (LinkerLoadState->Linker->ImportMap[PathIndex].OuterIndex.IsImport() && !LinkerLoadState->Linker->ImportMap[PathIndex].HasPackageName())
		{
			PathIndex = LinkerLoadState->Linker->ImportMap[PathIndex].OuterIndex.ToImport();
			PathComponents.Push(PathIndex);
		}
		int32 PackageImportIndex = PathComponents.Top();
		FObjectImport& PackageImport = LinkerLoadState->Linker->ImportMap[PackageImportIndex];
		FName ImportPackageName;
		const bool bImportHasPackageName = PackageImport.HasPackageName();
		if (bImportHasPackageName)
		{
			ImportPackageName = PackageImport.GetPackageName();
		}
		else
		{
			ImportPackageName =  PackageImport.ObjectName;
		}
		TCHAR NameStr[FName::StringBufferSize];
		uint32 NameLen = ImportPackageName.ToString(NameStr);
		bool bIsScriptImport = FPackageName::IsScriptPackage(FStringView(NameStr, NameLen));
		if (bIsScriptImport)
		{
			check(!bImportHasPackageName);
			TStringBuilder<256> FullPath;
			while (!PathComponents.IsEmpty())
			{
				NameLen = LinkerLoadState->Linker->ImportMap[PathComponents.Pop(EAllowShrinking::No)].ObjectName.ToString(NameStr);
				FPathViews::Append(FullPath, FStringView(NameStr, NameLen));
				LinkerLoadState->LinkerLoadHeaderData.ImportMap[ImportIndex] = FPackageObjectIndex::FromScriptPath(FullPath);
			}
		}
		else
		{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
			auto AssetRegistry = IAssetRegistryInterface::GetPtr();
			if (UE::LinkerLoad::CanLazyImport(*AssetRegistry, LinkerImport, *LinkerLoadState->Linker))
			{
				continue;
			}
#endif
			FPackageId ImportedPackageId = FPackageId::FromName(ImportPackageName);
			int32 ImportedPackageIndex = ImportedPackageIds.AddUnique(ImportedPackageId);
			if (ImportedPackageIndex == ImportedPackageNames.Num())
			{
				ImportedPackageNames.AddDefaulted();
			}
			ImportedPackageNames[ImportedPackageIndex] = ImportPackageName;
			bool bIsPackageImport = ImportIndex == PackageImportIndex;
			if (!bIsPackageImport || bImportHasPackageName)
			{
				if (!bImportHasPackageName)
				{
					PathComponents.Pop(EAllowShrinking::No);
				}
				TStringBuilder<256> PackageRelativeExportPath;
				while (!PathComponents.IsEmpty())
				{
					NameLen = LinkerLoadState->Linker->ImportMap[PathComponents.Pop(EAllowShrinking::No)].ObjectName.ToString(NameStr);
					for (uint32 I = 0; I < NameLen; ++I)
					{
						NameStr[I] = TChar<TCHAR>::ToLower(NameStr[I]);
					}
					PackageRelativeExportPath.AppendChar('/');
					PackageRelativeExportPath.Append(FStringView(NameStr, NameLen));
				}
				FPackageImportReference PackageImportRef(ImportedPackageIndex, LinkerLoadState->LinkerLoadHeaderData.ImportedPublicExportHashes.Num());
				LinkerLoadState->LinkerLoadHeaderData.ImportMap[ImportIndex] = FPackageObjectIndex::FromPackageImportRef(PackageImportRef);
				LinkerLoadState->LinkerLoadHeaderData.ImportedPublicExportHashes.Add(CityHash64(reinterpret_cast<const char*>(PackageRelativeExportPath.GetData() + 1), (PackageRelativeExportPath.Len() - 1) * sizeof(TCHAR)));
			}
		}
	}

	LinkerLoadState->LinkerLoadHeaderData.ExportMap.SetNum(LinkerLoadState->Linker->ExportMap.Num());
	for (int32 ExportIndex = 0; ExportIndex < LinkerLoadState->Linker->ExportMap.Num(); ++ExportIndex)
	{
		FObjectExport& ObjectExport = LinkerLoadState->Linker->ExportMap[ExportIndex];
		if ((ObjectExport.ObjectFlags & RF_ClassDefaultObject) != 0)
		{
			LinkerLoadState->bContainsClasses |= true;
		}

		//if ((ObjectExport.ObjectFlags & RF_Public) > 0) // We need hashes for all objects, external actors are breaking assumptions about public exports
		{
			TArray<int32, TInlineAllocator<128>> FullPath;
			int32 PathIndex = ExportIndex;
			FullPath.Push(PathIndex);
			while (LinkerLoadState->Linker->ExportMap[PathIndex].OuterIndex.IsExport())
			{
				PathIndex = LinkerLoadState->Linker->ExportMap[PathIndex].OuterIndex.ToExport();
				FullPath.Push(PathIndex);
			}
			TStringBuilder<256> PackageRelativeExportPath;
			while (!FullPath.IsEmpty())
			{
				TCHAR NameStr[FName::StringBufferSize];
				uint32 NameLen = LinkerLoadState->Linker->ExportMap[FullPath.Pop(EAllowShrinking::No)].ObjectName.ToString(NameStr);
				for (uint32 I = 0; I < NameLen; ++I)
				{
					NameStr[I] = TChar<TCHAR>::ToLower(NameStr[I]);
				}
				PackageRelativeExportPath.AppendChar('/');
				PackageRelativeExportPath.Append(FStringView(NameStr, NameLen));
			}
			uint64 PublicExportHash = CityHash64(reinterpret_cast<const char*>(PackageRelativeExportPath.GetData() + 1), (PackageRelativeExportPath.Len() - 1) * sizeof(TCHAR));
			LinkerLoadState->LinkerLoadHeaderData.ExportMap[ExportIndex].PublicExportHash = PublicExportHash;
		}
	}

	FPackageStoreEntry PackageStoreEntry;
	PackageStoreEntry.ImportedPackageIds = ImportedPackageIds;
	AsyncLoadingThread.InitializeAsyncPackageFromPackageStore(ThreadState, nullptr, this, PackageStoreEntry);

	HeaderData.ImportedPackageNames = ImportedPackageNames;
	HeaderData.ImportedPublicExportHashes = LinkerLoadState->LinkerLoadHeaderData.ImportedPublicExportHashes;
	HeaderData.ImportMap = LinkerLoadState->LinkerLoadHeaderData.ImportMap;
	HeaderData.ExportMap = LinkerLoadState->LinkerLoadHeaderData.ExportMap;

	AsyncLoadingThread.FinishInitializeAsyncPackage(ThreadState, this);

	FLoadedPackageRef& PackageRef = AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Desc.UPackageId, Desc.UPackageName);
	if (!bLoadHasFailed)
	{
		PackageRef.ReserveSpaceForPublicExports(LinkerLoadState->LinkerLoadHeaderData.ExportMap.Num());
#if WITH_EDITORONLY_DATA
		// Create metadata object, this needs to happen before any other package wants to use our exports
		LinkerLoadState->MetaDataIndex = LinkerLoadState->Linker->LoadMetaDataFromExportMap(false);
		if (LinkerLoadState->MetaDataIndex >= 0)
		{
			FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[LinkerLoadState->MetaDataIndex];
			FExportObject& ExportObject = Data.Exports[LinkerLoadState->MetaDataIndex];
			ExportObject.Object = LinkerExport.Object;
			if (ExportObject.Object)
			{
				ExportObject.bWasFoundInMemory = true; // Make sure that the async flags are cleared in ClearConstructedObjects
			}
			else
			{
				ExportObject.bExportLoadFailed = LinkerExport.bExportLoadFailed;
				if (!ExportObject.bExportLoadFailed)
				{
					ExportObject.bFiltered = true;
				}
			}
		}
#endif
	}
	else if (Desc.bCanBeImported)
	{
		PackageRef.SetHasFailed();
	}

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForDependencies;
	if (!AsyncLoadingThread.bHasRegisteredAllScriptObjects)
	{
		SetupScriptDependencies();
	}
	GetPackageNode(Package_DependenciesReady).ReleaseBarrier(&ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

bool FAsyncPackage2::PreloadLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState)
{
	// Serialize exports
	const int32 ExportCount = LinkerLoadState->Linker->ExportMap.Num();
	check(LinkerLoadState->Linker->ExportMap.Num() == Data.Exports.Num());
	while (LinkerLoadState->SerializeExportIndex < ExportCount)
	{
		const int32 ExportIndex = LinkerLoadState->SerializeExportIndex++;
		FExportObject& ExportObject = Data.Exports[ExportIndex];
		FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[ExportIndex];

		// The linker export table can be patched during reinstantiation. We need to adjust our own export table if needed.
		if (!LinkerExport.bExportLoadFailed)
		{
			if (ExportObject.Object != LinkerExport.Object)
			{
				UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("PreloadLinkerLoadExports"), TEXT("Patching export %d: %s -> %s"), ExportIndex, *GetPathNameSafe(ExportObject.Object), *GetPathNameSafe(LinkerExport.Object));
				ExportObject.Object = LinkerExport.Object;
			}
		}

		if (UObject* Object = ExportObject.Object)
		{
			if (Object->HasAnyFlags(RF_NeedLoad))
			{
				UE_ASYNC_PACKAGE_LOG(VeryVerbose, Desc, TEXT("PreloadLinkerLoadExports"), TEXT("Preloading export %d: %s"), ExportIndex, *Object->GetPathName());
				LinkerLoadState->Linker->Preload(Object);
			}
		}
		if (ThreadState.IsTimeLimitExceeded(TEXT("SerializeLinkerLoadExports")))
		{
			return false;
		}
	}

	return true;
}

bool FAsyncPackage2::ResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState)
{
	check(AsyncPackageLoadingState >= EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies);

	check (!LinkerLoadState->bIsCurrentlyResolvingImports);
	TGuardValue<bool> GuardIsCurrentlyResolvingImports(LinkerLoadState->bIsCurrentlyResolvingImports, true);

	// Validate that all imports are in the appropriate state and had their exports created
	const int32 ImportedPackagesCount = Data.ImportedAsyncPackages.Num();
	for (int32 ImportIndex = 0; ImportIndex < ImportedPackagesCount; ++ImportIndex)
	{
		FAsyncPackage2* ImportedPackage = Data.ImportedAsyncPackages[ImportIndex];
		if (ImportedPackage)
		{
			if (ImportedPackage->LinkerLoadState.IsSet())
			{
				if (ImportedPackage->AsyncPackageLoadingState < EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies)
				{
					check(ThreadState.PackagesOnStack.Contains(ImportedPackage));
					UE_LOG(LogStreaming, Warning, TEXT("Package %s might be missing an import from package %s because of a circular dependency between them."),
						*Desc.UPackageName.ToString(),
						*ImportedPackage->Desc.UPackageName.ToString());
				}
			}
			else
			{
				// A dependency is added for zen imports in ImportPackagesRecursiveInner that should prevent
				// us from getting a zen package in a state before its exports are ready. 
				// Just verify that it's working as intended.
				if (ImportedPackage->AsyncPackageLoadingState < EAsyncPackageLoadingState2::ExportsDone)
				{
					check(ThreadState.PackagesOnStack.Contains(ImportedPackage));
					UE_LOG(LogStreaming, Warning, TEXT("Package %s might be missing an import from cooked package %s because it's exports are not yet ready."),
						*Desc.UPackageName.ToString(),
						*ImportedPackage->Desc.UPackageName.ToString());
				}
			}
		}
	}

	const int32 ImportCount = HeaderData.ImportMap.Num();
	while (LinkerLoadState->CreateImportIndex < ImportCount)
	{
		const int32 ImportIndex = LinkerLoadState->CreateImportIndex++;
		const FPackageObjectIndex& GlobalImportIndex = HeaderData.ImportMap[ImportIndex];
		if (!GlobalImportIndex.IsNull())
		{
			UObject* FromImportStore = ImportStore.FindOrGetImportObject(HeaderData, GlobalImportIndex);
#if ALT2_VERIFY_LINKERLOAD_MATCHES_IMPORTSTORE
			/*if (Desc.UPackageId.ValueForDebugging() == 0xF37353A71BF5C938 && ImportIndex == 0x0000005d)
			{
				UE_DEBUG_BREAK();
			}*/
			UObject* FromLinker = LinkerLoadState->Linker->CreateImport(ImportIndex);
			if (FromImportStore != FromLinker)
			{
				bool bIsAcceptableDeviation = false;
				FObjectImport& LinkerImport = LinkerLoadState->Linker->ImportMap[ImportIndex];
				if (FromLinker)
				{
					check(LinkerImport.SourceLinker);
					check(LinkerImport.SourceIndex >= 0);
					FObjectExport& SourceExport = LinkerImport.SourceLinker->ExportMap[LinkerImport.SourceIndex];
					if (!FromImportStore && SourceExport.bExportLoadFailed)
					{
						bIsAcceptableDeviation = true; // Exports can be marked as failed after they have already been returned to CreateImport. Is this a bug or a feature?
					}
					else if (FromImportStore && FromImportStore->GetName() == FromLinker->GetName() && FromLinker->GetOutermost() == GetTransientPackage())
					{
						bIsAcceptableDeviation = true; // Linker are sometimes stuck with objects that have been moved to the transient package. Is this a bug or a feature?
					}
				}
				check(bIsAcceptableDeviation);
			}
#endif
			FObjectImport& LinkerImport = LinkerLoadState->Linker->ImportMap[ImportIndex];
			if (!LinkerImport.XObject && FromImportStore)
			{
				LinkerImport.XObject = FromImportStore;
				LinkerImport.SourceIndex = FromImportStore->GetLinkerIndex();
				LinkerImport.SourceLinker = FromImportStore->GetLinker();

				UE_ASYNC_PACKAGE_LOG(VeryVerbose, Desc, TEXT("ResolveLinkerLoadImports"), TEXT("Resolved import %d: %s"), ImportIndex, *FromImportStore->GetPathName());
			}

			if (FromImportStore == nullptr)
			{
				UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ResolveLinkerLoadImports"), TEXT("Could not resolve import %d"), ImportIndex);
			}
		}
	}

	return true;
}

bool FAsyncPackage2::CreateLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState)
{
	check(AsyncPackageLoadingState >= EAsyncPackageLoadingState2::DependenciesReady);

	check (!LinkerLoadState->bIsCurrentlyCreatingExports);
	TGuardValue<bool> GuardIsCurrentlyCreatingExports(LinkerLoadState->bIsCurrentlyCreatingExports, true);

	// Create exports
	const int32 ExportCount = LinkerLoadState->Linker->ExportMap.Num();
	while (LinkerLoadState->CreateExportIndex < ExportCount)
	{
		const int32 ExportIndex = LinkerLoadState->CreateExportIndex++;
#if WITH_EDITORONLY_DATA
		if (ExportIndex == LinkerLoadState->MetaDataIndex)
		{
			continue;
		}
#endif
		FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[ExportIndex];
		FExportObject& ExportObject = Data.Exports[ExportIndex];
		if (UObject* Object = LinkerLoadState->Linker->CreateExport(ExportIndex))
		{
			checkf(!Object->IsUnreachable(), TEXT("Trying to store an unreachable object '%s' in the import store"), *Object->GetFullName());
			ExportObject.Object = Object;
			ExportObject.bWasFoundInMemory = true; // Make sure that the async flags are cleared in ClearConstructedObjects
			EInternalObjectFlags FlagsToSet = EInternalObjectFlags::Async;
			uint64 PublicExportHash = LinkerLoadState->LinkerLoadHeaderData.ExportMap[ExportIndex].PublicExportHash;
			if (Desc.bCanBeImported && PublicExportHash)
			{
				FlagsToSet |= EInternalObjectFlags::LoaderImport;
				ImportStore.StoreGlobalObject(Desc.UPackageId, PublicExportHash, Object);
			}
			Object->SetInternalFlags(FlagsToSet);
		}
		else
		{
			ExportObject.bExportLoadFailed = LinkerExport.bExportLoadFailed;
			if (!ExportObject.bExportLoadFailed)
			{
				ExportObject.bFiltered = true;
			}
		}

		if (ThreadState.IsTimeLimitExceeded(TEXT("CreateLinkerLoadExports")))
		{
			return false;
		}
	}

	return true;
}

EEventLoadNodeExecutionResult FAsyncPackage2::ExecutePostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState)
{
	if (!bLoadHasFailed)
	{
		SCOPED_LOADTIMER(PostLoadObjectsTime);
		TRACE_LOADTIME_POSTLOAD_SCOPE;

		FAsyncLoadingTickScope2 InAsyncLoadingTick(AsyncLoadingThread);

#if WITH_EDITOR
		UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Desc.PIEInstanceID);
#endif

		// Begin async loading, simulates BeginLoad
		BeginAsyncLoad();

		FUObjectSerializeContext* LoadContext = GetSerializeContext();
		TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();

		// End async loading, simulates EndLoad
		ON_SCOPE_EXIT { ThreadObjLoaded.Reset(); EndAsyncLoad(); };

		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		TGuardValue<bool> GuardIsRoutingPostLoad(ThreadContext.IsRoutingPostLoad, true);

		// Only postload exports instead of constructed objects to avoid cross-package interference.
		// Also this step is only about running the thread-safe postload that are ready to run, the rest will be deferred.
		// This code applies to both non threaded loader and threaded loader to exercise the same code path for both
		// Any non thread-safe or not ready postload is automatically being moved to the deferred phase.
		while (LinkerLoadState->PostLoadExportIndex < Data.Exports.Num())
		{
			const int32 ExportIndex = LinkerLoadState->PostLoadExportIndex++;
			const FExportObject& Export = Data.Exports[ExportIndex];
		
			if (UObject* Object = Export.Object)
			{
				if (Object->HasAnyFlags(RF_NeedPostLoad) && CanPostLoadOnAsyncLoadingThread(Object) && Object->IsReadyForAsyncPostLoad())
				{
#if WITH_EDITOR
					SCOPED_LOADTIMER_ASSET_TEXT(*Object->GetPathName());
#endif
					ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
					Object->ConditionalPostLoad();
					ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;
				}

				if (ThreadState.IsTimeLimitExceeded(TEXT("ExecutePostLoadLinkerLoadPackageExports")))
				{
					return EEventLoadNodeExecutionResult::Timeout;
				}
			}
		}
	}

	// Reset this to be reused for the deferred postload phase
	LinkerLoadState->PostLoadExportIndex = 0;

	check(DeferredPostLoadGroup);
	check(DeferredPostLoadGroup->PackagesWithExportsToPostLoadCount > 0);
	--DeferredPostLoadGroup->PackagesWithExportsToPostLoadCount;

	AsyncLoadingThread.ConditionalBeginDeferredPostLoad(ThreadState, DeferredPostLoadGroup);
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::ExecuteDeferredPostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState)
{
	SCOPED_LOADTIMER(PostLoadDeferredObjectsTime);
	TRACE_LOADTIME_POSTLOAD_SCOPE;

	FAsyncLoadingTickScope2 InAsyncLoadingTick(AsyncLoadingThread);

#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Desc.PIEInstanceID);
#endif

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();

	ON_SCOPE_EXIT{ ThreadObjLoaded.Reset(); };

	// We can't return timeout during a flush as we're expected to be able to finish
	const bool bIsReadyForAsyncPostLoadAllowed = ThreadState.SyncLoadContextStack.IsEmpty();

	UE_MT_SCOPED_READ_ACCESS(ConstructedObjectsAccessDetector);

	// Go through both ConstructedObjects and export table as its possible to reload objects in the export table
	// without them being constructed and that would lead to missing postloads.
	// ConstructedObjects can be appended to during conditional postloads, so make sure to always take the latest value.
	const int32 ExportsCount = Data.Exports.Num();
	while (LinkerLoadState->PostLoadExportIndex < ExportsCount + ConstructedObjects.Num())
	{
		const int32 ObjectIndex = LinkerLoadState->PostLoadExportIndex++;

		if (ObjectIndex < ExportsCount)
		{
			FExportObject& ExportObject = Data.Exports[ObjectIndex];
			FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[ObjectIndex];
			// The linker export table can be patched during reinstantiation. We need to adjust our own export table if needed.
			if (!LinkerExport.bExportLoadFailed)
			{
				if (ExportObject.Object != LinkerExport.Object)
				{
					UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ExecuteDeferredPostLoadLinkerLoadPackageExports"), TEXT("Patching export %d: %s -> %s"), ObjectIndex, *GetPathNameSafe(ExportObject.Object), *GetPathNameSafe(LinkerExport.Object));
					ExportObject.Object = LinkerExport.Object;
				}
			}
		}

		UObject* Object = ObjectIndex < ExportsCount ? Data.Exports[ObjectIndex].Object : ConstructedObjects[ObjectIndex - ExportsCount];
		if (Object && Object->HasAnyFlags(RF_NeedPostLoad))
		{
			// Only allow to wait when there is no flush waiting on us
			if (bIsReadyForAsyncPostLoadAllowed && !Object->IsReadyForAsyncPostLoad())
			{
				--LinkerLoadState->PostLoadExportIndex;
				return EEventLoadNodeExecutionResult::Timeout;
			}
#if WITH_EDITOR
			SCOPED_LOADTIMER_ASSET_TEXT(*Object->GetPathName());
#endif
			Object->ConditionalPostLoad();
		}

		if (ThreadState.IsTimeLimitExceeded(TEXT("ExecuteDeferredPostLoadLinkerLoadPackageExports")))
		{
			return EEventLoadNodeExecutionResult::Timeout;
		}
	}

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoadDone;
	ConditionalFinishLoading(ThreadState);

	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_CreateLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_CreateLinkerLoadExports);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::CreateLinkerLoadExports);

	FAsyncPackageScope2 Scope(Package);
#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);
#endif

	check(Package->LinkerLoadState.IsSet());

	if (!Package->CreateLinkerLoadExports(ThreadState))
	{
		return EEventLoadNodeExecutionResult::Timeout;
	}

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies;
	Package->ConditionalBeginResolveLinkerLoadImports(ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

void FAsyncPackage2::ConditionalBeginResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginResolveLinkerLoadImports);

	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesImportState, EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies, AsyncLoadingThread.ConditionalBeginResolveImportsTick,
		[&ThreadState](FAsyncPackage2* Package)
		{
			if (Package->LinkerLoadState.IsSet())
			{
				check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForLinkerLoadDependencies);
				Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ResolveLinkerLoadImports;
				Package->GetPackageNode(Package_ResolveLinkerLoadImports).ReleaseBarrier(&ThreadState);
			}
			else
			{
				// Don't advance state of cooked package, nodes already have dependencies setup.
			}
		}
	);
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ResolveLinkerLoadImports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ResolveLinkerLoadImports);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ResolveLinkerLoadImports);

	FAsyncPackageScope2 Scope(Package);
#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);
#endif

	if (Package->LinkerLoadState.IsSet())
	{
		if (!Package->ResolveLinkerLoadImports(ThreadState))
		{
			return EEventLoadNodeExecutionResult::Timeout;
		}
	}

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::PreloadLinkerLoadExports;
	Package->GetPackageNode(Package_PreloadLinkerLoadExports).ReleaseBarrier(&ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_PreloadLinkerLoadExports(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_PreloadLinkerLoadExports);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PreloadLinkerLoadExports);

	FAsyncPackageScope2 Scope(Package);
#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);
#endif

	if (Package->LinkerLoadState.IsSet())
	{
		if (!Package->PreloadLinkerLoadExports(ThreadState))
		{
			return EEventLoadNodeExecutionResult::Timeout;
		}
	}

	if (Package->ExternalReadDependencies.Num() == 0)
	{
		Package->GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(&ThreadState);
	}
	else
	{
		Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForExternalReads;
		Package->AsyncLoadingThread.ExternalReadQueue.Enqueue(Package);
	}
	return EEventLoadNodeExecutionResult::Complete;
}

#endif // ALT2_ENABLE_LINKERLOAD_SUPPORT

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ProcessPackageSummary(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ProcessPackageSummary);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForIo);
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessPackageSummary;

	FAsyncPackageScope2 Scope(Package);

#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);
#endif

	Package->CallProgressCallbacks(EAsyncLoadingProgress::Read);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		return Package->ProcessLinkerLoadPackageSummary(ThreadState);
	}
#endif
	if (Package->bLoadHasFailed)
	{
		if (Package->Desc.bCanBeImported)
		{
			FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId, Package->Desc.UPackageName);
			PackageRef.SetHasFailed();
		}
	}
	else
	{
		TRACE_LOADTIME_PROCESS_SUMMARY_SCOPE(Package);
		check(Package->ExportBundleEntryIndex == 0);

		static_cast<FZenPackageHeader&>(Package->HeaderData) = FZenPackageHeader::MakeView(Package->SerializationState.IoRequest.GetResultOrDie().GetView());
#if WITH_EDITOR
		FAsyncPackageHeaderData* OptionalSegmentHeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
		if (OptionalSegmentHeaderData)
		{
			static_cast<FZenPackageHeader&>(*OptionalSegmentHeaderData) = FZenPackageHeader::MakeView(Package->OptionalSegmentSerializationState->IoRequest.GetResultOrDie().GetView());
		}
#endif
		if (Package->Desc.bCanBeImported)
		{
			int32 PublicExportsCount = 0;
			for (const FExportMapEntry& Export : Package->HeaderData.ExportMap)
			{
				if (Export.PublicExportHash)
				{
					++PublicExportsCount;
				}
			}
#if WITH_EDITOR
			if (Package->OptionalSegmentHeaderData.IsSet())
			{
				for (const FExportMapEntry& Export : Package->OptionalSegmentHeaderData->ExportMap)
				{
					if (Export.PublicExportHash)
					{
						++PublicExportsCount;
					}
				}
			}
#endif
			FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId, Package->Desc.UPackageName);
			if (PublicExportsCount)
			{
				PackageRef.ReserveSpaceForPublicExports(PublicExportsCount);
			}
		}

		check(Package->Desc.PackageIdToLoad == FPackageId::FromName(Package->HeaderData.PackageName));
		if (Package->Desc.PackagePathToLoad.IsEmpty())
		{
			Package->Desc.PackagePathToLoad = FPackagePath::FromPackageNameUnchecked(Package->HeaderData.PackageName);
		}
		// Imported packages won't have a UPackage name set unless they were redirected, in which case they will have the source package name
		if (Package->Desc.UPackageName.IsNone())
		{
			Package->Desc.UPackageName = Package->HeaderData.PackageName;
		}
		check(Package->Desc.UPackageId == FPackageId::FromName(Package->Desc.UPackageName));
		Package->CreateUPackage();
		Package->LinkerRoot->SetPackageFlags(Package->HeaderData.PackageSummary->PackageFlags);
#if WITH_EDITOR
		Package->LinkerRoot->bIsCookedForEditor = !!(Package->HeaderData.PackageSummary->PackageFlags & PKG_FilterEditorOnly);
#endif
		if (const FZenPackageVersioningInfo* VersioningInfo = Package->HeaderData.VersioningInfo.GetPtrOrNull())
		{
			Package->LinkerRoot->SetLinkerPackageVersion(VersioningInfo->PackageVersion);
			Package->LinkerRoot->SetLinkerLicenseeVersion(VersioningInfo->LicenseeVersion);
			Package->LinkerRoot->SetLinkerCustomVersions(VersioningInfo->CustomVersions);
		}
		else
		{
			Package->LinkerRoot->SetLinkerPackageVersion(GPackageFileUEVersion);
			Package->LinkerRoot->SetLinkerLicenseeVersion(GPackageFileLicenseeUEVersion);
		}

		// Check if the package is instanced
		FName PackageNameToLoad = Package->Desc.PackagePathToLoad.GetPackageFName();
		if (Package->Desc.UPackageName != PackageNameToLoad)
		{
			Package->Desc.InstancingContext.BuildPackageMapping(PackageNameToLoad, Package->Desc.UPackageName);
		}

		TRACE_LOADTIME_PACKAGE_SUMMARY(Package, Package->HeaderData.PackageName, Package->HeaderData.PackageSummary->HeaderSize, Package->HeaderData.ImportMap.Num(), Package->HeaderData.ExportMap.Num(), Package->Desc.Priority);
	}

	Package->AsyncLoadingThread.FinishInitializeAsyncPackage(ThreadState, Package);

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForDependencies;
	if (!Package->AsyncLoadingThread.bHasRegisteredAllScriptObjects)
	{
		Package->SetupScriptDependencies();
	}
	Package->GetPackageNode(Package_DependenciesReady).ReleaseBarrier(&ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_DependenciesReady(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_DependenciesReady);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForDependencies);

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DependenciesReady;
	Package->ConditionalBeginProcessPackageExports(ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

void FAsyncPackage2::CallProgressCallbacks(EAsyncLoadingProgress ProgressType)
{
	if (ProgressCallbacks.Num() != 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPackage2::CallProgressCallbacks);

		const FLoadPackageAsyncProgressParams Params
		{
			.PackageName = Desc.UPackageName,
			.LoadedPackage = GetLinkerRoot(),
			.ProgressType = ProgressType
		};

		for (TUniquePtr<FLoadPackageAsyncProgressDelegate>& ProgressCallback : ProgressCallbacks)
		{
			ProgressCallback->ExecuteIfBound(Params);
		}
	}
}

void FAsyncPackage2::InitializeExportArchive(FExportArchive& Ar, bool bIsOptionalSegment)
{
	Ar.SetUEVer(LinkerRoot->GetLinkerPackageVersion());
	Ar.SetLicenseeUEVer(LinkerRoot->GetLinkerLicenseeVersion());
	// Ar.SetEngineVer(Summary.SavedByEngineVersion); // very old versioning scheme
	if (!LinkerRoot->GetLinkerCustomVersions().GetAllVersions().IsEmpty())
	{
		Ar.SetCustomVersions(LinkerRoot->GetLinkerCustomVersions());
	}
	Ar.SetUseUnversionedPropertySerialization((LinkerRoot->GetPackageFlags() & PKG_UnversionedProperties) != 0);
	Ar.SetIsLoadingFromCookedPackage((LinkerRoot->GetPackageFlags() & PKG_Cooked) != 0);
	Ar.SetIsLoading(true);
	Ar.SetIsPersistent(true);
	if (LinkerRoot->GetPackageFlags() & PKG_FilterEditorOnly)
	{
		Ar.SetFilterEditorOnly(true);
	}
	Ar.ArAllowLazyLoading = true;

	// FExportArchive special fields
	Ar.PackageDesc = &Desc;
	Ar.HeaderData = &HeaderData;
#if WITH_EDITOR
	if (bIsOptionalSegment)
	{
		Ar.HeaderData = OptionalSegmentHeaderData.GetPtrOrNull();
		check(Ar.HeaderData);
	}
#endif
	Ar.ImportStore = &ImportStore;
	Ar.ExternalReadDependencies = &ExternalReadDependencies;
	Ar.InstanceContext = &Desc.InstancingContext;
	Ar.bIsOptionalSegment = bIsOptionalSegment;
	Ar.bExportsCookedToSeparateArchive = Ar.UEVer() >= EUnrealEngineObjectUE5Version::DATA_RESOURCES;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ProcessExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 InExportBundleIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ProcessExportBundle);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState >= EAsyncPackageLoadingState2::DependenciesReady);
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;

	UE_ASYNC_PACKAGE_LOG(VeryVerbose, Package->Desc, TEXT("ProcessExportBundle"), TEXT("Beginning Processing Export Bundle %d"), InExportBundleIndex);

	FAsyncPackageScope2 Scope(Package);
#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);
#endif

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	// This code path should not be reached for LinkerLoad packages
	check(!Package->LinkerLoadState.IsSet());
#endif

	check(InExportBundleIndex < Package->Data.TotalExportBundleCount);
	
	if (!Package->bLoadHasFailed)
	{
		bool bIsOptionalSegment = false;
#if WITH_EDITOR
		const FAsyncPackageHeaderData* HeaderData;
		FAsyncPackageSerializationState* SerializationState;
		bIsOptionalSegment = InExportBundleIndex == 1;

		if (bIsOptionalSegment)
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
			SerializationState = Package->OptionalSegmentSerializationState.GetPtrOrNull();
			check(SerializationState);
		}
		else
		{
			check(InExportBundleIndex == 0);
			HeaderData = &Package->HeaderData;
			SerializationState = &Package->SerializationState;
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
		FAsyncPackageSerializationState* SerializationState = &Package->SerializationState;
#endif
		const FIoBuffer& IoBuffer = SerializationState->IoRequest.GetResultOrDie();
		FExportArchive Ar(IoBuffer);
		Package->InitializeExportArchive(Ar, bIsOptionalSegment);

		while (Package->ExportBundleEntryIndex < HeaderData->ExportBundleEntries.Num())
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntries[Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_ProcessExportBundle")))
			{
				return EEventLoadNodeExecutionResult::Timeout;
			}
			const FExportMapEntry& ExportMapEntry = HeaderData->ExportMap[BundleEntry.LocalExportIndex];
			FExportObject& Export = HeaderData->ExportsView[BundleEntry.LocalExportIndex];

			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Create)
			{
				if (!Export.Object)
				{
					Package->EventDrivenCreateExport(*HeaderData, BundleEntry.LocalExportIndex);
				}
			}
			else
			{
				check(BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize);
				if (Export.Object && Export.Object->HasAllFlags(RF_NeedLoad))
				{
					Package->EventDrivenSerializeExport(*HeaderData, BundleEntry.LocalExportIndex, &Ar);
				}
			}
			++Package->ExportBundleEntryIndex;
		}
	}
	
	Package->ExportBundleEntryIndex = 0;

	if (++Package->ProcessedExportBundlesCount == Package->Data.TotalExportBundleCount)
	{
		Package->ProcessedExportBundlesCount = 0;
		Package->HeaderData.Reset();
		Package->SerializationState.ReleaseIoRequest();
#if WITH_EDITOR
		if (Package->OptionalSegmentHeaderData.IsSet())
		{
			Package->OptionalSegmentHeaderData->Reset();
			Package->OptionalSegmentSerializationState->ReleaseIoRequest();
		}
#endif
		check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles);

		if (Package->ExternalReadDependencies.Num() == 0)
		{
			Package->GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(&ThreadState);
		}
		else
		{
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForExternalReads;
			Package->AsyncLoadingThread.ExternalReadQueue.Enqueue(Package);
		}
	}
	else
	{
		// Release the next bundle now that we've finished.
		Package->GetExportBundleNode(EEventLoadNode2::ExportBundle_Process, Package->ProcessedExportBundlesCount).ReleaseBarrier(&ThreadState);
	}

	UE_ASYNC_PACKAGE_LOG(VeryVerbose, Package->Desc, TEXT("ProcessExportBundle"), TEXT("Finished Processing Export Bundle %d"), InExportBundleIndex);

	return EEventLoadNodeExecutionResult::Complete;
}

UObject* FAsyncPackage2::EventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, FPackageObjectIndex Index, bool bCheckSerialized)
{
	UObject* Result = nullptr;
	if (Index.IsNull())
	{
		return Result;
	}
	if (Index.IsExport())
	{
		Result = Header.ExportsView[Index.ToExport()].Object;
		UE_CLOG(!Result, LogStreaming, Warning, TEXT("Missing Dependency, missing export 0x%llX in package %s"),
			Index.Value(),
			*Desc.PackagePathToLoad.GetPackageFName().ToString());
	}
	else if (Index.IsImport())
	{
		Result = ImportStore.FindOrGetImportObject(Header, Index);
		UE_CLOG(!Result, LogStreaming, Warning, TEXT("Missing Dependency, missing %s import 0x%llX for package %s"),
			Index.IsScriptImport() ? TEXT("script") : TEXT("package"),
			Index.Value(),
			*Desc.PackagePathToLoad.GetPackageFName().ToString());
	}
#if DO_CHECK
	if (Result && bCheckSerialized)
	{
		bool bIsSerialized = Index.IsScriptImport() || Result->IsA(UPackage::StaticClass()) || Result->HasAllFlags(RF_WasLoaded | RF_LoadCompleted);
		if (!bIsSerialized)
		{
			UE_LOG(LogStreaming, Warning, TEXT("Missing Dependency, '%s' (0x%llX) for package %s has not been serialized yet."),
				*Result->GetFullName(),
				Index.Value(),
				*Desc.PackagePathToLoad.GetPackageFName().ToString());
		}
	}
	if (Result)
	{
		UE_CLOG(Result->IsUnreachable(), LogStreaming, Fatal, TEXT("Returning an object  (%s) from EventDrivenIndexToObject that is unreachable."), *Result->GetFullName());
	}
#endif
	return Result;
}

void FAsyncPackage2::ProcessExportDependencies(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex, FExportBundleEntry::EExportCommandType CommandType)
{
	static_assert(FExportBundleEntry::ExportCommandType_Count == 2, "Expected the only export command types fo be Create and Serialize");

	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessExportDependencies);
	const FDependencyBundleHeader& DependencyBundle = Header.DependencyBundleHeaders[LocalExportIndex];
	if (DependencyBundle.FirstEntryIndex < 0)
	{
		return;
	}
	int32 RunningIndex = DependencyBundle.FirstEntryIndex;
	if (CommandType == FExportBundleEntry::ExportCommandType_Serialize)
	{
		// Skip over the dependency entries for Create
		for (int32 Index = 0; Index < FExportBundleEntry::ExportCommandType_Count; ++Index)
		{
			RunningIndex += DependencyBundle.EntryCount[FExportBundleEntry::ExportCommandType_Create][Index];
		}
	}
	
	for (int32 Index = DependencyBundle.EntryCount[CommandType][FExportBundleEntry::ExportCommandType_Create]; Index > 0; --Index)
	{
		const FDependencyBundleEntry& Dep = Header.DependencyBundleEntries[RunningIndex++];
		if (Dep.LocalImportOrExportIndex.IsExport())
		{
			ConditionalCreateExport(Header, Dep.LocalImportOrExportIndex.ToExport());
		}
		else
		{
			ConditionalCreateImport(Header, Dep.LocalImportOrExportIndex.ToImport());
		}
	}

	for (int32 Index = DependencyBundle.EntryCount[CommandType][FExportBundleEntry::ExportCommandType_Serialize]; Index > 0; Index--)
	{
		const FDependencyBundleEntry& Dep = Header.DependencyBundleEntries[RunningIndex++];
		if (Dep.LocalImportOrExportIndex.IsExport())
		{
			ConditionalSerializeExport(Header, Dep.LocalImportOrExportIndex.ToExport());
		}
		else
		{
			ConditionalSerializeImport(Header, Dep.LocalImportOrExportIndex.ToImport());
		}
	}
}

int32 FAsyncPackage2::GetPublicExportIndex(uint64 ExportHash, FAsyncPackageHeaderData*& OutHeader)
{
	for (int32 ExportIndex = 0; ExportIndex < HeaderData.ExportMap.Num(); ++ExportIndex)
	{
		if (HeaderData.ExportMap[ExportIndex].PublicExportHash == ExportHash)
		{
			OutHeader = &HeaderData;
			return ExportIndex;
		}
	}
#if WITH_EDITOR
	if (FAsyncPackageHeaderData* OptionalSegmentHeaderDataPtr = OptionalSegmentHeaderData.GetPtrOrNull())
	{
		for (int32 ExportIndex = 0; ExportIndex < OptionalSegmentHeaderDataPtr->ExportMap.Num(); ++ExportIndex)
		{
			if (OptionalSegmentHeaderDataPtr->ExportMap[ExportIndex].PublicExportHash == ExportHash)
			{
				OutHeader = OptionalSegmentHeaderDataPtr;
				return HeaderData.ExportMap.Num() + ExportIndex;
			}
		}
	}
#endif
	return -1;
}
UObject* FAsyncPackage2::ConditionalCreateExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex)
{
	if (!Header.ExportsView[LocalExportIndex].Object)
	{
		FAsyncPackageScope2 Scope(this);
		EventDrivenCreateExport(Header, LocalExportIndex);
	}
	return Header.ExportsView[LocalExportIndex].Object;
}

UObject* FAsyncPackage2::ConditionalSerializeExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex)
{
	FExportObject& Export = Header.ExportsView[LocalExportIndex];
	
	if (!Export.Object && !(Export.bFiltered | Export.bExportLoadFailed))
	{
		ConditionalCreateExport(Header, LocalExportIndex);
	}
	
	if (!Export.Object || (Export.bFiltered | Export.bExportLoadFailed))
	{
		return nullptr;
	}

	if (Export.Object->HasAllFlags(RF_NeedLoad))
	{
		FAsyncPackageScope2 Scope(this);
		EventDrivenSerializeExport(Header, LocalExportIndex, nullptr);
	}

	return Export.Object;
}

UObject* FAsyncPackage2::ConditionalCreateImport(const FAsyncPackageHeaderData& Header, int32 LocalImportIndex)
{
	const FPackageObjectIndex& ObjectIndex = Header.ImportMap[LocalImportIndex];
	check(ObjectIndex.IsPackageImport());
	if (UObject* FromImportStore = ImportStore.FindOrGetImportObject(Header, ObjectIndex))
	{
		return FromImportStore;
	}

	FPackageImportReference PackageImportRef = ObjectIndex.ToPackageImportRef();
	FAsyncPackage2* SourcePackage = Header.ImportedAsyncPackagesView[PackageImportRef.GetImportedPackageIndex()];
	if (!SourcePackage)
	{
		return nullptr;
	}
	uint64 ExportHash = Header.ImportedPublicExportHashes[PackageImportRef.GetImportedPublicExportHashIndex()];
	FAsyncPackageHeaderData* SourcePackageHeader = nullptr;
	int32 ExportIndex = SourcePackage->GetPublicExportIndex(ExportHash, SourcePackageHeader);
	if (ExportIndex < 0)
	{
		return nullptr;
	}
	return SourcePackage->ConditionalCreateExport(*SourcePackageHeader, ExportIndex);
}

UObject* FAsyncPackage2::ConditionalSerializeImport(const FAsyncPackageHeaderData& Header, int32 LocalImportIndex)
{
	const FPackageObjectIndex& ObjectIndex = Header.ImportMap[LocalImportIndex];
	check(ObjectIndex.IsPackageImport());

	if (UObject* FromImportStore = ImportStore.FindOrGetImportObject(Header, ObjectIndex))
	{
		if (!FromImportStore->HasAllFlags(RF_NeedLoad))
		{
			return FromImportStore;
		}
	}

	FPackageImportReference PackageImportRef = ObjectIndex.ToPackageImportRef();
	FAsyncPackage2* SourcePackage = Header.ImportedAsyncPackagesView[PackageImportRef.GetImportedPackageIndex()];
	if (!SourcePackage)
	{
		return nullptr;
	}
	uint64 ExportHash = Header.ImportedPublicExportHashes[PackageImportRef.GetImportedPublicExportHashIndex()];
	FAsyncPackageHeaderData* SourcePackageHeader = nullptr;
	int32 ExportIndex = SourcePackage->GetPublicExportIndex(ExportHash, SourcePackageHeader);
	if (ExportIndex < 0)
	{
		return nullptr;
	}
	return SourcePackage->ConditionalSerializeExport(*SourcePackageHeader, ExportIndex);
}

void FAsyncPackage2::EventDrivenCreateExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateExport);

	const FExportMapEntry& Export = Header.ExportMap[LocalExportIndex];
	FExportObject& ExportObject = Header.ExportsView[LocalExportIndex];
	UObject*& Object = ExportObject.Object;
	check(!Object);

	TRACE_LOADTIME_CREATE_EXPORT_SCOPE(this, &Object);

	FName ObjectName;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ObjectNameFixup);
		ObjectName = Header.NameMap.GetName(Export.ObjectName);
	}

	auto FilterExport = [](const EExportFilterFlags FilterFlags) -> bool
	{
#if WITH_EDITOR
		return false;
#elif UE_SERVER
		return !!(static_cast<uint32>(FilterFlags) & static_cast<uint32>(EExportFilterFlags::NotForServer));
#elif !WITH_SERVER_CODE
		return !!(static_cast<uint32>(FilterFlags) & static_cast<uint32>(EExportFilterFlags::NotForClient));
#else
		static const bool bIsDedicatedServer = !GIsClient && GIsServer;
		static const bool bIsClientOnly = GIsClient && !GIsServer;

		if (bIsDedicatedServer && static_cast<uint32>(FilterFlags) & static_cast<uint32>(EExportFilterFlags::NotForServer))
		{
			return true;
		}

		if (bIsClientOnly && static_cast<uint32>(FilterFlags) & static_cast<uint32>(EExportFilterFlags::NotForClient))
		{
			return true;
		}

		return false;
#endif
	};

	ExportObject.bFiltered = FilterExport(Export.FilterFlags);
	if (ExportObject.bFiltered | ExportObject.bExportLoadFailed)
	{
		if (ExportObject.bExportLoadFailed)
		{
			UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("CreateExport"), TEXT("Skipped failed export %s"), *ObjectName.ToString());
		}
		else
		{
			UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbose, Desc, TEXT("CreateExport"), TEXT("Skipped filtered export %s"), *ObjectName.ToString());
		}
		return;
	}

	ProcessExportDependencies(Header, LocalExportIndex, FExportBundleEntry::ExportCommandType_Create);

	bool bIsCompleteyLoaded = false;
	UClass* LoadClass = Export.ClassIndex.IsNull() ? UClass::StaticClass() : CastEventDrivenIndexToObject<UClass>(Header, Export.ClassIndex, true);
	UObject* ThisParent = Export.OuterIndex.IsNull() ? LinkerRoot : EventDrivenIndexToObject(Header, Export.OuterIndex, false);

	if (!LoadClass)
	{
		UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find class object for %s"), *ObjectName.ToString());
		ExportObject.bExportLoadFailed = true;
		return;
	}
	if (!ThisParent)
	{
		UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find outer object for %s"), *ObjectName.ToString());
		ExportObject.bExportLoadFailed = true;
		return;
	}
	check(!dynamic_cast<UObjectRedirector*>(ThisParent));
	if (!Export.SuperIndex.IsNull())
	{
		ExportObject.SuperObject = EventDrivenIndexToObject(Header, Export.SuperIndex, false);
		if (!ExportObject.SuperObject)
		{
			UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find SuperStruct object for %s"), *ObjectName.ToString());
			ExportObject.bExportLoadFailed = true;
			return;
		}
	}
	// Find the Archetype object for the one we are loading.
	check(!Export.TemplateIndex.IsNull());
	ExportObject.TemplateObject = EventDrivenIndexToObject(Header, Export.TemplateIndex, true);
	if (!ExportObject.TemplateObject)
	{
		UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find template object for %s"), *ObjectName.ToString());
		ExportObject.bExportLoadFailed = true;
		return;
	}

	if ((Export.ObjectFlags & RF_ClassDefaultObject) == 0 
		&& !ExportObject.TemplateObject->IsA(LoadClass))
	{
		UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Export class type (%s) differs from the template object type (%s)"),
			*(LoadClass->GetFullName()),
			*(ExportObject.TemplateObject->GetClass()->GetFullName()));
		ExportObject.bExportLoadFailed = true;
		return;
	}

	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(GetLinkerRoot(), ELLMTagSet::Assets);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(LoadClass, ELLMTagSet::AssetClasses);
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(ObjectName, LoadClass->GetFName(), GetLinkerRoot()->GetFName());

	// Try to find existing object first as we cannot in-place replace objects, could have been created by other export in this package
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindExport);
		Object = StaticFindObjectFastInternal(NULL, ThisParent, ObjectName, true);
	}

	const bool bIsNewObject = !Object;

	// Object is found in memory.
	if (Object)
	{
		// If it has the AsyncLoading flag set it was created during the current load of this package (likely as a subobject)
		if (!Object->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
		{
			ExportObject.bWasFoundInMemory = true;
		}
		// If this object was allocated but never loaded (components created by a constructor, CDOs, etc) make sure it gets loaded
		// Do this for all subobjects created in the native constructor.
		const EObjectFlags ObjectFlags = Object->GetFlags();
		bIsCompleteyLoaded = !!(ObjectFlags & RF_LoadCompleted);
		if (!bIsCompleteyLoaded)
		{
			check(!(ObjectFlags & (RF_NeedLoad | RF_WasLoaded))); // If export exist but is not completed, it is expected to have been created from a native constructor and not from EventDrivenCreateExport, but who knows...?
			if (ObjectFlags & RF_ClassDefaultObject)
			{
				// never call PostLoadSubobjects on class default objects, this matches the behavior of the old linker where
				// StaticAllocateObject prevents setting of RF_NeedPostLoad and RF_NeedPostLoadSubobjects, but FLinkerLoad::Preload
				// assigns RF_NeedPostLoad for blueprint CDOs:
				Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_WasLoaded);
			}
			else
			{
				Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded);
			}
		}
	}
	else
	{
		// we also need to ensure that the template has set up any instances
		ExportObject.TemplateObject->ConditionalPostLoadSubobjects();

		check(!GVerifyObjectReferencesOnly); // not supported with the event driven loader
		// Create the export object, marking it with the appropriate flags to
		// indicate that the object's data still needs to be loaded.
		EObjectFlags ObjectLoadFlags = Export.ObjectFlags;
		ObjectLoadFlags = EObjectFlags(ObjectLoadFlags | RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded);

		// If we are about to create a CDO, we need to ensure that all parent sub-objects are loaded
		// to get default value initialization to work.
#if DO_CHECK
		if ((ObjectLoadFlags & RF_ClassDefaultObject) != 0)
		{
			UClass* SuperClass = LoadClass->GetSuperClass();
			UObject* SuperCDO = SuperClass ? SuperClass->GetDefaultObject() : nullptr;
			check(!SuperCDO || ExportObject.TemplateObject == SuperCDO); // the template for a CDO is the CDO of the super
			if (SuperClass && !SuperClass->IsNative())
			{
				check(SuperCDO);
				if (SuperClass->HasAnyFlags(RF_NeedLoad))
				{
					UE_LOG(LogStreaming, Fatal, TEXT("Super %s had RF_NeedLoad while creating %s"), *SuperClass->GetFullName(), *ObjectName.ToString());
					return;
				}
				if (SuperCDO->HasAnyFlags(RF_NeedLoad))
				{
					UE_LOG(LogStreaming, Fatal, TEXT("Super CDO %s had RF_NeedLoad while creating %s"), *SuperCDO->GetFullName(), *ObjectName.ToString());
					return;
				}
				TArray<UObject*> SuperSubObjects;
				GetObjectsWithOuter(SuperCDO, SuperSubObjects, /*bIncludeNestedObjects=*/ false, /*ExclusionFlags=*/ RF_NoFlags, /*InternalExclusionFlags=*/ EInternalObjectFlags::Native);

				for (UObject* SubObject : SuperSubObjects)
				{
					if (SubObject->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogStreaming, Fatal, TEXT("Super CDO subobject %s had RF_NeedLoad while creating %s"), *SubObject->GetFullName(), *ObjectName.ToString());
						return;
					}
				}
			}
			else
			{
				check(ExportObject.TemplateObject->IsA(LoadClass));
			}
		}
#endif
		checkf(!LoadClass->HasAnyFlags(RF_NeedLoad),
			TEXT("LoadClass %s had RF_NeedLoad while creating %s"), *LoadClass->GetFullName(), *ObjectName.ToString());
		checkf(!(LoadClass->GetDefaultObject() && LoadClass->GetDefaultObject()->HasAnyFlags(RF_NeedLoad)), 
			TEXT("Class CDO %s had RF_NeedLoad while creating %s"), *LoadClass->GetDefaultObject()->GetFullName(), *ObjectName.ToString());
		checkf(!ExportObject.TemplateObject->HasAnyFlags(RF_NeedLoad),
			TEXT("Template %s had RF_NeedLoad while creating %s"), *ExportObject.TemplateObject->GetFullName(), *ObjectName.ToString());

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ConstructObject);
			FStaticConstructObjectParameters Params(LoadClass);
			Params.Outer = ThisParent;
			Params.Name = ObjectName;
			Params.SetFlags = ObjectLoadFlags;
			Params.Template = ExportObject.TemplateObject;
			Params.bAssumeTemplateIsArchetype = true;
			Object = StaticConstructObject_Internal(Params);
		}

		if (GIsInitialLoad || GUObjectArray.IsOpenForDisregardForGC())
		{
			Object->AddToRoot();
		}

		check(Object->GetClass() == LoadClass);
		check(Object->GetFName() == ObjectName);
	}

	check(Object);
	EInternalObjectFlags FlagsToSet = EInternalObjectFlags::Async;
	
	if (Desc.bCanBeImported && Export.PublicExportHash)
	{
		FlagsToSet |= EInternalObjectFlags::LoaderImport;
		ImportStore.StoreGlobalObject(Desc.UPackageId, Export.PublicExportHash, Object);

		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateExport"), TEXT("Created %s export %s. Tracked as %s:0x%llX"),
			Object->HasAnyFlags(RF_Public) ? TEXT("public") : TEXT("private"), *Object->GetPathName(), *FormatPackageId(Desc.UPackageId), Export.PublicExportHash);
	}
	else
	{
		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateExport"), TEXT("Created %s export %s. Not tracked."),
			Object->HasAnyFlags(RF_Public) ? TEXT("public") : TEXT("private"), *Object->GetPathName());
	}
	Object->SetInternalFlags(FlagsToSet);
}

bool FAsyncPackage2::EventDrivenSerializeExport(const FAsyncPackageHeaderData& Header, int32 LocalExportIndex, FExportArchive* Ar)
{
	LLM_SCOPE(ELLMTag::UObject);
	TRACE_CPUPROFILER_EVENT_SCOPE(SerializeExport);

	const FExportMapEntry& Export = Header.ExportMap[LocalExportIndex];
	FExportObject& ExportObject = Header.ExportsView[LocalExportIndex];
	UObject* Object = ExportObject.Object;
	check(Object || (ExportObject.bFiltered | ExportObject.bExportLoadFailed));

	TRACE_LOADTIME_SERIALIZE_EXPORT_SCOPE(Object, Export.CookedSerialSize);

	if ((ExportObject.bFiltered | ExportObject.bExportLoadFailed) || !(Object && Object->HasAnyFlags(RF_NeedLoad)))
	{
		if (ExportObject.bExportLoadFailed)
		{
			UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("SerializeExport"),
				TEXT("Skipped failed export %s"), *Header.NameMap.GetName(Export.ObjectName).ToString());
		}
		else if (ExportObject.bFiltered)
		{
			UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbose, Desc, TEXT("SerializeExport"),
				TEXT("Skipped filtered export %s"), *Header.NameMap.GetName(Export.ObjectName).ToString());
		}
		else
		{
			UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("SerializeExport"),
				TEXT("Skipped already serialized export %s"), *Header.NameMap.GetName(Export.ObjectName).ToString());
		}
		return false;
	}

	TOptional<FExportArchive> LocalAr;
	if (!Ar)
	{
#if WITH_EDITOR
		if (&Header == OptionalSegmentHeaderData.GetPtrOrNull())
		{
			Ar = &LocalAr.Emplace(OptionalSegmentSerializationState->IoRequest.GetResultOrDie());
			InitializeExportArchive(*Ar, true);
		}
		else
#endif
		{
			Ar = &LocalAr.Emplace(SerializationState.IoRequest.GetResultOrDie());
			InitializeExportArchive(*Ar, false);
		}
	}

	ProcessExportDependencies(Header, LocalExportIndex, FExportBundleEntry::ExportCommandType_Serialize);

	// If this is a struct, make sure that its parent struct is completely loaded
	if (UStruct* Struct = dynamic_cast<UStruct*>(Object))
	{
		if (UStruct* SuperStruct = dynamic_cast<UStruct*>(ExportObject.SuperObject))
		{
			Struct->SetSuperStruct(SuperStruct);
			if (UClass* ClassObject = dynamic_cast<UClass*>(Object))
			{
				ClassObject->Bind();
			}
		}
	}

	const UClass* LoadClass = Export.ClassIndex.IsNull() ? UClass::StaticClass() : CastEventDrivenIndexToObject<UClass>(Header, Export.ClassIndex, true);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(Object->GetPackage(), ELLMTagSet::Assets);
	LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(LoadClass, ELLMTagSet::AssetClasses);
	UE_TRACE_METADATA_SCOPE_ASSET(Object, LoadClass);

	// cache archetype
	// prevents GetArchetype from hitting the expensive GetArchetypeFromRequiredInfoImpl
	check(ExportObject.TemplateObject);
	CacheArchetypeForObject(Object, ExportObject.TemplateObject);

	Object->ClearFlags(RF_NeedLoad);

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	UObject* PrevSerializedObject = LoadContext->SerializedObject;
	LoadContext->SerializedObject = Object;

	Ar->ExportBufferBegin(Object, Export.CookedSerialOffset, Export.CookedSerialSize);

	const int64 Pos = Ar->Tell();

	check(!Ar->TemplateForGetArchetypeFromLoader);
	Ar->TemplateForGetArchetypeFromLoader = ExportObject.TemplateObject;

	if (Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SerializeDefaultObject);
		Object->GetClass()->SerializeDefaultObject(Object, *Ar);
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SerializeObject);
		UE_SERIALIZE_ACCCESS_SCOPE(Object);
		Object->Serialize(*Ar);
	}
	Ar->TemplateForGetArchetypeFromLoader = nullptr;

	UE_ASYNC_PACKAGE_CLOG(
		Export.CookedSerialSize != uint64(Ar->Tell() - Pos), Fatal, Desc, TEXT("ObjectSerializationError"),
		TEXT("%s: Serial size mismatch: Expected read size %d, Actual read size %d"),
		Object ? *Object->GetFullName() : TEXT("null"), Export.CookedSerialSize, uint64(Ar->Tell() - Pos));

	Ar->ExportBufferEnd();

	Object->SetFlags(RF_LoadCompleted);
	LoadContext->SerializedObject = PrevSerializedObject;

#if DO_CHECK
	if (Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		check(Object->HasAllFlags(RF_NeedPostLoad | RF_WasLoaded));
		//Object->SetFlags(RF_NeedPostLoad | RF_WasLoaded);
	}
#endif

	UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("SerializeExport"), TEXT("Serialized export %s"), *Object->GetPathName());

	// push stats so that we don't overflow number of tags per thread during blocking loading
	LLM_PUSH_STATS_FOR_ASSET_TAGS();

	return true;
}

FAsyncPackage2* FAsyncPackage2::GetCurrentlyExecutingPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* PackageToFilter)
{
	FAsyncPackage2* CurrentlyExecutingPackage = nullptr;
	for (int32 Index = ThreadState.CurrentlyExecutingEventNodeStack.Num() - 1; Index >= 0; --Index)
	{
		FAsyncPackage2* Package = ThreadState.CurrentlyExecutingEventNodeStack[Index]->GetPackage();
		if (Package != nullptr && Package != PackageToFilter)
		{
			CurrentlyExecutingPackage = Package;
			break;
		}
	}
	return CurrentlyExecutingPackage;
}

void FAsyncPackage2::ConditionalReleasePartialRequests(FAsyncLoadingThreadState2& ThreadState)
{
#if WITH_PARTIAL_REQUEST_DURING_RECURSION
	FAsyncLoadingSyncLoadContext* CurrentSyncLoadContext = ThreadState.SyncLoadContextStack.Num() ? ThreadState.SyncLoadContextStack.Top() : nullptr;
	if (CurrentSyncLoadContext && CurrentSyncLoadContext->RequestingPackage)
	{
		EAsyncPackageLoadingState2 RequesterState = CurrentSyncLoadContext->RequestingPackage->AsyncPackageLoadingState;

		if (AsyncPackageLoadingState > RequesterState)
		{
			int32 Index = CurrentSyncLoadContext->RequestedPackages.Find(this);
			if (Index != INDEX_NONE)
			{
				int32 RequestId = CurrentSyncLoadContext->RequestIDs[Index];
				// Release the sync context request tied to our package, allowing the current flush to exit.
				UE_LOG(LogStreaming, Display, TEXT("Package %s has reached state %s > %s, releasing request %d to allow recursive sync load to finish"),
					*Desc.UPackageName.ToString(),
					LexToString(AsyncPackageLoadingState),
					LexToString(RequesterState),
					RequestId
				);
				AsyncLoadingThread.RemovePendingRequests(ThreadState, { RequestId });
			}

			// We want to skip postloads inside recursive loads that happens during serialization and preload
			// since it is mostly what BP expects and how the old LinkerLoad was implemented.
			// In some cases, trying to postload right away during recursion could end-up in data loss (i.e. UE-190649).
			// The expectation is that postload should be run as part of the package causing this load to occur.
			// As a more general rule, we want any synchronous load during deserialization to act like
			// an import. We want the deserialization steps done before returning from the load but
			// we keep postload for after deserialization is done on the referencer too so that postload 
			// sees everything initialized in the case where it also has a reference back to the referencer.
			// Using a partial request flag allows the flush to return as soon as exports are done which
			// gives the opportunity to the caller to get a pointer to an object inside the package even if it's not fully loaded yet.
			if (AsyncPackageLoadingState == EAsyncPackageLoadingState2::ExportsDone && CurrentSyncLoadContext->RequestingPackage->PostLoadGroup)
			{
				UE_LOG(LogStreaming, Display, TEXT("Merging postload groups of package %s with requester package %s"),
					*Desc.UPackageName.ToString(),
					*CurrentSyncLoadContext->RequestingPackage->Desc.UPackageName.ToString());

				// Do not adjust sync load context, we want to be able to exit the current one even if the caller has not finished yet.
				const bool bUpdateSyncLoadContext = false;
				AsyncLoadingThread.MergePostLoadGroups(ThreadState, PostLoadGroup, CurrentSyncLoadContext->RequestingPackage->PostLoadGroup, bUpdateSyncLoadContext);
			}
		}
	}
#endif // WITH_PARTIAL_REQUEST_DURING_RECURSION
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ExportsDone(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ExportsDone);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles || 
		  Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForExternalReads ||
		  Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PreloadLinkerLoadExports);
#else
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles ||
		  Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForExternalReads);
#endif

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ExportsDone;

	if (!Package->bLoadHasFailed && Package->Desc.bCanBeImported)
	{
		FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId, Package->Desc.UPackageName);
		PackageRef.SetAllPublicExportsLoaded();
	}

	if (!Package->Data.ShaderMapHashes.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReleasePreloadedShaderMaps);
		FCoreDelegates::ReleasePreloadedPackageShaderMaps.ExecuteIfBound(Package->Data.ShaderMapHashes);
	}

	Package->CallProgressCallbacks(EAsyncLoadingProgress::Serialized);

	FAsyncLoadingPostLoadGroup* PostLoadGroup = Package->PostLoadGroup;
	check(PostLoadGroup);
	check(PostLoadGroup->PackagesWithExportsToSerializeCount > 0);
	--PostLoadGroup->PackagesWithExportsToSerializeCount;

	Package->ConditionalReleasePartialRequests(ThreadState);
	Package->AsyncLoadingThread.ConditionalBeginPostLoad(ThreadState, PostLoadGroup);
	return EEventLoadNodeExecutionResult::Complete;
}

#if ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING
bool FAsyncPackage2::HasDependencyToPackageDebug(FAsyncPackage2* Package)
{
	TSet<FAsyncPackage2*> Visited;
	TArray<FAsyncPackage2*> Stack;
	for (FAsyncPackage2* ImportedPackage : Data.ImportedAsyncPackages)
	{
		if (ImportedPackage)
		{
			Stack.Push(ImportedPackage);
		}
	}
	while (!Stack.IsEmpty())
	{
		FAsyncPackage2* InnerPackage = Stack.Top();
		Stack.Pop();
		Visited.Add(InnerPackage);
		if (InnerPackage == Package)
		{
			return true;
		}
		for (FAsyncPackage2* ImportedPackage : InnerPackage->Data.ImportedAsyncPackages)
		{
			if (ImportedPackage && !Visited.Contains(ImportedPackage))
			{
				Stack.Push(ImportedPackage);
			}
		}
	}
	return false;
}

void FAsyncPackage2::CheckThatAllDependenciesHaveReachedStateDebug(FAsyncLoadingThreadState2& ThreadState, EAsyncPackageLoadingState2 PackageState, EAsyncPackageLoadingState2 PackageStateForCircularDependencies)
{
	TSet<FAsyncPackage2*> Visited;
	TArray<TTuple<FAsyncPackage2*, TArray<FAsyncPackage2*>>> Stack;

	TArray<FAsyncPackage2*> DependencyChain;
	DependencyChain.Add(this);
	Stack.Push(MakeTuple(this, DependencyChain));
	while (!Stack.IsEmpty())
	{
		TTuple<FAsyncPackage2*, TArray<FAsyncPackage2*>> PackageAndDependencyChain = Stack.Top();
		Stack.Pop();
		FAsyncPackage2* Package = PackageAndDependencyChain.Get<0>();
		DependencyChain = PackageAndDependencyChain.Get<1>();
		
		for (FAsyncPackage2* ImportedPackage : Package->Data.ImportedAsyncPackages)
		{
			if (ImportedPackage && !Visited.Contains(ImportedPackage) && !ThreadState.PackagesOnStack.Contains(Package))
			{
				TArray<FAsyncPackage2*> NextDependencyChain = DependencyChain;
				NextDependencyChain.Add(ImportedPackage);
				
				check(ImportedPackage->AsyncPackageLoadingState >= PackageStateForCircularDependencies);
				if (ImportedPackage->AsyncPackageLoadingState < PackageState)
				{
					bool bHasCircularDependencyToPackage = ImportedPackage->HasDependencyToPackageDebug(this);
					check(bHasCircularDependencyToPackage);
				}

				Visited.Add(ImportedPackage);
				Stack.Push(MakeTuple(ImportedPackage, NextDependencyChain));
			}
		}
	}
}
#endif

FAsyncPackage2* FAsyncPackage2::UpdateDependenciesStateRecursive(FAsyncLoadingThreadState2& ThreadState, FUpdateDependenciesStateRecursiveContext& Context)
{
	FAllDependenciesState& ThisState = this->*Context.StateMemberPtr;
	
	check(ThisState.PreOrderNumber < 0);
	
	if (ThisState.bAllDone)
	{
		return nullptr;
	}

	FAsyncPackage2* WaitingForPackage = ThisState.WaitingForPackage;
	if (WaitingForPackage)
	{
		if (WaitingForPackage->AsyncPackageLoadingState >= Context.WaitForPackageState)
		{
			FAllDependenciesState::RemoveFromWaitList(Context.StateMemberPtr, WaitingForPackage, this);
			WaitingForPackage = nullptr;
		}
		else if (ThreadState.PackagesOnStack.Contains(WaitingForPackage))
		{
			FAllDependenciesState::RemoveFromWaitList(Context.StateMemberPtr, WaitingForPackage, this);
			WaitingForPackage = nullptr;
		}
		else
		{
			return WaitingForPackage;
		}
	}

	ThisState.PreOrderNumber = Context.C;
	++Context.C;
	Context.S.Push(this);
	Context.P.Push(this);

	for (FAsyncPackage2* ImportedPackage : Data.ImportedAsyncPackages)
	{
		if (!ImportedPackage)
		{
			continue;
		}

		if (ThreadState.PackagesOnStack.Contains(ImportedPackage))
		{
			continue;
		}

		FAllDependenciesState& ImportedPackageState = ImportedPackage->*Context.StateMemberPtr;
		if (ImportedPackageState.bAllDone)
		{
			continue;
		}

		if (ImportedPackage->AsyncPackageLoadingState < Context.WaitForPackageState)
		{
			WaitingForPackage = ImportedPackage;
			break;
		}

		ImportedPackageState.UpdateTick(Context.CurrentTick);
		if (ImportedPackageState.PreOrderNumber < 0)
		{
			WaitingForPackage = ImportedPackage->UpdateDependenciesStateRecursive(ThreadState, Context);
			if (WaitingForPackage)
			{
				break;
			}
		}
		else if (!ImportedPackageState.bAssignedToStronglyConnectedComponent)
		{
			while ((Context.P.Top()->*Context.StateMemberPtr).PreOrderNumber > ImportedPackageState.PreOrderNumber)
			{
				Context.P.Pop();
			}
		}
		if (ImportedPackageState.WaitingForPackage)
		{
			WaitingForPackage = ImportedPackageState.WaitingForPackage;
			break;
		}
	}

	if (Context.P.Top() == this)
	{
		FAsyncPackage2* InStronglyConnectedComponent;
		do
		{
			InStronglyConnectedComponent = Context.S.Pop();
			FAllDependenciesState& InStronglyConnectedComponentState = InStronglyConnectedComponent->*Context.StateMemberPtr;
			InStronglyConnectedComponentState.bAssignedToStronglyConnectedComponent = true;
			check(InStronglyConnectedComponent->AsyncPackageLoadingState >= Context.WaitForPackageState);
			if (WaitingForPackage)
			{
#if ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING
				check(HasDependencyToPackageDebug(WaitingForPackage));
#endif
				FAllDependenciesState::AddToWaitList(Context.StateMemberPtr, WaitingForPackage, InStronglyConnectedComponent);
			}
			else
			{
				InStronglyConnectedComponentState.bAllDone = true;
#if ALT2_ENABLE_PACKAGE_DEPENDENCY_DEBUGGING
				InStronglyConnectedComponent->CheckThatAllDependenciesHaveReachedStateDebug(ThreadState, InStronglyConnectedComponent->AsyncPackageLoadingState, Context.WaitForPackageState);
#endif
				Context.OnStateReached(InStronglyConnectedComponent);
			}
		} while (InStronglyConnectedComponent != this);
		Context.P.Pop();
	}
	
	return WaitingForPackage;
}

void FAsyncPackage2::WaitForAllDependenciesToReachState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32& CurrentTickVariable, TFunctionRef<void(FAsyncPackage2*)> OnStateReached)
{
	check(AsyncPackageLoadingState == WaitForPackageState);
	++CurrentTickVariable;

	FUpdateDependenciesStateRecursiveContext Context(StateMemberPtr, WaitForPackageState, CurrentTickVariable, OnStateReached);

	FAllDependenciesState& ThisState = this->*StateMemberPtr;
	check(!ThisState.bAllDone);
	ThisState.UpdateTick(CurrentTickVariable);
	UpdateDependenciesStateRecursive(ThreadState, Context);
	check(ThisState.bAllDone || (ThisState.WaitingForPackage && ThisState.WaitingForPackage->AsyncPackageLoadingState < WaitForPackageState));

	while (FAsyncPackage2* WaitingPackage = ThisState.PackagesWaitingForThisHead)
	{
		FAllDependenciesState& WaitingPackageState = WaitingPackage->*StateMemberPtr;
		WaitingPackageState.UpdateTick(CurrentTickVariable);
		if (WaitingPackageState.PreOrderNumber < 0)
		{
			WaitingPackage->UpdateDependenciesStateRecursive(ThreadState, Context);
		}
		check(WaitingPackageState.bAllDone || (WaitingPackageState.WaitingForPackage && WaitingPackageState.WaitingForPackage->AsyncPackageLoadingState < WaitForPackageState));
	}
}

void FAsyncPackage2::ConditionalBeginProcessPackageExports(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginProcessPackageExports);

	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesSetupState, EAsyncPackageLoadingState2::DependenciesReady, AsyncLoadingThread.ConditionalBeginProcessExportsTick,
		[&ThreadState](FAsyncPackage2* Package)
		{
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::DependenciesReady);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
			if (Package->LinkerLoadState.IsSet())
			{
				Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::CreateLinkerLoadExports;
				Package->GetPackageNode(EEventLoadNode2::Package_CreateLinkerLoadExports).ReleaseBarrier(&ThreadState);
			}
			else
#endif
			{
				Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;
				if (Package->Data.TotalExportBundleCount > 0)
				{
					// Release a single export bundle node to avoid them being picked up recursively during a flush.
					// When a node finishes, it will release another one.
					Package->GetExportBundleNode(EEventLoadNode2::ExportBundle_Process, 0).ReleaseBarrier(&ThreadState);
				}
			}
		});
}

void FAsyncPackage2::ConditionalFinishLoading(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalFinishLoading);
	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesFullyLoadedState, EAsyncPackageLoadingState2::DeferredPostLoadDone, AsyncLoadingThread.ConditionalFinishLoadingTick,
		[&ThreadState](FAsyncPackage2* Package)
		{
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::DeferredPostLoadDone);
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::Finalize;
			Package->AsyncLoadingThread.LoadedPackagesToProcess.Add(Package);

			// Any update to LoadedPackagesToProcess is of interest to the main thread if we are on ALT.
			if (ThreadState.bIsAsyncLoadingThread)
			{
				Package->AsyncLoadingThread.MainThreadWakeEvent.Notify();
			}
		});
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_PostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 InExportBundleIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_PostLoad);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PostLoad);
	check(Package->ExternalReadDependencies.Num() == 0);

	FAsyncPackageScope2 PackageScope(Package);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		return Package->ExecutePostLoadLinkerLoadPackageExports(ThreadState);
	}
#endif

	/*TSet<FAsyncPackage2*> Visited;
	TArray<FAsyncPackage2*> ProcessQueue;
	ProcessQueue.Push(Package);
	while (ProcessQueue.Num() > 0)
	{
		FAsyncPackage2* CurrentPackage = ProcessQueue.Pop();
		Visited.Add(CurrentPackage);
		if (CurrentPackage->AsyncPackageLoadingState < EAsyncPackageLoadingState2::ExportsDone)
		{
			UE_DEBUG_BREAK();
		}
		for (const FPackageId& ImportedPackageId : CurrentPackage->StoreEntry.ImportedPackages)
		{
			FAsyncPackage2* ImportedPackage = CurrentPackage->AsyncLoadingThread.GetAsyncPackage(ImportedPackageId);
			if (ImportedPackage && !Visited.Contains(ImportedPackage))
			{
				ProcessQueue.Push(ImportedPackage);
			}
		}
	}*/
	
	check(InExportBundleIndex < Package->Data.TotalExportBundleCount);

	EEventLoadNodeExecutionResult LoadingState = EEventLoadNodeExecutionResult::Complete;

	if (!Package->bLoadHasFailed)
	{
		// Begin async loading, simulates BeginLoad
		Package->BeginAsyncLoad();

		SCOPED_LOADTIMER(PostLoadObjectsTime);
		TRACE_LOADTIME_POSTLOAD_SCOPE;

		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		TGuardValue<bool> GuardIsRoutingPostLoad(ThreadContext.IsRoutingPostLoad, true);

		const bool bAsyncPostLoadEnabled = FAsyncLoadingThreadSettings::Get().bAsyncPostLoadEnabled;
		const bool bIsMultithreaded = Package->AsyncLoadingThread.IsMultithreaded();

		{
#if WITH_EDITOR
		UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);

		const FAsyncPackageHeaderData* HeaderData;
		if (InExportBundleIndex == 1)
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
		}
		else
		{
			check(InExportBundleIndex == 0);
			HeaderData = &Package->HeaderData;
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
#endif

		while (Package->ExportBundleEntryIndex < HeaderData->ExportBundleEntriesCopyForPostLoad.Num())
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntriesCopyForPostLoad[Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_PostLoadExportBundle")))
			{
				LoadingState = EEventLoadNodeExecutionResult::Timeout;
				break;
			}
			
			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				do
				{
					FExportObject& Export = HeaderData->ExportsView[BundleEntry.LocalExportIndex];
					if (Export.bFiltered | Export.bExportLoadFailed)
					{
						break;
					}

					UObject* Object = Export.Object;
					check(Object);
					check(!Object->HasAnyFlags(RF_NeedLoad));
					if (!Object->HasAnyFlags(RF_NeedPostLoad))
					{
						break;
					}

					check(Object->IsReadyForAsyncPostLoad());
					if (!bIsMultithreaded || (bAsyncPostLoadEnabled && CanPostLoadOnAsyncLoadingThread(Object)))
					{
#if WITH_EDITOR
						SCOPED_LOADTIMER_ASSET_TEXT(*Object->GetPathName());
#endif
						ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
						Object->ConditionalPostLoad();
						ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;
					}
				} while (false);
			}
			++Package->ExportBundleEntryIndex;
		}

		}

		// End async loading, simulates EndLoad
		Package->EndAsyncLoad();
	}
	
	if (LoadingState == EEventLoadNodeExecutionResult::Timeout)
	{
		return LoadingState;
	}

	Package->ExportBundleEntryIndex = 0;

	if (++Package->ProcessedExportBundlesCount == Package->Data.TotalExportBundleCount)
	{
		Package->ProcessedExportBundlesCount = 0;
		if (Package->LinkerRoot && !Package->bLoadHasFailed)
		{
			UE_ASYNC_PACKAGE_LOG(Verbose, Package->Desc, TEXT("AsyncThread: FullyLoaded"),
				TEXT("Async loading of package is done, and UPackage is marked as fully loaded."));
			// mimic old loader behavior for now, but this is more correctly also done in FinishUPackage
			// called from ProcessLoadedPackagesFromGameThread just before completion callbacks
			Package->LinkerRoot->MarkAsFullyLoaded();
		}

		FAsyncLoadingPostLoadGroup* DeferredPostLoadGroup = Package->DeferredPostLoadGroup;
		check(DeferredPostLoadGroup);
		check(DeferredPostLoadGroup->PackagesWithExportsToPostLoadCount > 0);
		--DeferredPostLoadGroup->PackagesWithExportsToPostLoadCount;
		Package->AsyncLoadingThread.ConditionalBeginDeferredPostLoad(ThreadState, DeferredPostLoadGroup);
	}

	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_DeferredPostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 InExportBundleIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjectsGameThread);
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_DeferredPostLoad);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::DeferredPostLoad);

	FAsyncPackageScope2 PackageScope(Package);
	TGuardValue<bool> GuardIsRoutingPostLoad(PackageScope.ThreadContext.IsRoutingPostLoad, true);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		return Package->ExecuteDeferredPostLoadLinkerLoadPackageExports(ThreadState);
	}
#endif

	check(InExportBundleIndex < Package->Data.TotalExportBundleCount);
	EEventLoadNodeExecutionResult LoadingState = EEventLoadNodeExecutionResult::Complete;

	if (!Package->bLoadHasFailed)
	{
		SCOPED_LOADTIMER(PostLoadDeferredObjectsTime);
		TRACE_LOADTIME_POSTLOAD_SCOPE;

		FAsyncLoadingTickScope2 InAsyncLoadingTick(Package->AsyncLoadingThread);

#if WITH_EDITOR
		UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Package->Desc.PIEInstanceID);

		const FAsyncPackageHeaderData* HeaderData;
		if (InExportBundleIndex == 1)
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
		}
		else
		{
			check(InExportBundleIndex == 0);
			HeaderData = &Package->HeaderData;
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
#endif

		while (Package->ExportBundleEntryIndex < HeaderData->ExportBundleEntriesCopyForPostLoad.Num())
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntriesCopyForPostLoad[Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_DeferredPostLoadExportBundle")))
			{
				LoadingState = EEventLoadNodeExecutionResult::Timeout;
				break;
			}

			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				do
				{
					FExportObject& Export = HeaderData->ExportsView[BundleEntry.LocalExportIndex];
					if (Export.bFiltered | Export.bExportLoadFailed)
					{
						break;
					}

					UObject* Object = Export.Object;
					check(Object);
					check(!Object->HasAnyFlags(RF_NeedLoad));
					if (Object->HasAnyFlags(RF_NeedPostLoad))
					{
#if WITH_EDITOR
						SCOPED_LOADTIMER_ASSET_TEXT(*Object->GetPathName());
#endif
						PackageScope.ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
						{
							FScopeCycleCounterUObject ConstructorScope(Object, GET_STATID(STAT_FAsyncPackage_PostLoadObjectsGameThread));
							Object->ConditionalPostLoad();
						}
						PackageScope.ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;
					}
				} while (false);
			}
			++Package->ExportBundleEntryIndex;
		}
	}

	if (LoadingState == EEventLoadNodeExecutionResult::Timeout)
	{
		return LoadingState;
	}

	Package->ExportBundleEntryIndex = 0;

	if (++Package->ProcessedExportBundlesCount == Package->Data.TotalExportBundleCount)
	{
		Package->ProcessedExportBundlesCount = 0;
		check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::DeferredPostLoad);
		Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoadDone;
		Package->ConditionalFinishLoading(ThreadState);
	}

	return EEventLoadNodeExecutionResult::Complete;
}

FEventLoadNode2& FAsyncPackage2::GetPackageNode(EEventLoadNode2 Phase)
{
	check(Phase < EEventLoadNode2::Package_NumPhases);
	return PackageNodes[Phase];
}

FEventLoadNode2& FAsyncPackage2::GetExportBundleNode(EEventLoadNode2 Phase, uint32 ExportBundleIndex)
{
	check(ExportBundleIndex < uint32(Data.TotalExportBundleCount));
	uint32 ExportBundleNodeIndex = ExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases + Phase;
	return Data.ExportBundleNodes[ExportBundleNodeIndex];
}

void FAsyncLoadingThread2::UpdateSyncLoadContext(FAsyncLoadingThreadState2& ThreadState, bool bAutoHandleSyncLoadContext)
{
	if (ThreadState.bIsAsyncLoadingThread && bAutoHandleSyncLoadContext)
	{
		FAsyncLoadingSyncLoadContext* CreatedOnMainThread;
		while (ThreadState.SyncLoadContextsCreatedOnGameThread.Dequeue(CreatedOnMainThread))
		{
			ThreadState.SyncLoadContextStack.Push(CreatedOnMainThread);

			UE_LOG(LogStreaming, VeryVerbose, TEXT("Pushing ALT SyncLoadContext %d"), CreatedOnMainThread->ContextId);
		}
	}
	if (ThreadState.SyncLoadContextStack.IsEmpty())
	{
		return;
	}
	FAsyncLoadingSyncLoadContext* SyncLoadContext = ThreadState.SyncLoadContextStack.Top();
	if (ThreadState.bIsAsyncLoadingThread && bAutoHandleSyncLoadContext)
	{
		// Retire complete/invalid contexts for which we aren't loading any requests
		while (!ContainsAnyRequestID(SyncLoadContext->RequestIDs))
		{
			UE_LOG(LogStreaming, VeryVerbose, TEXT("Popping ALT SyncLoadContext %d"), SyncLoadContext->ContextId);

			SyncLoadContext->ReleaseRef();
			ThreadState.SyncLoadContextStack.Pop();
			if (ThreadState.SyncLoadContextStack.IsEmpty())
			{
				return;
			}
			SyncLoadContext = ThreadState.SyncLoadContextStack.Top();
		}
	}
	else if (!ContainsAnyRequestID(SyncLoadContext->RequestIDs))
	{
		return;
	}
	if (ThreadState.bCanAccessAsyncLoadingThreadData && !SyncLoadContext->bHasFoundRequestedPackages.load(std::memory_order_relaxed))
	{
		// Ensure that we've created the package we're waiting for
		CreateAsyncPackagesFromQueue(ThreadState);
		int32 FoundPackages = 0;
		for (int32 i=0; i < SyncLoadContext->RequestIDs.Num(); ++i)
		{
			int32 RequestID = SyncLoadContext->RequestIDs[i];
			if (SyncLoadContext->RequestedPackages[i] != nullptr)
			{
				++FoundPackages;
			}
			else if (FAsyncPackage2* RequestedPackage = RequestIdToPackageMap.FindRef(RequestID))
			{
				// Set RequestedPackage before setting bHasFoundRequestedPackage so that another thread looking at RequestedPackage
				// after validating that bHasFoundRequestedPackage is true would see the proper value.
				SyncLoadContext->RequestedPackages[i] = RequestedPackage;

#if WITH_PARTIAL_REQUEST_DURING_RECURSION
				FAsyncPackage2* RequestingPackage = SyncLoadContext->RequestingPackage;
				if (RequestingPackage != nullptr)
				{
					// If the flush is coming from a step before the requesting package is back on GT, there is no way to fully flush
					// the requested package unless its already done. We have no choice but to trigger partial loading in that case.
					if (RequestingPackage->AsyncPackageLoadingState < EAsyncPackageLoadingState2::DeferredPostLoad &&
						RequestedPackage->AsyncPackageLoadingState < EAsyncPackageLoadingState2::Complete)
					{
						//
						// Note: Update the FLoadingTests_RecursiveLoads_FullFlushFrom_Serialize test if you edit this error message
						//
						UE_LOG(LogStreaming, Display, TEXT("Flushing package %s (state: %s) recursively from another package %s (state: %s) will result in a partially loaded package to avoid a deadlock."),
							*RequestedPackage->Desc.UPackageName.ToString(),
							LexToString(RequestedPackage->AsyncPackageLoadingState),
							*RequestingPackage->Desc.UPackageName.ToString(),
							LexToString(RequestingPackage->AsyncPackageLoadingState),
							RequestID
						);

						// Check if partial loading rules allow to release the package right now.
						RequestedPackage->ConditionalReleasePartialRequests(ThreadState);
					}
				}
#endif

				IncludePackageInSyncLoadContextRecursive(ThreadState, SyncLoadContext->ContextId, RequestedPackage);
				++FoundPackages;
			}
		}
		
		// Only set when full list is available 
		if (FoundPackages == SyncLoadContext->RequestIDs.Num())
		{
			SyncLoadContext->bHasFoundRequestedPackages.store(true, std::memory_order_release);
		}
	}
	if (SyncLoadContext->bHasFoundRequestedPackages.load(std::memory_order_acquire))
	{
		for (int32 i=0; i < SyncLoadContext->RequestIDs.Num(); ++i)
		{
			int32 RequestID = SyncLoadContext->RequestIDs[i];
			FAsyncPackage2* RequestedPackage = SyncLoadContext->RequestedPackages[i];
			if (RequestedPackage && ThreadState.PackagesOnStack.Contains(RequestedPackage))
			{
				// Flushing a package while it's already being processed on the stack, if we're done preloading we let it pass and remove the request id
				bool bPreloadIsDone = RequestedPackage->AsyncPackageLoadingState >= EAsyncPackageLoadingState2::DeferredPostLoad;
				UE_CLOG(!bPreloadIsDone, LogStreaming, Warning, TEXT("Flushing package %s while it's being preloaded in the same callstack is not possible. Releasing request %d to unblock."), *RequestedPackage->Desc.UPackageName.ToString(), RequestID);
				RemovePendingRequests(ThreadState, {RequestID});
			}
		}
	}
}

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething)
{
	SCOPED_LOADTIMER(AsyncLoadingTime);

	check(IsInGameThread());

	FAsyncLoadingTickScope2 InAsyncLoadingTick(*this);
	uint32 LoopIterations = 0;

	while (true)
	{
		do 
		{
			if ((++LoopIterations) % 32 == 31)
			{
				// We're not multithreaded and flushing async loading
				// Update heartbeat after 32 events
				FThreadHeartBeat::Get().HeartBeat();
				FCoreDelegates::OnAsyncLoadingFlushUpdate.Broadcast();
			}

			if (ThreadState.IsTimeLimitExceeded(TEXT("ProcessAsyncLoadingFromGameThread")))
			{
				return EAsyncPackageState::TimeOut;
			}

			if (IsAsyncLoadingSuspended())
			{
				return EAsyncPackageState::TimeOut;
			}

			if (QueuedPackagesCounter || !PendingPackages.IsEmpty())
			{
				if (CreateAsyncPackagesFromQueue(ThreadState))
				{
					bDidSomething = true;
					break;
				}
				else
				{
					return EAsyncPackageState::TimeOut;
				}
			}

			if (!ThreadState.SyncLoadContextStack.IsEmpty() && ThreadState.SyncLoadContextStack.Top()->ContextId)
			{
				if (EventQueue.ExecuteSyncLoadEvents(ThreadState))
				{
					bDidSomething = true;
					break;
				}
			}
			else if (EventQueue.PopAndExecute(ThreadState))
			{
				bDidSomething = true;
				break;
			}

			if (!ExternalReadQueue.IsEmpty())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WaitingForExternalReads);

				FAsyncPackage2* Package = nullptr;
				ExternalReadQueue.Dequeue(Package);

				EAsyncPackageState::Type Result = Package->ProcessExternalReads(ThreadState, FAsyncPackage2::ExternalReadAction_Wait);
				check(Result == EAsyncPackageState::Complete);

				bDidSomething = true;
				break;
			}

			if (ProcessDeferredDeletePackagesQueue(1))
			{
				bDidSomething = true;
				break;
			}

			return EAsyncPackageState::Complete;
		} while (false);
	}
	check(false);
	return EAsyncPackageState::Complete;
}

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessLoadedPackagesFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething, TConstArrayView<int32> FlushRequestIDs)
{
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;

	if (IsMultithreaded() &&
		ENamedThreads::GetRenderThread() == ENamedThreads::GameThread &&
		!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread)) // render thread tasks are actually being sent to the game thread.
	{
		// The async loading thread might have queued some render thread tasks (we don't have a render thread yet, so these are actually sent to the game thread)
		// We need to process them now before we do any postloads.
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		if (ThreadState.IsTimeLimitExceeded(TEXT("ProcessLoadedPackagesFromGameThread")))
		{
			return EAsyncPackageState::TimeOut;
		}
	}

	TArray<FAsyncPackage2*, TInlineAllocator<4>> LocalCompletedAsyncPackages;
	for (;;)
	{
		FPlatformMisc::PumpEssentialAppMessages();

		if (ThreadState.IsTimeLimitExceeded(TEXT("ProcessAsyncLoadingFromGameThread")))
		{
			Result = EAsyncPackageState::TimeOut;
			break;
		}

		bool bLocalDidSomething = false;
		FAsyncPackage2* PackageToRepriortize;
		while (ThreadState.PackagesToReprioritize.Dequeue(PackageToRepriortize))
		{
			MainThreadEventQueue.UpdatePackagePriority(PackageToRepriortize);
			PackageToRepriortize->ReleaseRef();
		}
		uint64 SyncLoadContextId = !ThreadState.SyncLoadContextStack.IsEmpty() ? ThreadState.SyncLoadContextStack.Top()->ContextId : 0;
		if (SyncLoadContextId)
		{
			bLocalDidSomething |= MainThreadEventQueue.ExecuteSyncLoadEvents(ThreadState);
		}
		else
		{
			bLocalDidSomething |= MainThreadEventQueue.PopAndExecute(ThreadState);
		}

		for (int32 PackageIndex = 0; PackageIndex < LoadedPackagesToProcess.Num(); ++PackageIndex)
		{
			SCOPED_LOADTIMER(ProcessLoadedPackagesTime);
			FAsyncPackage2* Package = LoadedPackagesToProcess[PackageIndex];
			if (Package->SyncLoadContextId < SyncLoadContextId)
			{
				continue;
			}
			bLocalDidSomething = true;
			UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
			check(Package->AsyncPackageLoadingState >= EAsyncPackageLoadingState2::Finalize &&
				  Package->AsyncPackageLoadingState <= EAsyncPackageLoadingState2::CreateClusters);

			if (Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::Finalize)
			{
				TArray<UObject*> CDODefaultSubobjects;
				// Clear async loading flags (we still want RF_Async, but EInternalObjectFlags::AsyncLoading can be cleared)
				for (const FExportObject& Export : Package->Data.Exports)
				{
					if (Export.bFiltered | Export.bExportLoadFailed)
					{
						continue;
					}

					UObject* Object = Export.Object;

					// CDO need special handling, no matter if it's listed in DeferredFinalizeObjects
					UObject* CDOToHandle = ((Object != nullptr) && Object->HasAnyFlags(RF_ClassDefaultObject)) ? Object : nullptr;

					// Clear AsyncLoading in CDO's subobjects.
					if (CDOToHandle != nullptr)
					{
						CDOToHandle->GetDefaultSubobjects(CDODefaultSubobjects);
						for (UObject* SubObject : CDODefaultSubobjects)
						{
							if (SubObject && SubObject->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
							{
								SubObject->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
							}
						}
						CDODefaultSubobjects.Reset();
					}
				}

				Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::PostLoadInstances;
			}

			if (Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PostLoadInstances)
			{
				SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadInstancesGameThread);
				if (Package->PostLoadInstances(ThreadState) == EAsyncPackageState::Complete)
				{
					Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::CreateClusters;
				}
				else
				{
					// PostLoadInstances timed out
					Result = EAsyncPackageState::TimeOut;
				}
			}


			if (Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::CreateClusters)
			{
				SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateClustersGameThread);
				if (Package->bLoadHasFailed || !CanCreateObjectClusters())
				{
					Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::Complete;
				}
				else if (Package->CreateClusters(ThreadState) == EAsyncPackageState::Complete)
				{
					// All clusters created, it's safe to delete the package
					Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::Complete;
				}
				else
				{
					// Cluster creation timed out
					Result = EAsyncPackageState::TimeOut;
				}
			}

			// push stats so that we don't overflow number of tags per thread during blocking loading
			LLM_PUSH_STATS_FOR_ASSET_TAGS();

			if (Result == EAsyncPackageState::TimeOut)
			{
				break;
			}

			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::Complete);

			Package->FinishUPackage();

			{
				FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
				AsyncPackageLookup.Remove(Package->Desc.UPackageId);
				if (AsyncPackageLookup.IsEmpty())
				{
					AsyncPackageLookup.Empty(DefaultAsyncPackagesReserveCount);
				}

#if WITH_EDITOR
				if (!Package->bLoadHasFailed)
				{
					// In the editor we need to find any assets and packages and add them to list for later callback
					EditorCompletedUPackages.Add(Package->LinkerRoot);
					if (GIsEditor)
					{
						for (UObject* Object : Package->ConstructedObjects)
						{
							if (Object->IsAsset())
							{
								EditorLoadedAssets.Add(Object);
							}
						}
					}
				}
#endif

				Package->ClearConstructedObjects();

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
				Package->DetachLinker();
#endif
			}

			// Remove the package from the list before we trigger the callbacks, 
			// this is to ensure we can re-enter FlushAsyncLoading from any of the callbacks
			LoadedPackagesToProcess.RemoveAt(PackageIndex--);

			// Incremented on the Async Thread, now decrement as we're done with this package
			--LoadingPackagesCounter;

			TRACE_COUNTER_SET(AsyncLoadingLoadingPackages, LoadingPackagesCounter);

			LocalCompletedAsyncPackages.Add(Package);
		}

		{
			FScopeLock _(&FailedPackageRequestsCritical);
			CompletedPackageRequests.Append(MoveTemp(FailedPackageRequests));
			FailedPackageRequests.Reset();
		}
		for (FAsyncPackage2* Package : LocalCompletedAsyncPackages)
		{
			UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
			UE_ASYNC_PACKAGE_LOG(Verbose, Package->Desc, TEXT("GameThread: LoadCompleted"),
				TEXT("All loading of package is done, and the async package and load request will be deleted."));


			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::Complete);
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredDelete;
			Package->ClearImportedPackages();

			if (Package->CompletionCallbacks.IsEmpty() && Package->ProgressCallbacks.IsEmpty())
			{
				RemovePendingRequests(ThreadState, Package->RequestIDs);
				Package->ReleaseRef();
			}
			else
			{
				// Note: We need to keep the package alive until the callback has been executed
				CompletedPackageRequests.Add(FCompletedPackageRequest::FromLoadedPackage(Package));
			}
		}
		LocalCompletedAsyncPackages.Reset();

		TArray<FCompletedPackageRequest> RequestsToProcess;

		// Move CompletedPackageRequest out of the global collection to prevent it from changing from within the callbacks.
		// If we're flushing a specific request only call callbacks for that request
		for (int32 CompletedPackageRequestIndex = CompletedPackageRequests.Num() - 1; CompletedPackageRequestIndex >= 0; --CompletedPackageRequestIndex)
		{
			FCompletedPackageRequest& CompletedPackageRequest = CompletedPackageRequests[CompletedPackageRequestIndex];
			if (FlushRequestIDs.Num() == 0 
				|| Algo::AnyOf(FlushRequestIDs, [&CompletedPackageRequest](int32 FlushRequestID) { return CompletedPackageRequest.RequestIDs.Contains(FlushRequestID); }))
			{
				RemovePendingRequests(ThreadState, CompletedPackageRequest.RequestIDs);
				RequestsToProcess.Emplace(MoveTemp(CompletedPackageRequest));
				CompletedPackageRequests.RemoveAt(CompletedPackageRequestIndex);
				bLocalDidSomething = true;
			}
		}

		// Call callbacks in a batch in a stack-local array after all other work has been done to handle
		// callbacks that may call FlushAsyncLoading
		for (FCompletedPackageRequest& CompletedPackageRequest : RequestsToProcess)
		{
			TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);
			CompletedPackageRequest.CallCompletionCallbacks();
			if (CompletedPackageRequest.AsyncPackage)
			{
				CompletedPackageRequest.AsyncPackage->ReleaseRef();
			}
			else
			{
				// Requests for missing packages have no AsyncPackage but they count as packages with remaining work
				--PackagesWithRemainingWorkCounter;
			}
		}

		if (!bLocalDidSomething)
		{
			break;
		}

		bDidSomething = true;
		
		if (FlushRequestIDs.Num() != 0 && !ContainsAnyRequestID(FlushRequestIDs))
		{
			// The only packages we care about have finished loading, so we're good to exit
			break;
		}
	}

	if (Result == EAsyncPackageState::Complete)
	{
	}

	return Result;
}

EAsyncPackageState::Type FAsyncLoadingThread2::TickAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, TConstArrayView<int32> FlushRequestIDs, bool& bDidSomething)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_TickAsyncLoadingGameThread);
	//TRACE_INT_VALUE(QueuedPackagesCounter, QueuedPackagesCounter);
	//TRACE_INT_VALUE(GraphNodeCount, GraphAllocator.TotalNodeCount);
	//TRACE_INT_VALUE(GraphArcCount, GraphAllocator.TotalArcCount);
	//TRACE_MEMORY_VALUE(GraphMemory, GraphAllocator.TotalAllocated);


	check(IsInGameThread());
	check(!IsGarbageCollecting());

#if WITH_EDITOR
	// In the editor loading cannot be part of a transaction as it cannot be undone, and may result in recording half-loaded objects. So we suppress any active transaction while in this stack, and set the editor loading flag
	TGuardValue<ITransaction*> SuppressTransaction(GUndo, nullptr);
	TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, GIsEditor || GIsEditorLoadingPackage);
#endif

	const bool bLoadingSuspended = IsAsyncLoadingSuspended();
	EAsyncPackageState::Type Result = bLoadingSuspended ? EAsyncPackageState::PendingImports : EAsyncPackageState::Complete;

	if (!bLoadingSuspended)
	{
		// Use a time limit scope to restore the time limit to the old values when exiting the scope
		// This is required to ensure a reentrant call here doesn't overwrite time limits permanently.
		FAsyncLoadingThreadState2::FTimeLimitScope TimeLimitScope(bUseTimeLimit, TimeLimit);

		const bool bIsMultithreaded = FAsyncLoadingThread2::IsMultithreaded();
		double TickStartTime = FPlatformTime::Seconds();

		if (!bIsMultithreaded)
		{
			RemoveUnreachableObjects(UnreachableObjects);
		}
		UpdateSyncLoadContext(ThreadState);

		{
			Result = ProcessLoadedPackagesFromGameThread(ThreadState, bDidSomething, FlushRequestIDs);
			double TimeLimitUsedForProcessLoaded = FPlatformTime::Seconds() - TickStartTime;
			UE_CLOG(!GIsEditor && bUseTimeLimit && TimeLimitUsedForProcessLoaded > .1f, LogStreaming, Warning, TEXT("Took %6.2fms to ProcessLoadedPackages"), float(TimeLimitUsedForProcessLoaded) * 1000.0f);
		}

		if (!bIsMultithreaded && Result != EAsyncPackageState::TimeOut)
		{
			Result = TickAsyncThreadFromGameThread(ThreadState, bDidSomething);
		}

		if (Result != EAsyncPackageState::TimeOut)
		{
			if (!bDidSomething && PendingCDOs.Num() > 0)
			{
				bDidSomething = ProcessPendingCDOs(ThreadState);
			}

			// Flush deferred messages
			if (!IsAsyncLoadingPackages())
			{
				FDeferredMessageLog::Flush();
			}
		}

		// Call update callback once per tick on the game thread
		FCoreDelegates::OnAsyncLoadingFlushUpdate.Broadcast();
	}

#if WITH_EDITOR
	ConditionalProcessEditorCallbacks();
#endif

	return Result;
}

FAsyncLoadingThread2::FAsyncLoadingThread2(FIoDispatcher& InIoDispatcher, IAsyncPackageLoader* InUncookedPackageLoader)
	: Thread(nullptr)
	, IoDispatcher(InIoDispatcher)
	, UncookedPackageLoader(InUncookedPackageLoader)
	, PackageStore(FPackageStore::Get())
{
	EventQueue.SetZenaphore(&AltZenaphore);

	AsyncPackageLookup.Reserve(DefaultAsyncPackagesReserveCount);
	PendingPackages.Reserve(DefaultAsyncPackagesReserveCount);
	RequestIdToPackageMap.Reserve(DefaultAsyncPackagesReserveCount);

	EventSpecs.AddDefaulted(EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_NumPhases);
	EventSpecs[EEventLoadNode2::Package_ProcessSummary] = { &FAsyncPackage2::Event_ProcessPackageSummary, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_DependenciesReady] = { &FAsyncPackage2::Event_DependenciesReady, &EventQueue, false };
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	EventSpecs[EEventLoadNode2::Package_CreateLinkerLoadExports] = { &FAsyncPackage2::Event_CreateLinkerLoadExports, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_ResolveLinkerLoadImports] = { &FAsyncPackage2::Event_ResolveLinkerLoadImports, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_PreloadLinkerLoadExports] = { &FAsyncPackage2::Event_PreloadLinkerLoadExports, &EventQueue, false };
#endif
	EventSpecs[EEventLoadNode2::Package_ExportsSerialized] = { &FAsyncPackage2::Event_ExportsDone, &EventQueue, true };

	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_Process] = { &FAsyncPackage2::Event_ProcessExportBundle, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_PostLoad] = { &FAsyncPackage2::Event_PostLoadExportBundle, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_DeferredPostLoad] = { &FAsyncPackage2::Event_DeferredPostLoadExportBundle, &MainThreadEventQueue, false };

	CancelLoadingEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadResumedEvent = FPlatformProcess::GetSynchEventFromPool();
	AsyncLoadingTickCounter = 0;

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FAsyncLoadingThread2::OnPreGarbageCollect);

	FAsyncLoadingThreadState2::TlsSlot = FPlatformTLS::AllocTlsSlot();
	GameThreadState = MakeUnique<FAsyncLoadingThreadState2>(GraphAllocator, IoDispatcher);
	EventQueue.SetOwnerThread(GameThreadState.Get());
	MainThreadEventQueue.SetOwnerThread(GameThreadState.Get());
	FAsyncLoadingThreadState2::Set(GameThreadState.Get());
	
	UE_LOG(LogStreaming, Display, TEXT("AsyncLoading2 - Created: Event Driven Loader: %s, Async Loading Thread: %s, Async Post Load: %s"),
		GEventDrivenLoaderEnabled ? TEXT("true") : TEXT("false"),
		FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled ? TEXT("true") : TEXT("false"),
		FAsyncLoadingThreadSettings::Get().bAsyncPostLoadEnabled ? TEXT("true") : TEXT("false"));
}

FAsyncLoadingThread2::~FAsyncLoadingThread2()
{
	if (Thread)
	{
		ShutdownLoading();
	}
}

void FAsyncLoadingThread2::ShutdownLoading()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);

	delete Thread;
	Thread = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(CancelLoadingEvent);
	CancelLoadingEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadSuspendedEvent);
	ThreadSuspendedEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadResumedEvent);
	ThreadResumedEvent = nullptr;
}

void FAsyncLoadingThread2::StartThread()
{
	// Make sure the GC sync object is created before we start the thread (apparently this can happen before we call InitUObject())
	FGCCSyncObject::Create();

	// Clear game thread initial load arrays
	check(PendingCDOs.Num() == 0);
	PendingCDOs.Empty();
	check(PendingCDOsRecursiveStack.Num() == 0);
	PendingCDOsRecursiveStack.Empty();

	if (FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled && !Thread)
	{
		AsyncLoadingThreadState = MakeUnique<FAsyncLoadingThreadState2>(GraphAllocator, IoDispatcher);
		EventQueue.SetOwnerThread(AsyncLoadingThreadState.Get());

		// When using ALT, we want to wake the main thread ASAP in case it's sleeping for lack of something to do during flush.
		MainThreadEventQueue.SetWakeEvent(&MainThreadWakeEvent);
		PackagesWithRemainingWorkCounter.SetWakeEvent(&MainThreadWakeEvent);

		AsyncLoadingThreadState->bIsAsyncLoadingThread = true;
		AsyncLoadingThreadState->bCanAccessAsyncLoadingThreadData = true;
		GameThreadState->bCanAccessAsyncLoadingThreadData = false;
		UE_LOG(LogStreaming, Log, TEXT("Starting Async Loading Thread."));
		bThreadStarted = true;
		FPlatformMisc::MemoryBarrier();
		UE::Trace::ThreadGroupBegin(TEXT("AsyncLoading"));
		Thread = FRunnableThread::Create(this, TEXT("FAsyncLoadingThread"), 0, TPri_Normal);
		UE::Trace::ThreadGroupEnd();
	}

	UE_LOG(LogStreaming, Display, TEXT("AsyncLoading2 - Thread Started: %s, IsInitialLoad: %s"),
		FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled ? TEXT("true") : TEXT("false"),
		GIsInitialLoad ? TEXT("true") : TEXT("false"));
}

bool FAsyncLoadingThread2::Init()
{
	return true;
}

uint32 FAsyncLoadingThread2::Run()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	AsyncLoadingThreadID = FPlatformTLS::GetCurrentThreadId();

	FAsyncLoadingThreadState2::Set(AsyncLoadingThreadState.Get());

	TRACE_LOADTIME_START_ASYNC_LOADING();

	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetAsyncLoadingThreadMask());
	FMemory::SetupTLSCachesOnCurrentThread();

	FAsyncLoadingThreadState2& ThreadState = *FAsyncLoadingThreadState2::Get();
	
	FZenaphoreWaiter Waiter(AltZenaphore, TEXT("WaitForEvents"));
	enum class EMainState : uint8
	{
		Suspended,
		Loading,
		Waiting,
	};
	EMainState PreviousState = EMainState::Loading;
	EMainState CurrentState = EMainState::Loading;
	while (!bStopRequested.load(std::memory_order_relaxed))
	{
		if (CurrentState == EMainState::Suspended)
		{
			// suspended, sleep until loading can be resumed
			while (!bStopRequested.load(std::memory_order_relaxed))
			{
				if (SuspendRequestedCount.load(std::memory_order_relaxed) == 0 && !IsGarbageCollectionWaiting())
				{
					ThreadResumedEvent->Trigger();
					CurrentState = EMainState::Loading;
					break;
				}

				FPlatformProcess::Sleep(0.001f);
			}
		}
		else if (CurrentState == EMainState::Waiting)
		{
			// no packages in flight and waiting for new load package requests,
			// or done serializing and waiting for deferred deletes of packages being postloaded
			Waiter.Wait();
			CurrentState = EMainState::Loading;
		}
		else if (CurrentState == EMainState::Loading)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(AsyncLoadingTime);

			bool bShouldSuspend = false;
			bool bShouldWaitForExternalReads = false;
			while (!bStopRequested.load(std::memory_order_relaxed))
			{
				if (bShouldSuspend || SuspendRequestedCount.load(std::memory_order_relaxed) > 0 || IsGarbageCollectionWaiting())
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(SuspendAsyncLoading);
					ThreadSuspendedEvent->Trigger();
					CurrentState = EMainState::Suspended;
					break;
				}

				{
					FGCScopeGuard GCGuard;
					{
						FScopeLock UnreachableObjectsLock(&UnreachableObjectsCritical);
						RemoveUnreachableObjects(UnreachableObjects);
					}

					if (bShouldWaitForExternalReads)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(WaitingForExternalReads);
						FAsyncPackage2* Package = nullptr;
						ExternalReadQueue.Dequeue(Package);
						check(Package);
						EAsyncPackageState::Type Result = Package->ProcessExternalReads(ThreadState, FAsyncPackage2::ExternalReadAction_Wait);
						check(Result == EAsyncPackageState::Complete);
						bShouldWaitForExternalReads = false;
						continue;
					}


					if (QueuedPackagesCounter || !PendingPackages.IsEmpty())
					{
						if (CreateAsyncPackagesFromQueue(ThreadState))
						{
							// Fall through to FAsyncLoadEventQueue2 processing unless we need to suspend
							if (SuspendRequestedCount.load(std::memory_order_relaxed) > 0 || IsGarbageCollectionWaiting())
							{
								bShouldSuspend = true;
								continue;
							}
						}
					}

					// do as much event queue processing as we possibly can
					{
						bool bDidSomething = false;
						bool bPopped = false;
						do 
						{
							bPopped = false;
							UpdateSyncLoadContext(ThreadState);
							if (!ThreadState.SyncLoadContextStack.IsEmpty() && ThreadState.SyncLoadContextStack.Top()->ContextId)
							{
								if (EventQueue.ExecuteSyncLoadEvents(ThreadState))
								{
									bPopped = true;
									bDidSomething = true;
								}
							}
							else if (EventQueue.PopAndExecute(ThreadState))
							{
								bPopped = true;
								bDidSomething = true;
							}
							if (SuspendRequestedCount.load(std::memory_order_relaxed) > 0 || IsGarbageCollectionWaiting())
							{
								bShouldSuspend = true;
								bDidSomething = true;
								bPopped = false;
								break;
							}
						} while (bPopped);

						if (bDidSomething)
						{
							continue;
						}
					}

					{
						FAsyncPackage2** Package = ExternalReadQueue.Peek();
						if (Package != nullptr)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(PollExternalReads);
							check(*Package);
							EAsyncPackageState::Type Result = (*Package)->ProcessExternalReads(ThreadState, FAsyncPackage2::ExternalReadAction_Poll);
							if (Result == EAsyncPackageState::Complete)
							{
								ExternalReadQueue.Dequeue();
								continue;
							}
						}
					}

					if (ProcessDeferredDeletePackagesQueue(100))
					{
						continue;
					}
				} // release FGCScopeGuard

				if (PendingIoRequestsCounter.Load() > 0)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(WaitingForIo);
					Waiter.Wait();
					continue;
				}

				if (!ExternalReadQueue.IsEmpty())
				{
					bShouldWaitForExternalReads = true;
					continue;
				}

				// no async loading work left to do for now
				CurrentState = EMainState::Waiting;
				break;
			}
		}
	}
	return 0;
}

EAsyncPackageState::Type FAsyncLoadingThread2::TickAsyncThreadFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething)
{
	check(IsInGameThread());
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	
	if (AsyncThreadReady.GetValue())
	{
		if (ThreadState.IsTimeLimitExceeded(TEXT("TickAsyncThreadFromGameThread")))
		{
			Result = EAsyncPackageState::TimeOut;
		}
		else
		{
			Result = ProcessAsyncLoadingFromGameThread(ThreadState, bDidSomething);
		}
	}

	return Result;
}

void FAsyncLoadingThread2::Stop()
{
	SuspendRequestedCount.fetch_add(1);
	bStopRequested.store(true);
	AltZenaphore.NotifyAll();
}

void FAsyncLoadingThread2::CancelLoading()
{
	check(false);
	// TODO
}

void FAsyncLoadingThread2::SuspendLoading()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SuspendLoading);
	UE_CLOG(!IsInGameThread() || IsInSlateThread(), LogStreaming, Fatal, TEXT("Async loading can only be suspended from the main thread"));
	int32 OldCount = SuspendRequestedCount.fetch_add(1);
	if (OldCount == 0)
	{
		UE_LOG(LogStreaming, Display, TEXT("Suspending async loading"));
		if (IsMultithreaded())
		{
			TRACE_LOADTIME_SUSPEND_ASYNC_LOADING();
			AltZenaphore.NotifyAll();
			ThreadSuspendedEvent->Wait();
		}
	}
	else
	{
		UE_LOG(LogStreaming, Verbose, TEXT("Async loading is already suspended (count: %d)"), OldCount + 1);
	}
}

void FAsyncLoadingThread2::ResumeLoading()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ResumeLoading);
	UE_CLOG(!IsInGameThread() || IsInSlateThread(), LogStreaming, Fatal, TEXT("Async loading can only be resumed from the main thread"));
	int32 OldCount = SuspendRequestedCount.fetch_sub(1);
	UE_CLOG(OldCount < 1, LogStreaming, Fatal, TEXT("Trying to resume async loading when it's not suspended"));
	if (OldCount == 1)
	{
		UE_LOG(LogStreaming, Display, TEXT("Resuming async loading"));
		if (IsMultithreaded())
		{
			ThreadResumedEvent->Wait();
			TRACE_LOADTIME_RESUME_ASYNC_LOADING();
		}
	}
	else
	{
		UE_LOG(LogStreaming, Verbose, TEXT("Async loading is still suspended (count: %d)"), OldCount - 1);
	}
}

float FAsyncLoadingThread2::GetAsyncLoadPercentage(const FName& PackageName)
{
	float LoadPercentage = -1.0f;
	/*
	FAsyncPackage2* Package = FindAsyncPackage(PackageName);
	if (Package)
	{
		LoadPercentage = Package->GetLoadPercentage();
	}
	*/
	return LoadPercentage;
}

static void VerifyObjectLoadFlagsWhenFinishedLoading()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VerifyObjectLoadFlagsWhenFinishedLoading);

	const EInternalObjectFlags AsyncFlags =
		EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;

	const EObjectFlags LoadIntermediateFlags = 
		EObjectFlags::RF_NeedLoad | EObjectFlags::RF_WillBeLoaded |
		EObjectFlags::RF_NeedPostLoad | RF_NeedPostLoadSubobjects;

	ParallelFor(TEXT("VerifyObjectLoadFlagsDebugTask"), GUObjectArray.GetObjectArrayNum(), 512,
		[AsyncFlags,LoadIntermediateFlags](int32 ObjectIndex)
	{
		FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];
		if (UObject* Obj = static_cast<UObject*>(ObjectItem->Object))
		{
			const EInternalObjectFlags InternalFlags = Obj->GetInternalFlags();
			const EObjectFlags Flags = Obj->GetFlags();
			const bool bHasAnyAsyncFlags = !!(InternalFlags & AsyncFlags);
			const bool bHasAnyLoadIntermediateFlags = !!(Flags & LoadIntermediateFlags);
			const bool bHasLoaderImportFlag = !!(InternalFlags & EInternalObjectFlags::LoaderImport);
			const bool bWasLoaded = !!(Flags & RF_WasLoaded);
			const bool bLoadCompleted = !!(Flags & RF_LoadCompleted);

			ensureMsgf(!bHasAnyLoadIntermediateFlags,
				TEXT("Object '%s' (ObjectFlags=%X, InternalObjectFlags=%x) should not have any load flags now")
				TEXT(", or this check is incorrectly reached during active loading."),
				*Obj->GetFullName(),
				Flags,
				InternalFlags);

			ensureMsgf(!bHasLoaderImportFlag || GUObjectArray.IsDisregardForGC(Obj),
				TEXT("Object '%s' (ObjectFlags=%X, InternalObjectFlags=%x) should not have the LoaderImport flag now")
				TEXT(", or this check is incorrectly reached during active loading."),
				*Obj->GetFullName(),
				Flags,
				InternalFlags);

			if (bWasLoaded)
			{
				const bool bIsPackage = Obj->IsA(UPackage::StaticClass());

				ensureMsgf(bIsPackage || bLoadCompleted,
					TEXT("Object '%s' (ObjectFlags=%x, InternalObjectFlags=%x) is a serialized object and should be completely loaded now")
					TEXT(", or this check is incorrectly reached during active loading."),
					*Obj->GetFullName(),
					Flags,
					InternalFlags);

				ensureMsgf(!bHasAnyAsyncFlags,
					TEXT("Object '%s' (ObjectFlags=%x, InternalObjectFlags=%x) is a serialized object and should not have any async flags now")
					TEXT(", or this check is incorrectly reached during active loading."),
					*Obj->GetFullName(),
					Flags,
					InternalFlags);
			}
		}
	});
	UE_LOG(LogStreaming, Log, TEXT("Verified load flags when finished active loading."));
}

void FAsyncLoadingThread2::CollectUnreachableObjects(
	TArrayView<FUObjectItem*> UnreachableObjectItems,
	FUnreachableObjects& OutUnreachableObjects)
{
	check(IsInGameThread());

	OutUnreachableObjects.SetNum(UnreachableObjectItems.Num());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CollectUnreachableObjects);
		ParallelFor(TEXT("CollectUnreachableObjectsTask"), UnreachableObjectItems.Num(), 2048,
			[&UnreachableObjectItems, &OutUnreachableObjects](int32 Index)
		{
			UObject* Object = static_cast<UObject*>(UnreachableObjectItems[Index]->Object);

			FUnreachableObject& Item = OutUnreachableObjects[Index];
			Item.ObjectIndex = GUObjectArray.ObjectToIndex(Object);
			Item.ObjectName = Object->GetFName();

			if (!Object->GetOuter())
			{
				UPackage* Package = static_cast<UPackage*>(Object);
				if (Package->bCanBeImported)
				{
					Item.PackageId = Package->GetPackageId();
				}
			}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
			// Clear garbage objects from linker export tables
			// Normally done from UObject::BeginDestroy but we need to do it already here
			if (FLinkerLoad* ObjectLinker = Object->GetLinker())
			{
				const int32 CachedLinkerIndex = Object->GetLinkerIndex();
				Object->SetLinker(nullptr, INDEX_NONE);
				// As we are garbaging the object, mark it as invalid in the linker
				// Either it is now truly invalid to access this object 
				// (i.e the asset ran upgraded, migrated away from this object and doesn't hold any references to it anymore.)
				// or we are gc'ing the entire asset in which case the linker will eventually get purged and those entry won't be marked invalid anymore on recreation
				FObjectExport& ObjExport = ObjectLinker->ExportMap[CachedLinkerIndex];
				ObjExport.bExportLoadFailed = true;
			}
#endif // ALT2_ENABLE_LINKERLOAD_SUPPORT
		});
	}

	if (GVerifyUnreachableObjects)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(VerifyUnreachableObjects);
		ParallelFor(TEXT("VerifyUnreachableObjectsDebugTask"), UnreachableObjectItems.Num(), 512,
			[this, &UnreachableObjectItems](int32 Index)
		{
			UObject* Object = static_cast<UObject*>(UnreachableObjectItems[Index]->Object);
			if (!Object->GetOuter())
			{
				UPackage* Package = static_cast<UPackage*>(Object);
				if (Package->bCanBeImported)
				{
					FPackageId PackageId = Package->GetPackageId();
					FLoadedPackageRef* PackageRef = GlobalImportStore.FindPackageRef(PackageId);
					if (PackageRef)
					{
						GlobalImportStore.VerifyPackageForRemoval(*PackageRef);
					}
				}
			}
			GlobalImportStore.VerifyObjectForRemoval(Object);
		});
	}
}

void FAsyncLoadingThread2::OnPreGarbageCollect()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncLoadingThread2::OnPreGarbageCollect);
	// Flush the delete queue so that we don't prevent packages from being garbage collected if we're done with them
	ProcessDeferredDeletePackagesQueue();
}

void FAsyncLoadingThread2::RemoveUnreachableObjects(FUnreachableObjects& ObjectsToRemove)
{
	if (ObjectsToRemove.Num() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(RemoveUnreachableObjects);

	const int32 ObjectCount = ObjectsToRemove.Num();

	const int32 OldLoadedPackageCount = GlobalImportStore.GetStoredPackagesCount();
	const int32 OldPublicExportCount = GlobalImportStore.GetStoredPublicExportsCount();

	const double StartTime = FPlatformTime::Seconds();

	GlobalImportStore.RemovePackages(ObjectsToRemove);
	GlobalImportStore.RemovePublicExports(ObjectsToRemove);
	ObjectsToRemove.Reset();

	const int32 NewLoadedPackageCount = GlobalImportStore.GetStoredPackagesCount();
	const int32 NewPublicExportCount = GlobalImportStore.GetStoredPublicExportsCount();
	const int32 RemovedLoadedPackageCount = OldLoadedPackageCount - NewLoadedPackageCount;
	const int32 RemovedPublicExportCount = OldPublicExportCount - NewPublicExportCount;

	const double StopTime = FPlatformTime::Seconds();
	UE_LOG(LogStreaming, Display,
		TEXT("%.3f ms for processing %d objects in RemoveUnreachableObjects(Queued=%d, Async=%d). ")
		TEXT("Removed %d (%d->%d) packages and %d (%d->%d) public exports."),
		(StopTime - StartTime) * 1000,
		ObjectCount,
		GetNumQueuedPackages(), GetNumAsyncPackages(),
		RemovedLoadedPackageCount, OldLoadedPackageCount, NewLoadedPackageCount,
		RemovedPublicExportCount, OldPublicExportCount, NewPublicExportCount);
}

void FAsyncLoadingThread2::NotifyUnreachableObjects(const TArrayView<FUObjectItem*>& UnreachableObjectItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(NotifyUnreachableObjects);

	if (GExitPurge)
	{
		return;
	}

	FScopeLock UnreachableObjectsLock(&UnreachableObjectsCritical);

	// unreachable objects from last GC should typically have been processed already,
	// if not handle them here before adding new ones
	RemoveUnreachableObjects(UnreachableObjects);

	CollectUnreachableObjects(UnreachableObjectItems, UnreachableObjects);

	if (GVerifyObjectLoadFlags && !IsAsyncLoading())
	{
		GlobalImportStore.VerifyLoadedPackages();
		VerifyObjectLoadFlagsWhenFinishedLoading();
	}

	if (GRemoveUnreachableObjectsOnGT)
	{
		RemoveUnreachableObjects(UnreachableObjects);
	}

	// wake up ALT to remove unreachable objects
	AltZenaphore.NotifyAll();
}

/**
 * Call back into the async loading code to inform of the creation of a new object
 * @param Object		Object created
 * @param bSubObjectThatAlreadyExists	Object created as a sub-object of a loaded object
 */
void FAsyncLoadingThread2::NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObjectThatAlreadyExists)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (!ThreadContext.AsyncPackage)
	{
		// Something is creating objects on the async loading thread outside of the actual async loading code
		// e.g. ShaderCodeLibrary::OnExternalReadCallback doing FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event);
		return;
	}

	FAsyncPackage2* AsyncPackage2 = (FAsyncPackage2*)ThreadContext.AsyncPackage;

#if WITH_EDITOR
	// In editor, objects from other packages might be constructed with the wrong package currently on stack (i.e. reinstantiation).
	// It's a lot cleaner and easier to properly dispatch them to their respective package than adding scopes everywhere.
	if (UPackage* ObjectPackage = Object->GetPackage())
	{
		if (FLinkerLoad* LinkerLoad = ObjectPackage->GetLinker())
		{
			if (FAsyncPackage2* AsyncPackage = (FAsyncPackage2*)LinkerLoad->AsyncRoot)
			{
				AsyncPackage->AddConstructedObject(Object, bSubObjectThatAlreadyExists);
				return;
			}
		}

		FPackageId PackageId = ObjectPackage->GetPackageId();
		if (PackageId.IsValid() && PackageId != AsyncPackage2->Desc.UPackageId)
		{
			FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
			if (FAsyncPackage2* AsyncPackage = AsyncPackageLookup.FindRef(PackageId))
			{
				AsyncPackage->AddConstructedObject(Object, bSubObjectThatAlreadyExists);
				return;
			}
		}
	}
#endif
	
	AsyncPackage2->AddConstructedObject(Object, bSubObjectThatAlreadyExists);
}

void FAsyncLoadingThread2::NotifyRegistrationEvent(
	const TCHAR* PackageName,
	const TCHAR* Name,
	ENotifyRegistrationType NotifyRegistrationType,
	ENotifyRegistrationPhase NotifyRegistrationPhase,
	UObject* (*InRegister)(),
	bool InbDynamic,
	UObject* FinishedObject)
{
	if (NotifyRegistrationPhase == ENotifyRegistrationPhase::NRP_Finished)
	{
		ensureMsgf(FinishedObject, TEXT("FinishedObject was not provided by NotifyRegistrationEvent when called with ENotifyRegistrationPhase::NRP_Finished, see call stack for offending code."));
		GlobalImportStore.AddScriptObject(PackageName, Name, FinishedObject);
	}
}

void FAsyncLoadingThread2::NotifyRegistrationComplete()
{
	GlobalImportStore.RegistrationComplete();
	bHasRegisteredAllScriptObjects = true;

	UE_LOG(LogStreaming, Display,
		TEXT("AsyncLoading2 - NotifyRegistrationComplete: Registered %d public script object entries (%.2f KB)"),
		GlobalImportStore.GetStoredScriptObjectsCount(), (float)GlobalImportStore.GetStoredScriptObjectsAllocatedSize() / 1024.f);
}

/*-----------------------------------------------------------------------------
	FAsyncPackage implementation.
-----------------------------------------------------------------------------*/

/**
* Constructor
*/
FAsyncPackage2::FAsyncPackage2(
	FAsyncLoadingThreadState2& ThreadState,
	const FAsyncPackageDesc2& InDesc,
	FAsyncLoadingThread2& InAsyncLoadingThread,
	FAsyncLoadEventGraphAllocator& InGraphAllocator,
	const FAsyncLoadEventSpec* EventSpecs)
: Desc(InDesc)
, AsyncLoadingThread(InAsyncLoadingThread)
, GraphAllocator(InGraphAllocator)
, ImportStore(AsyncLoadingThread.GlobalImportStore)
{
	TRACE_LOADTIME_NEW_ASYNC_PACKAGE(this);
	AddRequestID(ThreadState, Desc.RequestID);

	CreatePackageNodes(EventSpecs);

	ImportStore.AddPackageReference(Desc);
}

void FAsyncPackage2::CreatePackageNodes(const FAsyncLoadEventSpec* EventSpecs)
{
	const int32 BarrierCount = 1;
	
	FEventLoadNode2* Node = reinterpret_cast<FEventLoadNode2*>(PackageNodesMemory);
	for (int32 Phase = 0; Phase < EEventLoadNode2::Package_NumPhases; ++Phase)
	{
		new (Node + Phase) FEventLoadNode2(EventSpecs + Phase, this, -1, BarrierCount);
	}
	PackageNodes = MakeArrayView(Node, EEventLoadNode2::Package_NumPhases);
}

void FAsyncPackage2::CreateExportBundleNodes(const FAsyncLoadEventSpec* EventSpecs)
{
	const int32 BarrierCount = 1;
	for (int32 ExportBundleIndex = 0; ExportBundleIndex < Data.TotalExportBundleCount; ++ExportBundleIndex)
	{
		uint32 NodeIndex = EEventLoadNode2::ExportBundle_NumPhases * ExportBundleIndex;
		for (int32 Phase = 0; Phase < EEventLoadNode2::ExportBundle_NumPhases; ++Phase)
		{
			new (&Data.ExportBundleNodes[NodeIndex + Phase]) FEventLoadNode2(EventSpecs + EEventLoadNode2::Package_NumPhases + Phase, this, ExportBundleIndex, BarrierCount);
		}
	}
}

FAsyncPackage2::~FAsyncPackage2()
{
	TRACE_LOADTIME_DESTROY_ASYNC_PACKAGE(this);
	UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("AsyncThread: Deleted"), TEXT("Package deleted."));

	ImportStore.ReleaseImportedPackageReferences(Desc, HeaderData.ImportedPackageIds);
#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		ImportStore.ReleaseImportedPackageReferences(Desc, OptionalSegmentHeaderData->ImportedPackageIds);
	}
#endif
	ImportStore.ReleasePackageReference(Desc);

	checkf(RefCount.load(std::memory_order_relaxed) == 0, TEXT("RefCount is not 0 when deleting package %s"),
		*Desc.PackagePathToLoad.GetPackageFName().ToString());

	checkf(ConstructedObjects.Num() == 0, TEXT("ClearConstructedObjects() has not been called for package %s"),
		*Desc.PackagePathToLoad.GetPackageFName().ToString());

	FMemory::Free(Data.MemoryBuffer0);
	FMemory::Free(Data.MemoryBuffer1);

	check(PostLoadGroup == nullptr);
	check(DeferredPostLoadGroup == nullptr);
}

void FAsyncPackage2::ReleaseRef()
{
	int32 OldRefCount = RefCount.fetch_sub(1);
	check(OldRefCount > 0);
	if (OldRefCount == 1)
	{
		FAsyncLoadingThread2& AsyncLoadingThreadLocal = AsyncLoadingThread;
		AsyncLoadingThreadLocal.DeferredDeletePackages.Enqueue(this);
		AsyncLoadingThreadLocal.AltZenaphore.NotifyOne();
	}
}

void FAsyncPackage2::ClearImportedPackages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ClearImportedPackages);
	// Reset the ImportedAsyncPackages member array before releasing the async package references.
	// ReleaseRef may queue up the ImportedAsyncPackage for deletion on the ALT.
	// ImportedAsyncPackages of this package can still be accessed by both the GT and the ALT.
	TArrayView<FAsyncPackage2*> LocalImportedAsyncPackages = Data.ImportedAsyncPackages;
	Data.ImportedAsyncPackages = MakeArrayView(Data.ImportedAsyncPackages.GetData(), 0);
	for (FAsyncPackage2* ImportedAsyncPackage : LocalImportedAsyncPackages)
	{
		if (ImportedAsyncPackage)
		{
			ImportedAsyncPackage->ReleaseRef();
		}
	}
}

void FAsyncPackage2::ClearConstructedObjects()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ClearConstructedObjects);

	for (UObject* Object : ConstructedObjects)
	{
		Object->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading | EInternalObjectFlags::Async);
	}
	ConstructedObjects.Empty();

	for (FExportObject& Export : Data.Exports)
	{
		if (Export.bWasFoundInMemory)
		{
			check(Export.Object);
			Export.Object->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading | EInternalObjectFlags::Async);
		}
		else
		{
			checkf(!Export.Object || !Export.Object->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading | EInternalObjectFlags::Async),
				TEXT("Export object: %s (ObjectFlags=%x, InternalObjectFlags=%x)"),
					*Export.Object->GetFullName(),
					Export.Object->GetFlags(),
					Export.Object->GetInternalFlags());
		}
	}

	if (LinkerRoot)
	{
		LinkerRoot->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading | EInternalObjectFlags::Async);
	}
}

void FAsyncPackage2::AddRequestID(FAsyncLoadingThreadState2& ThreadState, int32 Id)
{
	if (Id > 0)
	{
		if (Desc.RequestID == INDEX_NONE)
		{
			// For debug readability
			Desc.RequestID = Id;
		}

		RequestIDs.Add(Id);

		// The Id is most likely already present because it's added as soon as the request is created.
		AsyncLoadingThread.AddPendingRequest(Id);

		TRACE_LOADTIME_ASYNC_PACKAGE_REQUEST_ASSOCIATION(this, Id);
	}
}

/**
 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
 */
double FAsyncPackage2::GetLoadStartTime() const
{
	return LoadStartTime;
}

/**
 * Begin async loading process. Simulates parts of BeginLoad.
 *
 * Objects created during BeginAsyncLoad and EndAsyncLoad will have EInternalObjectFlags::AsyncLoading set
 */
void FAsyncPackage2::BeginAsyncLoad()
{
	if (IsInGameThread())
	{
		AsyncLoadingThread.EnterAsyncLoadingTick();
	}

	// this won't do much during async loading except increase the load count which causes IsLoading to return true
	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	BeginLoad(LoadContext);
}

/**
 * End async loading process. Simulates parts of EndLoad(). 
 */
void FAsyncPackage2::EndAsyncLoad()
{
	check(AsyncLoadingThread.IsAsyncLoadingPackages());

	// this won't do much during async loading except decrease the load count which causes IsLoading to return false
	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	EndLoad(LoadContext);

	if (IsInGameThread())
	{
		AsyncLoadingThread.LeaveAsyncLoadingTick();
	}
}

void FAsyncPackage2::CreateUPackage()
{
	check(!LinkerRoot);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFind);
		LinkerRoot = FindObjectFast<UPackage>(/*Outer*/nullptr, Desc.UPackageName);
	}
	if (!LinkerRoot)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPackageCreate);
		UE_TRACK_REFERENCING_PACKAGE_SCOPED(Desc.PackageReferencer.ReferencerPackageName, Desc.PackageReferencer.ReferencerPackageOp);

#if WITH_EDITOR
		FCookLoadScope CookLoadScope(Desc.PackageReferencer.CookLoadType);
#endif
		// Add scope so that this constructed object is assigned to ourself and not another package up the stack in ImportPackagesRecursive.
		FAsyncPackageScope2 Scope(this);
		LinkerRoot = NewObject<UPackage>(/*Outer*/nullptr, Desc.UPackageName);
		bCreatedLinkerRoot = true;
	}

#if WITH_EDITOR
	// Do not overwrite PIEInstanceID for package that might already have one from a previous load
	// or an in-memory only package created specifically for PIE.
	if (!LinkerRoot->bHasBeenFullyLoaded && LinkerRoot->GetLoadedPath().IsEmpty())
	{
		LinkerRoot->SetPIEInstanceID(Desc.PIEInstanceID);
	}
#endif

	LinkerRoot->SetFlags(RF_Public | RF_WasLoaded);
	LinkerRoot->SetLoadedPath(Desc.PackagePathToLoad);
	LinkerRoot->SetCanBeImportedFlag(Desc.bCanBeImported);
	LinkerRoot->SetPackageId(Desc.UPackageId);
	LinkerRoot->SetPackageFlags(Desc.PackageFlags);

	EInternalObjectFlags FlagsToSet = EInternalObjectFlags::Async;
	if (Desc.bCanBeImported)
	{
		FlagsToSet |= EInternalObjectFlags::LoaderImport;
	}
	LinkerRoot->SetInternalFlags(FlagsToSet);

	// update global import store
	// temp packages can't be imported and are never stored or found in global import store
	if (Desc.bCanBeImported)
	{
		FLoadedPackageRef& PackageRef = AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Desc.UPackageId, Desc.UPackageName);
		UPackage* ExistingPackage = PackageRef.GetPackage();
		if (!ExistingPackage)
		{
			PackageRef.SetPackage(LinkerRoot);
		}
		else if (ExistingPackage != LinkerRoot)
		{
			UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("CreateUPackage: ReplacePackage"),
				TEXT("Replacing renamed package %s (0x%llX) while being referenced by the loader, RefCount=%d"),
				*ExistingPackage->GetName(), ExistingPackage->GetPackageId().ValueForDebugging(),
				PackageRef.GetRefCount());

			AsyncLoadingThread.GlobalImportStore.ReplaceReferencedRenamedPackage(PackageRef, LinkerRoot);
		}
	}

	if (bCreatedLinkerRoot)
	{
		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateUPackage: AddPackage"),
			TEXT("New UPackage created."));
	}
	else
	{
		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateUPackage: UpdatePackage"),
			TEXT("Existing UPackage updated."));
	}
}

EAsyncPackageState::Type FAsyncPackage2::ProcessExternalReads(FAsyncLoadingThreadState2& ThreadState, EExternalReadAction Action)
{
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForExternalReads);
	double WaitTime;
	if (Action == ExternalReadAction_Poll)
	{
		WaitTime = -1.f;
	}
	else// if (Action == ExternalReadAction_Wait)
	{
		WaitTime = 0.f;
	}

	while (ExternalReadIndex < ExternalReadDependencies.Num())
	{
		FExternalReadCallback& ReadCallback = ExternalReadDependencies[ExternalReadIndex];
		if (!ReadCallback(WaitTime))
		{
			return EAsyncPackageState::TimeOut;
		}
		++ExternalReadIndex;
	}

	ExternalReadDependencies.Empty();
	GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(&ThreadState);
	return EAsyncPackageState::Complete;
}

EAsyncPackageState::Type FAsyncPackage2::PostLoadInstances(FAsyncLoadingThreadState2& ThreadState)
{
	if (bLoadHasFailed)
	{
		return EAsyncPackageState::Complete;
	}
	const int32 ExportCount = Data.Exports.Num();
	while (PostLoadInstanceIndex < ExportCount && !ThreadState.IsTimeLimitExceeded(TEXT("PostLoadInstances")))
	{
		const FExportObject& Export = Data.Exports[PostLoadInstanceIndex++];

		if (!(Export.bFiltered | Export.bExportLoadFailed))
		{
			UClass* ObjClass = Export.Object->GetClass();
			ObjClass->PostLoadInstance(Export.Object);
		}
	}
	return PostLoadInstanceIndex == ExportCount ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

EAsyncPackageState::Type FAsyncPackage2::CreateClusters(FAsyncLoadingThreadState2& ThreadState)
{
	const int32 ExportCount = Data.Exports.Num();
	while (DeferredClusterIndex < ExportCount)
	{
		const FExportObject& Export = Data.Exports[DeferredClusterIndex++];

		if (!(Export.bFiltered | Export.bExportLoadFailed) && Export.Object->CanBeClusterRoot())
		{
			Export.Object->CreateCluster();
			if (DeferredClusterIndex < ExportCount && ThreadState.IsTimeLimitExceeded(TEXT("CreateClusters")))
			{
				break;
			}
		}
	}

	return DeferredClusterIndex == ExportCount ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

void FAsyncPackage2::FinishUPackage()
{
	if (LinkerRoot && !bLoadHasFailed)
	{
		// Mark package as having been fully loaded and update load time.
		LinkerRoot->MarkAsFullyLoaded();
		LinkerRoot->SetLoadTime((float)(FPlatformTime::Seconds() - LoadStartTime));
	}
}

#if WITH_EDITOR
void FAsyncLoadingThread2::ConditionalProcessEditorCallbacks()
{
	check(IsInGameThread());
	if (!GameThreadState->SyncLoadContextStack.IsEmpty())
	{
		return;
	}

	FBlueprintSupport::FlushReinstancingQueue();

	while (!EditorCompletedUPackages.IsEmpty() || !EditorLoadedAssets.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncLoadingThread2::ConditionalProcessEditorCallbacks);

		TArray<UObject*> LocalEditorLoadedAssets;
		Swap(LocalEditorLoadedAssets, EditorLoadedAssets);
		TArray<UPackage*> LocalEditorCompletedUPackages;
		Swap(LocalEditorCompletedUPackages, EditorCompletedUPackages);

		// Call the global delegate for package endloads and set the bHasBeenLoaded flag that is used to
		// check which packages have reached this state
		for (UPackage* CompletedUPackage : LocalEditorCompletedUPackages)
		{
			CompletedUPackage->SetHasBeenEndLoaded(true);
		}
		FCoreUObjectDelegates::OnEndLoadPackage.Broadcast(
			FEndLoadPackageContext{ LocalEditorCompletedUPackages, 0, false /* bSynchronous */ });

		// In editor builds, call the asset load callback. This happens in both editor and standalone to match EndLoad
		for (UObject* LoadedObject : LocalEditorLoadedAssets)
		{
			if (LoadedObject)
			{
				UE_TRACK_REFERENCING_PACKAGE_SCOPED(LoadedObject->GetPackage()->GetFName(), PackageAccessTrackingOps::NAME_Load);
				FCoreUObjectDelegates::OnAssetLoaded.Broadcast(LoadedObject);
			}
		}
	}
}
#endif

void FAsyncPackage2::AddCompletionCallback(TUniquePtr<FLoadPackageAsyncDelegate>&& Callback)
{
	// This is to ensure that there is no one trying to subscribe to a already loaded package
	//check(!bLoadHasFinished && !bLoadHasFailed);
	CompletionCallbacks.Emplace(MoveTemp(Callback));
}

void FAsyncPackage2::AddProgressCallback(TUniquePtr<FLoadPackageAsyncProgressDelegate>&& Callback)
{
	ProgressCallbacks.Emplace(MoveTemp(Callback));
}

UE_TRACE_EVENT_BEGIN(CUSTOM_LOADTIMER_LOG, LoadAsyncPackageInternal, NoSync)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, PackageName)
UE_TRACE_EVENT_END()

int32 FAsyncLoadingThread2::LoadPackageInternal(const FPackagePath& InPackagePath, FName InCustomName, TUniquePtr<FLoadPackageAsyncDelegate>&& InCompletionDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate>&& InProgressDelegate, EPackageFlags InPackageFlags, int32 InPIEInstanceID, int32 InPackagePriority, const FLinkerInstancingContext* InInstancingContext, uint32 InLoadFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackage);

	const FName PackageNameToLoad = InPackagePath.GetPackageFName();
	if (InCustomName == PackageNameToLoad)
	{
		InCustomName = NAME_None;
	}
	SCOPED_CUSTOM_LOADTIMER(LoadAsyncPackageInternal)
		ADD_CUSTOM_LOADTIMER_META(LoadAsyncPackageInternal, PackageName, *WriteToString<256>(PackageNameToLoad));

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (FCoreDelegates::OnAsyncLoadPackage.IsBound())
	{
		checkf(IsInGameThread(), TEXT("FCoreDelegates::OnAsyncLoadPackage is not thread-safe and deprecated, update the callees to be thread-safe and register to FCoreDelegates::GetOnAsyncLoadPackage() instead before calling LoadPackageAsync from any other thread than the game-thread."));
		const FName PackageName = InCustomName.IsNone() ? PackageNameToLoad : InCustomName;
		FCoreDelegates::OnAsyncLoadPackage.Broadcast(PackageName.ToString());
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (FCoreDelegates::GetOnAsyncLoadPackage().IsBound())
	{
		const FName PackageName = InCustomName.IsNone() ? PackageNameToLoad : InCustomName;
		FCoreDelegates::GetOnAsyncLoadPackage().Broadcast(PackageName.ToString());
	}

	// Generate new request ID and add it immediately to the global request list (it needs to be there before we exit
	// this function, otherwise it would be added when the packages are being processed on the async thread).
	const int32 RequestId = IAsyncPackageLoader::GetNextRequestId();
	TRACE_LOADTIME_BEGIN_REQUEST(RequestId);
	AddPendingRequest(RequestId);

	FPackageReferencer PackageReferencer;
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	if (AccumulatedScopeData)
	{
		PackageReferencer.ReferencerPackageName = AccumulatedScopeData->PackageName;
		PackageReferencer.ReferencerPackageOp = AccumulatedScopeData->OpName;
	}
#endif
#if WITH_EDITOR
	PackageReferencer.CookLoadType = FCookLoadScope::GetCurrentValue();
#endif
	PackageRequestQueue.Enqueue(FPackageRequest::Create(RequestId, InPackageFlags, InLoadFlags, InPIEInstanceID, InPackagePriority, InInstancingContext, InPackagePath, InCustomName, MoveTemp(InCompletionDelegate), MoveTemp(InProgressDelegate), PackageReferencer));
	++QueuedPackagesCounter;
	++PackagesWithRemainingWorkCounter;

	TRACE_COUNTER_SET(AsyncLoadingQueuedPackages, QueuedPackagesCounter);
	TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);

	AltZenaphore.NotifyOne();

	return RequestId;
}

int32 FAsyncLoadingThread2::LoadPackage(const FPackagePath& InPackagePath, FLoadPackageAsyncOptionalParams InParams)
{
	return LoadPackageInternal(InPackagePath, InParams.CustomPackageName, MoveTemp(InParams.CompletionDelegate), MoveTemp(InParams.ProgressDelegate), InParams.PackageFlags, InParams.PIEInstanceID, InParams.PackagePriority, InParams.InstancingContext, InParams.LoadFlags);
}

int32 FAsyncLoadingThread2::LoadPackage(const FPackagePath& InPackagePath, FName InCustomName, FLoadPackageAsyncDelegate InCompletionDelegate, EPackageFlags InPackageFlags, int32 InPIEInstanceID, int32 InPackagePriority, const FLinkerInstancingContext* InInstancingContext, uint32 InLoadFlags)
{
	// Allocate delegate before going async, it is not safe to copy delegates by value on other threads
	TUniquePtr<FLoadPackageAsyncDelegate> CompletionDelegate = InCompletionDelegate.IsBound()
		? MakeUnique<FLoadPackageAsyncDelegate>(MoveTemp(InCompletionDelegate))
		: TUniquePtr<FLoadPackageAsyncDelegate>();

	return LoadPackageInternal(InPackagePath, InCustomName, MoveTemp(CompletionDelegate), TUniquePtr<FLoadPackageAsyncProgressDelegate>(), InPackageFlags, InPIEInstanceID, InPackagePriority, InInstancingContext, InLoadFlags);
}

void FAsyncLoadingThread2::QueueMissingPackage(FAsyncLoadingThreadState2& ThreadState, FAsyncPackageDesc2& PackageDesc, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate, TUniquePtr<FLoadPackageAsyncProgressDelegate>&& PackageProgressDelegate)
{
	const FName FailedPackageName = PackageDesc.UPackageName;

	static TSet<FName> SkippedPackages;
	bool bIsAlreadySkipped = false;

	SkippedPackages.Add(FailedPackageName, &bIsAlreadySkipped);

	bool bIssueWarning = !bIsAlreadySkipped;
#if WITH_EDITOR
	bIssueWarning &= ((PackageDesc.LoadFlags & (LOAD_NoWarn | LOAD_Quiet)) == 0);
#endif
	if (bIssueWarning)
	{
		bool bIsScriptPackage = FPackageName::IsScriptPackage(WriteToString<FName::StringBufferSize>(FailedPackageName));
		bIssueWarning &= !bIsScriptPackage;
	}

	UE_CLOG(bIssueWarning, LogStreaming, Warning,
		TEXT("LoadPackage: SkipPackage: %s (0x%llX) - The package to load does not exist on disk or in the loader"),
		*FailedPackageName.ToString(), PackageDesc.PackageIdToLoad.ValueForDebugging());

	if (PackageProgressDelegate.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PackageProgressCallback_Failed);

		const FLoadPackageAsyncProgressParams Params
		{
			.PackageName = FailedPackageName,
			.LoadedPackage = nullptr,
			.ProgressType = EAsyncLoadingProgress::Failed
		};

		PackageProgressDelegate->ExecuteIfBound(Params);
	}

	if (PackageLoadedDelegate.IsValid())
	{
		FScopeLock _(&FailedPackageRequestsCritical);
		FailedPackageRequests.Add(
			FCompletedPackageRequest::FromMissingPackage(PackageDesc, MoveTemp(PackageLoadedDelegate)));
	}
	else
	{
		RemovePendingRequests(ThreadState, TArrayView<int32>(&PackageDesc.RequestID, 1));
		--PackagesWithRemainingWorkCounter;
		TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);
	}
}

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_AsyncLoadingTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(AsyncLoading);

	// CSV_CUSTOM_STAT(FileIO, EDLEventQueueDepth, (int32)GraphAllocator.TotalNodeCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FileIO, QueuedPackagesQueueDepth, GetNumQueuedPackages(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FileIO, ExistingQueuedPackagesQueueDepth, GetNumAsyncPackages(), ECsvCustomStatOp::Set);

	bool bDidSomething = false;
	TickAsyncLoadingFromGameThread(ThreadState, bUseTimeLimit, bUseFullTimeLimit, TimeLimit, {}, bDidSomething);
	return IsAsyncLoadingPackages() ? EAsyncPackageState::TimeOut : EAsyncPackageState::Complete;
}

void FAsyncLoadingThread2::WarnAboutPotentialSyncLoadStall(FAsyncLoadingSyncLoadContext* SyncLoadContext)
{
	for (int32 Index = 0; Index < SyncLoadContext->RequestIDs.Num(); ++Index)
	{
		int32 RequestId = SyncLoadContext->RequestIDs[Index];
		if (ContainsRequestID(RequestId))
		{
			FAsyncPackage2* Package = SyncLoadContext->RequestedPackages[Index];
			UE_LOG(LogStreaming, Warning, TEXT("A flush request appear to be stuck waiting on package %s at state %s to reach state > %s"),
				*Package->Desc.UPackageName.ToString(),
				LexToString(Package->AsyncPackageLoadingState),
				LexToString(SyncLoadContext->RequestingPackage->AsyncPackageLoadingState)
			);
		}
	}
}

void FAsyncLoadingThread2::FlushLoadingFromLoadingThread(FAsyncLoadingThreadState2& ThreadState, TConstArrayView<int32> RequestIDs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FlushLoadingFromLoadingThread);

	if (RequestIDs.IsEmpty())
	{
		return;
	}

	FAsyncLoadingSyncLoadContext* SyncLoadContext = new FAsyncLoadingSyncLoadContext(RequestIDs);

	FAsyncPackage2* GetCurrentlyExecutingPackage = FAsyncPackage2::GetCurrentlyExecutingPackage(ThreadState);
	SyncLoadContext->RequestingPackage = GetCurrentlyExecutingPackage;

	UE_LOG(LogStreaming, VeryVerbose, TEXT("Pushing ALT SyncLoadContext %d"), SyncLoadContext->ContextId);
	AsyncLoadingThreadState->SyncLoadContextStack.Push(SyncLoadContext);

	// We handle the context push/pop manually since we don't interact with the main thread during this time.
	static constexpr bool bAutoHandleSyncLoadContext = false;
	UpdateSyncLoadContext(ThreadState, bAutoHandleSyncLoadContext);

	int64 DidNothingCount = 0;
	while (ContainsAnyRequestID(SyncLoadContext->RequestIDs))
	{
		const bool bDidSomething = EventQueue.ExecuteSyncLoadEvents(ThreadState);
		if (bDidSomething)
		{
			DidNothingCount = 0;
		}
		else if (DidNothingCount++ == 100)
		{
			WarnAboutPotentialSyncLoadStall(SyncLoadContext);
		}
	}

	check(AsyncLoadingThreadState->SyncLoadContextStack.Top() == SyncLoadContext);
	UE_LOG(LogStreaming, VeryVerbose, TEXT("Popping ALT SyncLoadContext %d"), AsyncLoadingThreadState->SyncLoadContextStack.Top()->ContextId);
	AsyncLoadingThreadState->SyncLoadContextStack.Pop();
	delete SyncLoadContext;
}

void FAsyncLoadingThread2::FlushLoading(TConstArrayView<int32> RequestIDs)
{
	if (IsAsyncLoadingPackages())
	{
		// We can't possibly support flushing from async loading thread unless we have the partial request support active.
#if WITH_PARTIAL_REQUEST_DURING_RECURSION
		const bool bIsFlushSupportedOnCurrentThread = IsInGameThread() || IsInAsyncLoadingThread();
#else
		const bool bIsFlushSupportedOnCurrentThread = IsInGameThread();
#endif
		ELoaderType LoaderType = GetLoaderType();

		if (!bIsFlushSupportedOnCurrentThread)
		{
			// As there is no ensure for test builds, use custom stacktrace logging so we can see these problems in playtest and fix them.
			// But just return in shipping build to avoid crashing as the side effect of missing flushes are not always fatal.
			// Don't forget to adjust unittest in FLoadingTests_InvalidFlush_FromWorker if changing this.
#if !UE_BUILD_SHIPPING
			const FString& ThreadName = FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId());
			const FString Heading = 
				FString::Printf(
					TEXT("The current loader '%s' is unable to FlushAsyncLoading from the current thread '%s'. Flush will be ignored."), 
					LexToString(LoaderType),
					*ThreadName
			);
			FDebug::DumpStackTraceToLog(*Heading, ELogVerbosity::Error);
#endif
			return;
		}

		// Flushing async loading while loading is suspend will result in infinite stall
		UE_CLOG(SuspendRequestedCount.load(std::memory_order_relaxed) > 0, LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended"));

		if (RequestIDs.Num() != 0 && !ContainsAnyRequestID(RequestIDs))
		{
			return;
		}

		FAsyncLoadingThreadState2* ThreadState = FAsyncLoadingThreadState2::Get();
		if (ThreadState && ThreadState->bIsAsyncLoadingThread)
		{
			FlushLoadingFromLoadingThread(*ThreadState, RequestIDs);
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

		// if the sync count is 0, then this flush is not triggered from a sync load, broadcast the delegate in that case
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		if (ThreadContext.SyncLoadUsingAsyncLoaderCount == 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCoreDelegates::OnAsyncLoadingFlush);
			FCoreDelegates::OnAsyncLoadingFlush.Broadcast();
		}

		double StartTime = FPlatformTime::Seconds();
		double LogFlushTime = StartTime;

		FAsyncPackage2* CurrentlyExecutingPackage = nullptr;
		if (GameThreadState->CurrentlyExecutingEventNodeStack.Num() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HandleCurrentlyExecutingEventNode);

			UE_CLOG(RequestIDs.Num() == 0, LogStreaming, Fatal, TEXT("Flushing async loading while creating, serializing or postloading an object is not permitted"));
			CurrentlyExecutingPackage = GameThreadState->CurrentlyExecutingEventNodeStack.Top()->GetPackage();
			GameThreadState->PackagesOnStack.Push(CurrentlyExecutingPackage);
			// Update the state of any package that is waiting for the currently executing one
			while (FAsyncPackage2* WaitingPackage = CurrentlyExecutingPackage->AllDependenciesFullyLoadedState.PackagesWaitingForThisHead)
			{
				FAsyncPackage2::FAllDependenciesState::RemoveFromWaitList(&FAsyncPackage2::AllDependenciesFullyLoadedState, CurrentlyExecutingPackage, WaitingPackage);
				WaitingPackage->ConditionalFinishLoading(*GameThreadState);
			}
#if !WITH_PARTIAL_REQUEST_DURING_RECURSION
			if (GameThreadState->bCanAccessAsyncLoadingThreadData)
			{
				if (FAsyncLoadingPostLoadGroup* PostLoadGroup = CurrentlyExecutingPackage->PostLoadGroup)
				{
					check(PostLoadGroup->Packages.Contains(CurrentlyExecutingPackage));
					check(PostLoadGroup->PackagesWithExportsToSerializeCount > 0);
					if (PostLoadGroup->Packages.Num() > 1)
					{
						PostLoadGroup->Packages.Remove(CurrentlyExecutingPackage);
						--PostLoadGroup->PackagesWithExportsToSerializeCount;
						ConditionalBeginPostLoad(*GameThreadState, PostLoadGroup);
						CurrentlyExecutingPackage->PostLoadGroup = new FAsyncLoadingPostLoadGroup();
						CurrentlyExecutingPackage->PostLoadGroup->SyncLoadContextId = CurrentlyExecutingPackage->SyncLoadContextId;
						CurrentlyExecutingPackage->PostLoadGroup->Packages.Add(CurrentlyExecutingPackage);
						CurrentlyExecutingPackage->PostLoadGroup->PackagesWithExportsToSerializeCount = 1;
					}
				}
			}
#endif
		}

		FAsyncLoadingSyncLoadContext* SyncLoadContext = nullptr;
		if (RequestIDs.Num() != 0 && GOnlyProcessRequiredPackagesWhenSyncLoading)
		{
			SyncLoadContext = new FAsyncLoadingSyncLoadContext(RequestIDs);
			SyncLoadContext->RequestingPackage = CurrentlyExecutingPackage;

			UE_LOG(LogStreaming, VeryVerbose, TEXT("Pushing GT SyncLoadContext %d"), SyncLoadContext->ContextId);

			GameThreadState->SyncLoadContextStack.Push(SyncLoadContext);
			if (AsyncLoadingThreadState)
			{
				SyncLoadContext->AddRef();
				AsyncLoadingThreadState->SyncLoadContextsCreatedOnGameThread.Enqueue(SyncLoadContext);
				AltZenaphore.NotifyOne();
			}
		}

		// Flush async loaders by not using a time limit. Needed for e.g. garbage collection.
		{
			while (IsAsyncLoadingPackages())
			{
				bool bDidSomething = false;
				EAsyncPackageState::Type Result = TickAsyncLoadingFromGameThread(*GameThreadState, false, false, 0.0, RequestIDs, bDidSomething);
				if (RequestIDs.Num() != 0 && !ContainsAnyRequestID(RequestIDs))
				{
					break;
				}

#if 0
				if (!bDidSomething)
				{
					if (!AsyncLoadingThreadState && !PendingIoRequestsCounter)
					{
						UE_LOG(LogStreaming, Fatal, TEXT("Loading is stuck, flush will never finish"));
					}
				}
#endif

				if (IsMultithreaded())
				{
					// Update the heartbeat and sleep a little if we had nothing to do unless ALT has made progress.
					// If we're not multithreading, the heartbeat is updated after each package has been processed.
					FThreadHeartBeat::Get().HeartBeat();

					// Only going idle if nothing has been done
					if (!bDidSomething)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(MainThreadWaitingOnAsyncLoadingThread);
						// Still let the main thread tick at 60fps for processing message loop/etc.
						MainThreadWakeEvent.WaitFor(UE::FMonotonicTimeSpan::FromMilliseconds(16));
						// Reset the manual event right after we wake up so we don't miss any trigger.
						// Worst case, we'll do an empty spin before going back to sleep.
						MainThreadWakeEvent.Reset();
					}

					// Flush logging when running cook-on-the-fly and waiting for packages
					if (IsRunningCookOnTheFly() && FPlatformTime::Seconds() - LogFlushTime > 1.0)
					{
						GLog->FlushThreadedLogs(EOutputDeviceRedirectorFlushOptions::Async);
						LogFlushTime = FPlatformTime::Seconds();
					}
				}

				// push stats so that we don't overflow number of tags per thread during blocking loading
				LLM_PUSH_STATS_FOR_ASSET_TAGS();
			}
		}

		if (SyncLoadContext)
		{
			check(GameThreadState->SyncLoadContextStack.Top() == SyncLoadContext);

			UE_LOG(LogStreaming, VeryVerbose, TEXT("Popping GT SyncLoadContext %d"), SyncLoadContext->ContextId);

			SyncLoadContext->ReleaseRef();
			GameThreadState->SyncLoadContextStack.Pop();
			AltZenaphore.NotifyOne();
		}

		if (CurrentlyExecutingPackage)
		{
			check(GameThreadState->PackagesOnStack.Top() == CurrentlyExecutingPackage);
			GameThreadState->PackagesOnStack.Pop();
		}

#if WITH_EDITOR
		ConditionalProcessEditorCallbacks();
#endif

		// If we asked to flush everything, we should no longer have anything in the pipeline
		check(RequestIDs.Num() != 0 || !IsAsyncLoadingPackages());
	}
}

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessLoadingUntilCompleteFromGameThread(FAsyncLoadingThreadState2& ThreadState, TFunctionRef<bool()> CompletionPredicate, double TimeLimit)
{
	if (!IsAsyncLoadingPackages())
	{
		return EAsyncPackageState::Complete;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessLoadingUntilComplete);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

	// Flushing async loading while loading is suspend will result in infinite stall
	UE_CLOG(SuspendRequestedCount.load(std::memory_order_relaxed) > 0, LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended"));

	bool bUseTimeLimit = TimeLimit > 0.0f;
	double TimeLoadingPackage = 0.0f;

	// If there's an active sync load context we need to supress is for the duration of this call since we've no idea what the CompletionPredicate is waiting for
	uint64 NoSyncLoadContextId = 0;
	uint64& SyncLoadContextId = ThreadState.SyncLoadContextStack.IsEmpty() ? NoSyncLoadContextId : ThreadState.SyncLoadContextStack.Top()->ContextId;
	TGuardValue<uint64> GuardSyncLoadContextId(SyncLoadContextId, 0);

	bool bLoadingComplete = !IsAsyncLoadingPackages() || CompletionPredicate();
	while (!bLoadingComplete && (!bUseTimeLimit || TimeLimit > 0.0f))
	{
		double TickStartTime = FPlatformTime::Seconds();
		if (ProcessLoadingFromGameThread(ThreadState, bUseTimeLimit, bUseTimeLimit, TimeLimit) == EAsyncPackageState::Complete)
		{
			return EAsyncPackageState::Complete;
		}

		if (IsMultithreaded())
		{
			// Update the heartbeat and sleep. If we're not multithreading, the heartbeat is updated after each package has been processed
			// only update the heartbeat up to the limit of the hang detector to ensure if we get stuck in this loop that the hang detector gets a chance to trigger
			if (TimeLoadingPackage < FThreadHeartBeat::Get().GetHangDuration())
			{
				FThreadHeartBeat::Get().HeartBeat();
			}
			FPlatformProcess::SleepNoStats(0.0001f);
		}

		double TimeDelta = (FPlatformTime::Seconds() - TickStartTime);
		TimeLimit -= TimeDelta;
		TimeLoadingPackage += TimeDelta;

		bLoadingComplete = !IsAsyncLoadingPackages() || CompletionPredicate();
	}

	return bLoadingComplete ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

IAsyncPackageLoader* MakeAsyncPackageLoader2(FIoDispatcher& InIoDispatcher, IAsyncPackageLoader* InUncookedPackageLoader)
{
	return new FAsyncLoadingThread2(InIoDispatcher, InUncookedPackageLoader);
}
