// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#include "Serialization/AsyncLoading2.h"
#include "Serialization/AsyncPackageLoader.h"
#include "IO/PackageStore.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreStats.h"
#include "HAL/IConsoleManager.h"
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
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "HAL/LowLevelMemStats.h"
#include "HAL/IPlatformFileOpenLogWrapper.h"
#include "Modules/ModuleManager.h"
#include "Containers/MpscQueue.h"
#include "Containers/SpscQueue.h"
#include "Misc/PathViews.h"
#include "UObject/LinkerLoad.h"
#include "Containers/SpscQueue.h"

#include <atomic>

#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
PRAGMA_DISABLE_OPTIMIZATION
#endif

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

FArchive& operator<<(FArchive& Ar, FExportBundleEntry& ExportBundleEntry)
{
	Ar << ExportBundleEntry.LocalExportIndex;
	Ar << ExportBundleEntry.CommandType;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FExportBundleHeader& ExportBundleHeader)
{
	Ar << ExportBundleHeader.SerialOffset;
	Ar << ExportBundleHeader.FirstEntryIndex;
	Ar << ExportBundleHeader.EntryCount;

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

TRACE_DECLARE_INT_COUNTER(AsyncLoadingQueuedPackages, TEXT("AsyncLoading/PackagesQueued"));
TRACE_DECLARE_INT_COUNTER(AsyncLoadingLoadingPackages, TEXT("AsyncLoading/PackagesLoading"));
TRACE_DECLARE_INT_COUNTER(AsyncLoadingPackagesWithRemainingWork, TEXT("AsyncLoading/PackagesWithRemainingWork"));
TRACE_DECLARE_INT_COUNTER(AsyncLoadingPendingIoRequests, TEXT("AsyncLoading/PendingIoRequests"));
TRACE_DECLARE_MEMORY_COUNTER(AsyncLoadingTotalLoaded, TEXT("AsyncLoading/TotalLoaded"));

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

struct FPackageRequest
{
	int32 RequestId = -1;
	int32 Priority = -1;
	EPackageFlags PackageFlags = PKG_None;
#if WITH_EDITOR
	int32 PIEInstanceID = INDEX_NONE;
#endif
#if WITH_EDITORONLY_DATA
	FLinkerInstancingContext InstancingContext;
#endif
	FName CustomName;
	FPackagePath PackagePath;
	TUniquePtr<FLoadPackageAsyncDelegate> PackageLoadedDelegate;
	FPackageRequest* Next = nullptr;

	FLinkerInstancingContext* GetInstancingContext()
	{
#if WITH_EDITORONLY_DATA
		return &InstancingContext;
#else
		return nullptr;
#endif
	}

	static FPackageRequest Create(int32 RequestId, EPackageFlags PackageFlags, int32 PIEInstanceID, int32 Priority, const FLinkerInstancingContext* InstancingContext, const FPackagePath& PackagePath, FName CustomName, TUniquePtr<FLoadPackageAsyncDelegate> PackageLoadedDelegate)
	{
		return FPackageRequest
		{
			RequestId,
			Priority,
			PackageFlags,
#if WITH_EDITOR
			PIEInstanceID,
#endif
#if WITH_EDITORONLY_DATA
			InstancingContext ? *InstancingContext : FLinkerInstancingContext(),
#endif
			CustomName,
			PackagePath,
			MoveTemp(PackageLoadedDelegate),
			nullptr
		};
	}
};

struct FAsyncPackageDesc2
{
	// A unique request id for each external call to LoadPackage
	int32 RequestID;
	// The request id of the referencer propagated down the import chain from the most recent load request
	int32 ReferencerRequestId;
	// Package priority
	int32 Priority;
	/** The flags that should be applied to the package */
	EPackageFlags PackageFlags;
#if WITH_EDITOR
	/** PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;
#endif
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
			Request.RequestId,
			Request.Priority,
			Request.PackageFlags,
#if WITH_EDITOR
			Request.PIEInstanceID,
#endif
			FPackageId::FromName(UPackageName),
			PackageIdToLoad,
			UPackageName,
			MoveTemp(Request.PackagePath),
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
			ImportingPackageDesc.ReferencerRequestId,
			ImportingPackageDesc.Priority,
			PKG_None,
#if WITH_EDITOR
			INDEX_NONE,
#endif
			ImportedPackageId,
			PackageIdToLoad,
			UPackageName,
			MoveTemp(PackagePathToLoad),
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
		FPublicExportMap()
		{
		}

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

		void Remove(uint64 ExportHash)
		{
			TArrayView<uint64> Keys = GetKeys();
			int32 Index = Algo::LowerBound(Keys, ExportHash);
			if (Index < Count && Keys[Index] == ExportHash)
			{
				TArrayView<int32> Values = GetValues();
				Values[Index] = -1;
			}
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

		void PinForGC()
		{
			for (int32 ObjectIndex : GetValues())
			{
				if (ObjectIndex >= 0)
				{
					UObject* Object = static_cast<UObject*>(GUObjectArray.IndexToObject(ObjectIndex)->Object);
					checkf(!Object->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Object->GetFullName());
					Object->SetInternalFlags(EInternalObjectFlags::LoaderImport);
				}
			}
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

	UPackage* Package = nullptr;
	FPublicExportMap PublicExportMap;
	int32 RefCount = 0;
	bool bAreAllPublicExportsLoaded = false;
	bool bIsMissing = false;
	bool bHasFailed = false;
	bool bHasBeenLoadedDebug = false;

public:
	FLoadedPackageRef()
	{
	}

	FLoadedPackageRef(const FLoadedPackageRef& Other) = delete;

	FLoadedPackageRef(FLoadedPackageRef&& Other) = default;

	FLoadedPackageRef& operator=(const FLoadedPackageRef& Other) = delete;

	FLoadedPackageRef& operator=(FLoadedPackageRef&& Other) = default;
	
	inline int32 GetRefCount() const
	{
		return RefCount;
	}

	inline UPackage* GetPackage() const
	{
#if DO_CHECK
		if (Package)
		{
			check(!bIsMissing);
			check(!Package->IsUnreachable());
		}
		else
		{
			check(!bAreAllPublicExportsLoaded);
		}
#endif
		return Package;
	}

	inline void SetPackage(UPackage* InPackage)
	{
		check(!bAreAllPublicExportsLoaded);
		check(!bIsMissing);
		check(!bHasFailed);
		check(!Package);
		Package = InPackage;
	}

	inline bool AreAllPublicExportsLoaded() const
	{
		return bAreAllPublicExportsLoaded;
	}

	inline void SetAllPublicExportsLoaded()
	{
		check(!bIsMissing);
		check(!bHasFailed);
		check(Package);
		bIsMissing = false;
		bAreAllPublicExportsLoaded = true;
		bHasBeenLoadedDebug = true;
	}

	inline void SetIsMissingPackage()
	{
		check(!bAreAllPublicExportsLoaded);
		check(!Package);
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
		check(Package);
		bAreAllPublicExportsLoaded = false;
		PublicExportMap.Remove(ExportHash);
	}

	UObject* GetPublicExport(uint64 ExportHash)
	{
		return PublicExportMap.Find(ExportHash);
	}

	void PinPublicExportsForGC()
	{
		UE_ASYNC_UPACKAGE_DEBUG(Package);

		if (GUObjectArray.IsDisregardForGC(Package))
		{
			return;
		}
		PublicExportMap.PinForGC();
		checkf(!Package->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport), TEXT("%s"), *Package->GetFullName());
		Package->SetInternalFlags(EInternalObjectFlags::LoaderImport);
	}

	void UnpinPublicExportsForGC()
	{
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

	uint32 GetStoredScriptObjectsAllocatedSize() const
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

	inline FLoadedPackageRef& FindPackageRefChecked(FPackageId PackageId)
	{
		FLoadedPackageRef* PackageRef = FindPackageRef(PackageId);
		UE_CLOG(!PackageRef, LogStreaming, Fatal, TEXT("FindPackageRefChecked: Package with id 0x%llX has been deleted"), PackageId.ValueForDebugging());
		return *PackageRef;
	}

	inline FLoadedPackageRef& AddPackageRef(FPackageId PackageId, FName PackageNameIfKnown)
	{
		LLM_SCOPE_BYNAME(TEXT("AsyncLoadPackageStore"));

		FLoadedPackageRef& PackageRef = Packages.FindOrAdd(PackageId);
		// is this the first reference to a package that already exists?
		if (PackageRef.RefCount == 0)
		{
#if WITH_EDITOR
			if (!PackageRef.Package && !PackageNameIfKnown.IsNone())
			{
				UPackage* FoundPackage = FindObjectFast<UPackage>(nullptr, PackageNameIfKnown);
				if (FoundPackage)
				{
					PackageRef.SetPackage(FoundPackage);
					FoundPackage->SetCanBeImportedFlag(true);
					FoundPackage->SetPackageId(PackageId);
					if (FoundPackage->bHasBeenFullyLoaded)
					{
						PackageRef.SetAllPublicExportsLoaded();
					}
				}
			}
#endif
			if (PackageRef.Package)
			{
				PackageRef.PinPublicExportsForGC();
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
			PackageRef.Package ? *PackageRef.Package->GetName() : TEXT("None"),
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
		if (PackageRef.RefCount == 0 && PackageRef.Package)
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
				TEXT("PackageId '0x%llX' with ref count %d should not have a ref count now")
				TEXT(", or this check is incorrectly reached during active loading."),
				PackageId.ValueForDebugging(),
				Ref.GetRefCount());
		}
	}

	void RemovePackage(UPackage* Package)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RemovePackage);
		VerifyPackageForRemoval(Package);
		FPackageId PackageId = Package->GetPackageId();
		RemovePackage(PackageId, Package->GetFName());
	}

	void RemovePackages(const FUnreachableObjects& ObjectsToRemove)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RemovePackages);
		for (const FUnreachableObject& Item : ObjectsToRemove)
		{
			const FPackageId& PackageId = Item.PackageId;
			if (PackageId.IsValid())
			{
				RemovePackage(PackageId, Item.ObjectName);
			}
		}
	}

	void RemovePackage(FPackageId PackageId, FName PackageName)
	{
		UE_ASYNC_PACKAGEID_DEBUG(PackageId);

		FLoadedPackageRef PackageRef;
		bool bRemoved = Packages.RemoveAndCopyValue(PackageId, PackageRef);
		if (bRemoved)
		{
			if (PackageRef.GetRefCount() > 0)
			{
				UE_LOG(LogStreaming, Fatal,
					TEXT("FGlobalImportStore::RemovePackage: %s (0x%llX) - ")
					TEXT("Package removed while still being referenced, RefCount %d > 0."),
					*PackageName.ToString(),
					PackageId.ValueForDebugging(),
					PackageRef.GetRefCount());
			}
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
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id 0x%llX:0x%llX is missing in GlobalImportStore. ")
					TEXT("Reason unknown. Double delete? Bug or hash collision?"),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					GCObject->GetInternalFlags(),
					PackageId.ValueForDebugging(),
					PublicExportKey->GetExportHash());

				UE_CLOG(ExistingObject != GCObject, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id 0x%llX:0x%llX is not matching the object '%s' in GlobalImportStore. "),
					TEXT("Reason unknown. Overwritten after it was added? Bug or hash collision?"),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					GCObject->GetInternalFlags(),
					PackageId.ValueForDebugging(),
					PublicExportKey->GetExportHash(),
					*ExistingObject->GetFullName());
			}
			else
			{
				UE_LOG(LogStreaming, Warning,
					TEXT("FGlobalImportStore::VerifyObjectForRemoval: The package for the serialized GC object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id 0x%llX:0x%llX is missing in GlobalImportStore. ")
					TEXT("Most likely this object has been moved into this package after it was loaded, while the original package is still around."),
					*GCObject->GetFullName(),
					GCObject->GetFlags(),
					GCObject->GetInternalFlags(),
					PackageId.ValueForDebugging(),
					PublicExportKey->GetExportHash());
			}
		}
	}

	void VerifyPackageForRemoval(UPackage* Package)
	{
		check(Package && Package->CanBeImported());

		FPackageId PackageId = Package->GetPackageId();
		FLoadedPackageRef* PackageRef = FindPackageRef(PackageId);

		UE_CLOG(!PackageRef, LogStreaming, Fatal,
			TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (0x%llX) - ")
			TEXT("A loaded package that can be imported by other packages is missing in GlobalImportStore."),
			*Package->GetName(),
			PackageId.ValueForDebugging());

		UE_CLOG(PackageRef->GetRefCount() > 0, LogStreaming, Fatal,
			TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (0x%llX) - ")
			TEXT("Package removed while still being referenced, RefCount %d > 0."),
			*Package->GetName(),
			PackageId.ValueForDebugging(),
			PackageRef->GetRefCount());

		for (int32 ObjectIndex : PackageRef->GetPublicExportObjectIndices())
		{
			if (ObjectIndex >= 0)
			{
				UObject* Object = static_cast<UObject*>(GUObjectArray.IndexToObject(ObjectIndex)->Object);
				ensureMsgf(!Object->HasAnyInternalFlags(EInternalObjectFlags::LoaderImport),
						TEXT("FGlobalImportStore::VerifyPackageForRemoval: The loaded public export object '%s' with flags (ObjectFlags=%x, InternalObjectFlags=%x) and id 0x%llX:0x%llX is probably still referenced by the loader."),
						*Object->GetFullName(),
						Object->GetFlags(),
						Object->GetInternalFlags());

				FPublicExportKey* PublicExportKey = ObjectIndexToPublicExport.Find(ObjectIndex);
				UE_CLOG(!PublicExportKey, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (0x%llX) - ")
					TEXT("The loaded public export object '%s' is missing in GlobalImportStore."),
					*Package->GetName(),
					PackageId.ValueForDebugging(),
					*Object->GetFullName());

				FPackageId ObjectPackageId = PublicExportKey->GetPackageId();
				UE_CLOG(ObjectPackageId != PackageId, LogStreaming, Fatal,
					TEXT("FGlobalImportStore::VerifyPackageForRemoval: %s (0x%llX) - ")
					TEXT("The loaded public export object '%s' has a mismatching package id 0x%llX in GlobalImportStore."),
					*Package->GetName(),
					PackageId.ValueForDebugging(),
					*Object->GetFullName(),
					ObjectPackageId.ValueForDebugging());

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

			UE_LOG(LogStreaming, Warning,
				TEXT("FGlobalImportStore::StoreGlobalObject: The constructed public export object '%s' with index %d and id 0x%llX:0x%llX collides with object '%s' (ObjectFlags=%X, InternalObjectFlags=%x) with index %d in GlobalImportStore. ")
				TEXT("The existing object will be replaced since it was most likely renamed within the package after it was loaded."),
				Object ? *Object->GetFullName() : TEXT("null"),
				ObjectIndex,
				Key.GetPackageId().Value(), Key.GetExportHash(),
				*ExistingObject->GetFullName(),
				ExistingObject->GetFlags(),
				ExistingObject->GetInternalFlags(),
				ExistingObjectIndex);

			ExistingObject->AtomicallyClearInternalFlags(EInternalObjectFlags::LoaderImport);
			ObjectIndexToPublicExport.Remove(ExistingObjectIndex);
		}

		FPublicExportKey* ExistingKey = ObjectIndexToPublicExport.Find(ObjectIndex);
		if (ExistingKey)
		{
			UE_CLOG(*ExistingKey != Key, LogStreaming, Fatal,
				TEXT("FGlobalImportStore::StoreGlobalObject: The constructed public export object '%s' with index %d and id 0x%llX:0x%llX already exists in GlobalImportStore but with a different key 0x%llX:0x%llX."),
				Object ? *Object->GetFullName() : TEXT("null"),
				ObjectIndex,
				Key.GetPackageId().Value(), Key.GetExportHash(),
				ExistingKey->GetPackageId().Value(), ExistingKey->GetExportHash());
		}

		FLoadedPackageRef& PackageRef = FindPackageRefChecked(Key.GetPackageId());
		PackageRef.StorePublicExport(ExportHash, Object);
		ObjectIndexToPublicExport.Add(ObjectIndex, Key);
	}

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

struct FAsyncPackageHeaderData
{
	uint32 CookedHeaderSize = 0;
	uint32 ExportCount = 0; // Need to keep this count around until after ExportMap is being cleared
	TOptional<FZenPackageVersioningInfo> VersioningInfo;
	FNameMap NameMap;
	FName PackageName;
	// Backed by IoBuffer
	const FZenPackageSummary* PackageSummary = nullptr;
	TArrayView<const uint64> ImportedPublicExportHashes;
	TArrayView<const FPackageObjectIndex> ImportMap;
	TArrayView<const FExportMapEntry> ExportMap;
	TArrayView<const uint8> ArcsData;
	// Backed by allocation in FAsyncPackageData
	TArrayView<FPackageId> ImportedPackageIds;
	TArrayView<FName> ImportedPackageNames;
	TArrayView<FExportBundleHeader> ExportBundleHeaders;
	TArrayView<FExportBundleEntry> ExportBundleEntries;

	void OnReleaseHeaderBuffer()
	{
		PackageSummary = nullptr;
		ImportedPublicExportHashes = TArrayView<const uint64>();
		ImportMap = TArrayView<const FPackageObjectIndex>();
		ExportMap = TArrayView<const FExportMapEntry>();
		ArcsData = TArrayView<const uint8>();
	}
};

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
struct FAsyncPackageLinkerLoadHeaderData
{
	TArray<uint64> ImportedPublicExportHashes;
	TArray<FPackageObjectIndex> ImportMap;
	TArray<uint64> PublicExportHashes;
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
	void AddImportedPackageReference(FPackageId ImportedPackageId, FName PackageNameIfKnown)
	{
		GlobalImportStore.AddPackageRef(ImportedPackageId, PackageNameIfKnown);
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
	FExportArchive(const uint8* AllExportDataPtr, const uint8* CurrentExportPtr, uint64 AllExportDataSize)
	{
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
		ActiveFPLB = &InlineFPLB;
#endif
		ActiveFPLB->OriginalFastPathLoadBuffer = AllExportDataPtr;
		ActiveFPLB->StartFastPathLoadBuffer = CurrentExportPtr;
		ActiveFPLB->EndFastPathLoadBuffer = AllExportDataPtr + AllExportDataSize;
	}

	~FExportArchive()
	{
	}
	void ExportBufferBegin(UObject* Object, uint64 InExportCookedFileSerialOffset, uint64 InExportSerialSize)
	{
		CurrentExport = Object;
		CookedSerialOffset = InExportCookedFileSerialOffset;
		BufferSerialOffset = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
		CookedSerialSize = InExportSerialSize;
	}

	void ExportBufferEnd()
	{
		CurrentExport = nullptr;
		CookedSerialOffset = 0;
		BufferSerialOffset = 0;
		CookedSerialSize = 0;
	}

	void CheckBufferPosition(const TCHAR* Text, uint64 Offset = 0)
	{
#if DO_CHECK
		const uint64 BufferPosition = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer) + Offset;
		const bool bIsInsideExportBuffer =
			(BufferSerialOffset <= BufferPosition) && (BufferPosition <= BufferSerialOffset + CookedSerialSize);

		UE_ASYNC_PACKAGE_CLOG(
			!bIsInsideExportBuffer,
			Error, *PackageDesc, TEXT("FExportArchive::InvalidPosition"),
			TEXT("%s: Position %llu is outside of the current export buffer (%lld,%lld)."),
			Text,
			BufferPosition,
			BufferSerialOffset, BufferSerialOffset + CookedSerialSize);
#endif
	}

	void Skip(int64 InBytes)
	{
		CheckBufferPosition(TEXT("InvalidSkip"), InBytes);
		ActiveFPLB->StartFastPathLoadBuffer += InBytes;
	}

	virtual int64 TotalSize() override
	{
		return HeaderData->CookedHeaderSize + (ActiveFPLB->EndFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
	}

	virtual int64 Tell() override
	{
		int64 CookedFilePosition = (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
		CookedFilePosition -= BufferSerialOffset;
		CookedFilePosition += CookedSerialOffset;
		return CookedFilePosition;
	}

	virtual void Seek(int64 Position) override
	{
		uint64 BufferPosition = (uint64)Position;
		BufferPosition -= CookedSerialOffset;
		BufferPosition += BufferSerialOffset;
		ActiveFPLB->StartFastPathLoadBuffer = ActiveFPLB->OriginalFastPathLoadBuffer + BufferPosition;
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
			CurrentExport ? *CurrentExport->GetFullName() : TEXT("null"), ExportIndex, Exports.Num());

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
			if (ExportIndex < Exports.Num())
			{
				Object = Exports[ExportIndex].Object;

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
		FixupSoftObjectPathForInstancedPackage(ID);
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
	//~ End FArchive::FLinkerLoad Interface

private:
	friend FAsyncPackage2;
#if (!DEVIRTUALIZE_FLinkerLoad_Serialize)
	FArchive::FFastPathLoadBuffer InlineFPLB;
	FArchive::FFastPathLoadBuffer* ActiveFPLB;
#endif

	UObject* TemplateForGetArchetypeFromLoader = nullptr;

	FAsyncPackageDesc2* PackageDesc = nullptr;
	FPackageImportStore* ImportStore = nullptr;
	TArray<FExternalReadCallback>* ExternalReadDependencies;
	const FAsyncPackageHeaderData* HeaderData = nullptr;
	TArrayView<const FExportObject> Exports;
	UObject* CurrentExport = nullptr;
	uint64 CookedSerialOffset = 0;
	uint64 CookedSerialSize = 0;
	uint64 BufferSerialOffset = 0;

	/** Set when the package is being loaded as an instance; empty otherwise. */
	FNameBuilder InstancedPackageSourceName;
	FNameBuilder InstancedPackageInstanceName;
	void FixupSoftObjectPathForInstancedPackage(FSoftObjectPath& InOutSoftObjectPath)
	{
		if (InstancedPackageSourceName.Len() > 0 && InstancedPackageInstanceName.Len() > 0)
		{
			FNameBuilder TmpSoftObjectPathBuilder;
			InOutSoftObjectPath.ToString(TmpSoftObjectPathBuilder);

			FStringView InstancedPackageSourceNameView = InstancedPackageSourceName.ToView();
			FStringView TmpSoftObjectPathView = TmpSoftObjectPathBuilder.ToView();

			if (TmpSoftObjectPathView.StartsWith(InstancedPackageSourceNameView) && (TmpSoftObjectPathView.Len() == InstancedPackageSourceNameView.Len() || TmpSoftObjectPathView[InstancedPackageSourceNameView.Len()] == TEXT('.')))
			{
				TmpSoftObjectPathBuilder.ReplaceAt(0, InstancedPackageSourceNameView.Len(), InstancedPackageInstanceName.ToView());
				InOutSoftObjectPath.SetPath(TmpSoftObjectPathBuilder.ToView());
			}
		}
	}
};

enum class EAsyncPackageLoadingState2 : uint8
{
	NewPackage,
	WaitingForIo,
	ProcessPackageSummary,
	SetupDependencies,
	ProcessExportBundles,
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
	void ReleaseBarrier(FAsyncLoadingThreadState2* ThreadState = nullptr);
	EEventLoadNodeExecutionResult Execute(FAsyncLoadingThreadState2& ThreadState);

	FAsyncPackage2* GetPackage() const
	{
		return Package;
	}

	int32 ReferencerRequestId() const;

private:
	friend class FAsyncLoadEventQueue2;

	FEventLoadNode2()
	{

	}

	void ProcessDependencies(FAsyncLoadingThreadState2& ThreadState);
	void Fire(FAsyncLoadingThreadState2* ThreadState = nullptr);

	union
	{
		FEventLoadNode2* SingleDependent;
		FEventLoadNode2** MultipleDependents;
	};
	std::atomic<FEventLoadNode2*> Next { nullptr };
	uint32 DependenciesCount = 0;
	uint32 DependenciesCapacity = 0;
	std::atomic<int32> BarrierCount { 0 };
	std::atomic<bool> bIsUpdatingDependencies { false };
	std::atomic<bool> bIsDone { false };

	const FAsyncLoadEventSpec* Spec = nullptr;
	FAsyncPackage2* Package = nullptr;
	int32 ImportOrExportIndex = -1;
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

	bool PopAndExecute(FAsyncLoadingThreadState2& ThreadState);
	void Push(FEventLoadNode2* Node);
	bool ExecuteSyncLoadEvents(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingSyncLoadContext& SyncLoadContext);

private:
	void Deplete();

	FZenaphore* Zenaphore = nullptr;
	FEventLoadNode2 Sentinel; // `Sentinel.Next` is the head of the queue
	std::atomic<FEventLoadNode2*> Tail{ &Sentinel };
	FEventLoadNode2* LocalHead = nullptr;
	FEventLoadNode2* LocalTail = nullptr;
	FEventLoadNode2* TimedOutEventNode = nullptr;
	int32 ExecuteSyncLoadEventsCallCounter = 0;
	bool bIsInPopAndExecute = false;

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
	FAsyncLoadingSyncLoadContext(int32 InRequestId)
		: RequestId(InRequestId)
	{
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
	int32 RequestId;
	FAsyncPackage2* RequestedPackageDebug = nullptr;
	FAsyncPackage2* RequestingPackageDebug = nullptr;
	bool bHasFoundRequestedPackage = false;

private:
	std::atomic<int32> RefCount = 1;

	static uint64 NextContextId;
};

uint64 FAsyncLoadingSyncLoadContext::NextContextId = 1;

struct FAsyncLoadingThreadState2
{
	static void Set(FAsyncLoadingThreadState2* State)
	{
		check(TlsSlot != 0);
		check(!FPlatformTLS::GetTlsValue(TlsSlot));
		FPlatformTLS::SetTlsValue(TlsSlot, State);
	}

	static FAsyncLoadingThreadState2* Get()
	{
		check(TlsSlot != 0);
		return static_cast<FAsyncLoadingThreadState2*>(FPlatformTLS::GetTlsValue(TlsSlot));
	}

	FAsyncLoadingThreadState2(FAsyncLoadEventGraphAllocator& InGraphAllocator, FIoDispatcher& InIoDispatcher)
		: GraphAllocator(InGraphAllocator)
	{

	}

	void SetTimeLimit(bool bInUseTimeLimit, double InTimeLimit)
	{
		bUseTimeLimit = bInUseTimeLimit;
		TimeLimit = InTimeLimit;
		StartTime = FPlatformTime::Seconds();
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
	FEventLoadNode2* CurrentlyExecutingEventNode = nullptr;
	TArray<FAsyncLoadingSyncLoadContext*> SyncLoadContextStack;
	TArray<FAsyncPackage2*> PackagesExcludedFromStateDependencyChecks;
	TSpscQueue<FAsyncLoadingSyncLoadContext*> SyncLoadContextsCreatedOnGameThread;
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
	Package_SetupDependencies,
	Package_ExportsSerialized,
	Package_NumPhases,

	ExportBundle_Process = 0,
	ExportBundle_PostLoad,
	ExportBundle_DeferredPostLoad,
	ExportBundle_NumPhases,
};

struct FAsyncPackageExportToBundleMapping
{
	uint64 ExportHash;
	int32 BundleIndex[FExportBundleEntry::ExportCommandType_Count];
};

struct FAsyncPackageData
{
	uint8* MemoryBuffer = nullptr;
	TArrayView<FExportObject> Exports;
	TArrayView<FAsyncPackage2*> ImportedAsyncPackages;
	TArrayView<FEventLoadNode2> ExportBundleNodes;
	TArrayView<const FSHAHash> ShaderMapHashes;
	TArrayView<FAsyncPackageExportToBundleMapping> ExportToBundleMappings;
	int32 ExportBundleCount = 0;
};

struct FAsyncPackageSerializationState
{
	FIoRequest IoRequest;
	const uint8* AllExportDataPtr = nullptr;
	const uint8* CurrentExportDataPtr = nullptr;

	void ReleaseIoRequest()
	{
		IoRequest.Release();
		AllExportDataPtr = nullptr;
		CurrentExportDataPtr = nullptr;
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

/**
* Structure containing intermediate data required for async loading of all exports of a package.
*/

struct FAsyncPackage2
{
	friend struct FScopedAsyncPackageEvent2;
	friend struct FAsyncPackageScope2;
	friend class FAsyncLoadingThread2;
	friend class FAsyncLoadEventQueue2;

	FAsyncPackage2(const FAsyncPackageDesc2& InDesc,
		FAsyncLoadingThread2& InAsyncLoadingThread,
		FAsyncLoadEventGraphAllocator& InGraphAllocator,
		const FAsyncLoadEventSpec* EventSpecs);
	virtual ~FAsyncPackage2();


	void AddRef()
	{
		++RefCount;
	}

	void ReleaseRef();

	void ClearImportedPackages();

	/**
	 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
	 */
	double GetLoadStartTime() const;

	void AddCompletionCallback(TUniquePtr<FLoadPackageAsyncDelegate>&& Callback);

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
	void AddRequestID(int32 Id);

	int32 ReferencerRequestId() const
	{
		return Desc.ReferencerRequestId;
	}

	/**
	* Cancel loading this package.
	*/
	void Cancel();

	void AddConstructedObject(UObject* Object, bool bSubObjectThatAlreadyExists)
	{
		if (bSubObjectThatAlreadyExists)
		{
			ConstructedObjects.AddUnique(Object);
		}
		else
		{
			ConstructedObjects.Add(Object);
		}
	}

	void ClearConstructedObjects();

	/** Returns the UPackage wrapped by this, if it is valid */
	UPackage* GetLoadedPackage();

	/** Class specific callback for initializing non-native objects */
	EAsyncPackageState::Type PostLoadInstances(FAsyncLoadingThreadState2& ThreadState);

	/** Creates GC clusters from loaded objects */
	EAsyncPackageState::Type CreateClusters(FAsyncLoadingThreadState2& ThreadState);

	void ImportPackagesRecursive(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore);
	void StartLoading(FIoBatch& IoBatch);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	void InitializeLinkerLoadState(const FLinkerInstancingContext* InstancingContext);
	void CreateLinker(const FLinkerInstancingContext* InstancingContext);
	void DetachLinker();
#endif

#if WITH_EDITOR
	void GetLoadedAssetsAndPackages(TSet<FWeakObjectPtr>& AssetList, TSet<UPackage*>& PackageList);
#endif

private:
	void ImportPackagesRecursiveInner(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore, const TArrayView<FPackageId>& ImportedPackageIds, const TArrayView<FName>& ImportedPackageNames, int32& ImportedPackageIndex);

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
		TOptional<int32> MetaDataIndex;
#endif
		bool bIsCurrentlyProcessingImports = false;

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
	/** Time load begun. This is NOT the time the load was requested in the case of pending requests.	*/
	double						LoadStartTime = 0.0;
	TAtomic<int32> RefCount{ 0 };
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
	EAsyncPackageLoadingState2	AsyncPackageLoadingState = EAsyncPackageLoadingState2::NewPackage;

	struct FAllDependenciesState
	{
		FAsyncPackage2* WaitingForPackage = nullptr;
		FAsyncPackage2* PackagesWaitingForThisHead = nullptr;
		FAsyncPackage2* PackagesWaitingForThisTail = nullptr;
		FAsyncPackage2* PrevLink = nullptr;
		FAsyncPackage2* NextLink = nullptr;
		uint32 LastTick = 0;
		bool bAllDone = false;
		bool bAnyNotDone = false;
		bool bVisitedMark = false;

		void UpdateTick(int32 CurrentTick)
		{
			if (LastTick != CurrentTick)
			{
				LastTick = CurrentTick;
				bAnyNotDone = false;
				bVisitedMark = false;
			}
		}

		static void AddToWaitList(FAllDependenciesState FAsyncPackage2::* StateMemberPtr, FAsyncPackage2* WaitListPackage, FAsyncPackage2* PackageToAdd)
		{
			check(WaitListPackage);
			check(PackageToAdd);
			FAllDependenciesState& WaitListPackageState = WaitListPackage->*StateMemberPtr;
			FAllDependenciesState& PackageToAddState = PackageToAdd->*StateMemberPtr;
			
			if (PackageToAddState.WaitingForPackage == WaitListPackage)
			{
				return;
			}
			if (PackageToAddState.WaitingForPackage)
			{
				PackageToAddState.RemoveFromWaitList(StateMemberPtr, PackageToAddState.WaitingForPackage, PackageToAdd);
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
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	FAllDependenciesState		AllDependenciesSetupState;
#endif
	FAllDependenciesState		AllDependenciesSerializedState;
	FAllDependenciesState		AllDependenciesFullyLoadedState;

	/** True if our load has failed */
	bool						bLoadHasFailed = false;
	/** True if this package was created by this async package */
	bool						bCreatedLinkerRoot = false;
	/** List of all request handles */
	TArray<int32, TInlineAllocator<2>> RequestIDs;
	/** List of ConstructedObjects = Exports + UPackage + ObjectsCreatedFromExports */
	TArray<UObject*> ConstructedObjects;
	TArray<FExternalReadCallback> ExternalReadDependencies;
	/** Call backs called when we finished loading this package											*/
	using FCompletionCallback = TUniquePtr<FLoadPackageAsyncDelegate>;
	TArray<FCompletionCallback, TInlineAllocator<2>> CompletionCallbacks;

public:

	FAsyncLoadingThread2& GetAsyncLoadingThread()
	{
		return AsyncLoadingThread;
	}

	FAsyncLoadEventGraphAllocator& GetGraphAllocator()
	{
		return GraphAllocator;
	}

	/** [EDL] Begin Event driven loader specific stuff */

	static EEventLoadNodeExecutionResult Event_ProcessExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);
	static EEventLoadNodeExecutionResult Event_ProcessPackageSummary(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_SetupDependencies(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_ExportsDone(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32);
	static EEventLoadNodeExecutionResult Event_PostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);
	static EEventLoadNodeExecutionResult Event_DeferredPostLoadExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 ExportBundleIndex);

	void EventDrivenCreateExport(const FAsyncPackageHeaderData& Header, const TArrayView<FExportObject>& Exports, int32 LocalExportIndex);
	bool EventDrivenSerializeExport(const FAsyncPackageHeaderData& Header, const TArrayView<FExportObject>& Exports, int32 LocalExportIndex, FExportArchive& Ar);

	UObject* EventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, const TArrayView<const FExportObject>& Exports, FPackageObjectIndex Index, bool bCheckSerialized);
	template<class T>
	T* CastEventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, const TArrayView<const FExportObject>& Exports, FPackageObjectIndex Index, bool bCheckSerialized)
	{
		UObject* Result = EventDrivenIndexToObject(Header, Exports, Index, bCheckSerialized);
		if (!Result)
		{
			return nullptr;
		}
		return CastChecked<T>(Result);
	}

	FEventLoadNode2& GetPackageNode(EEventLoadNode2 Phase);
	FEventLoadNode2& GetExportBundleNode(EEventLoadNode2 Phase, uint32 ExportBundleIndex);

	/** [EDL] End Event driven loader specific stuff */

	void CallCompletionCallbacks(EAsyncLoadingResult::Type LoadingResult);

private:
	void CreatePackageNodes(const FAsyncLoadEventSpec* EventSpecs);
	void CreateExportBundleNodes(const FAsyncLoadEventSpec* EventSpecs);
	void SetupSerializedArcs(const FAsyncPackageHeaderData& Header, const TArrayView<FEventLoadNode2>& ExportBundleNodes, const TArrayView<FAsyncPackage2*>& ImportedAsyncPackages);
	void SetupScriptDependencies();
	bool HaveAllDependenciesReachedStateDebug(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, TSet<FAsyncPackage2*>& VisitedPackages, EAsyncPackageLoadingState2 WaitForPackageState, bool bExcludeZenPackages);
	bool HaveAllDependenciesReachedState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32 CurrentTick, bool bExcludeZenPackages);
	void UpdateDependenciesStateRecursive(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32 CurrentTick, FAsyncPackage2* Root, bool bExcludeZenPackages);
	void WaitForAllDependenciesToReachState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32& CurrentTickVariable, bool bExcludeZenPackages, TFunctionRef<void(FAsyncPackage2*)> OnStateReached);
	void ConditionalBeginPostLoad(FAsyncLoadingThreadState2& ThreadState);
	void ConditionalFinishLoading(FAsyncLoadingThreadState2& ThreadState);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	void ConditionalProcessLinkerLoadPackageImportsAndExports(FAsyncLoadingThreadState2& ThreadState);
	EEventLoadNodeExecutionResult ProcessLinkerLoadPackageSummary(FAsyncLoadingThreadState2& ThreadState);
	bool ProcessLinkerLoadPackageImports(FAsyncLoadingThreadState2& ThreadState);
	bool ProcessLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState);
	EEventLoadNodeExecutionResult ProcessLinkerLoadPackageImportsAndExports(FAsyncLoadingThreadState2& ThreadState);
	EEventLoadNodeExecutionResult PostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState);
#endif

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
	EAsyncPackageState::Type ProcessExternalReads(EExternalReadAction Action);

	/**
	* Updates load percentage stat
	*/
	void UpdateLoadPercentage();

public:

	/** Serialization context for this package */
	FUObjectSerializeContext* GetSerializeContext();
};

struct FScopedAsyncPackageEvent2
{
	/** Current scope package */
	FAsyncPackage2* Package;
	/** Outer scope package */
	FAsyncPackage2* PreviousPackage;
#if WITH_EDITOR
	IAsyncPackageLoader* PreviousAsyncPackageLoader;
#endif

	FScopedAsyncPackageEvent2(FAsyncPackage2* InPackage);
	~FScopedAsyncPackageEvent2();
};

class FAsyncLoadingThread2 final
	: public FRunnable
	, public IAsyncPackageLoader
{
	friend struct FAsyncPackage2;
public:
	FAsyncLoadingThread2(FIoDispatcher& IoDispatcher, IAsyncPackageLoader* InUncookedPackageLoader);
	virtual ~FAsyncLoadingThread2();

private:
	/** Thread to run the worker FRunnable on */
	static constexpr int32 DefaultAsyncPackagesReserveCount = 512;
	FRunnableThread* Thread;
	TAtomic<bool> bStopRequested { false };
	TAtomic<bool> bSuspendRequested { false };
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
	/** [GAME THREAD] Game thread CompletedPackages list */
	TArray<FAsyncPackage2*> CompletedPackages;
#if WITH_EDITOR
	/** [GAME THREAD] Game thread LoadedAssets list */
	TSet<FWeakObjectPtr> LoadedAssets;
#endif
	/** [ASYNC/GAME THREAD] Packages to be deleted from async thread */
	TMpscQueue<FAsyncPackage2*> DeferredDeletePackages;
	
	struct FFailedPackageRequest
	{
		int32 RequestID = INDEX_NONE;
		FName PackageName;
		TUniquePtr<FLoadPackageAsyncDelegate> Callback;
	};
	TArray<FFailedPackageRequest> FailedPackageRequests;
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
	/** [ASYNC/GAME THREAD] While this is non-zero there's work left to do */
	TAtomic<int32> PackagesWithRemainingWorkCounter{ 0 };

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

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	uint32 ConditionalProcessLinkerLoadPackageExportsTick = 0;
#endif
	uint32 ConditionalBeginPostLoadTick = 0;
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
		return bSuspendRequested;
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

	void UpdatePackagePriority(FAsyncPackage2* Package, int32 NewPriority);

	FAsyncPackage2* FindOrInsertPackage(FAsyncPackageDesc2& InDesc, bool& bInserted, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate = TUniquePtr<FLoadPackageAsyncDelegate>());
	void QueueMissingPackage(FAsyncPackageDesc2& PackageDesc, TUniquePtr<FLoadPackageAsyncDelegate>&& LoadPackageAsyncDelegate);

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
	EAsyncPackageState::Type TickAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 FlushRequestID, bool& bDidSomething);

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

	virtual int32 LoadPackage(
		const FPackagePath& InPackagePath,
		FName InCustomName,
		FLoadPackageAsyncDelegate InCompletionDelegate,
		EPackageFlags InPackageFlags,
		int32 InPIEInstanceID,
		int32 InPackagePriority,
		const FLinkerInstancingContext* InstancingContext = nullptr) override;

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

	virtual void FlushLoading(int32 PackageId) override;

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
	void RemovePendingRequests(TArrayView<int32> RequestIDs)
	{
		FScopeLock Lock(&PendingRequestsCritical);
		for (int32 ID : RequestIDs)
		{
			PendingRequests.Remove(ID);
			TRACE_LOADTIME_END_REQUEST(ID);
		}
		if (PendingRequests.IsEmpty())
		{
			PendingRequests.Empty(DefaultAsyncPackagesReserveCount);
		}
	}

	void AddPendingCDOs(FAsyncPackage2* Package, TArray<UClass*, TInlineAllocator<8>>& Classes)
	{
		FEventLoadNode2& FirstBundleNode = Package->GetExportBundleNode(ExportBundle_Process, 0);
		FirstBundleNode.AddBarrier(Classes.Num());
		for (UClass* Class : Classes)
		{
			PendingCDOs.FindOrAdd(Class).Add(&FirstBundleNode);
		}
	}

private:
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	bool ShouldLoadPackageFromFileSystem(FName PackageNameToLoad, FPackagePath& OutPackagePath)
	{
		return !PackageNameToLoad.IsNone() &&
			FPackagePath::TryFromPackageName(PackageNameToLoad, OutPackagePath) &&
			FPackageName::DoesPackageExistEx(OutPackagePath, FPackageName::EPackageLocationFilter::FileSystem, false /* bMatchCaseOnDisk */, &OutPackagePath) != FPackageName::EPackageLocationFilter::None;
	}
#endif

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

	void OnLeakedPackageRename(UPackage* Package);

	void OnPreGarbageCollect();

	void CollectUnreachableObjects(TArrayView<FUObjectItem*> UnreachableObjectItems, FUnreachableObjects& OutUnreachableObjects);

	void RemoveUnreachableObjects(FUnreachableObjects& ObjectsToRemove);

	bool ProcessPendingCDOs()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ProcessPendingCDOs);

		bool bDidSomething = false;
		UClass* Class = nullptr;
		int32 MaxRequestId = -1;
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
				int32 RequestId = Node->ReferencerRequestId();
				if (RequestId > MaxRequestId)
				{
					MaxRequestId = RequestId;
					Class = CurrentClass;
				}
			}
		}

		if (ensure(Class))
		{
			TArray<FEventLoadNode2*> Nodes;
			PendingCDOs.RemoveAndCopyValue(Class, Nodes);

			UE_LOG(LogStreaming, Log,
				TEXT("ProcessPendingCDOs: Creating CDO for '%s' for request id %d, releasing %d nodes. %d CDOs remaining."),
				*Class->GetFullName(), MaxRequestId, Nodes.Num(), PendingCDOs.Num());
			PendingCDOsRecursiveStack.Push(Class);
			UObject* CDO = Class->GetDefaultObject(/*bCreateIfNeeded*/ true);
			verify(PendingCDOsRecursiveStack.Pop() == Class);

			ensureAlwaysMsgf(CDO, TEXT("Failed to create CDO for %s"), *Class->GetFullName());
			UE_LOG(LogStreaming, Verbose, TEXT("ProcessPendingCDOs: Created CDO for '%s'."), *Class->GetFullName());

			for (FEventLoadNode2* Node : Nodes)
			{
				Node->ReleaseBarrier();
			}

			bDidSomething = true;
		}
		else
		{
			for (TMap<UClass*, TArray<FEventLoadNode2*>>::TIterator It = PendingCDOs.CreateIterator(); It; ++It)
			{
				UClass* CurrentClass = It.Key();
				check(CurrentClass != nullptr);

				const TArray<FEventLoadNode2*>& Nodes = It.Value();
				UE_LOG(LogStreaming, Warning,
					TEXT("ProcessPendingCDOs: '%s' with %d nodes could not be processed from this stack."),
					*CurrentClass->GetFullName(), Nodes.Num());
			}
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
	EAsyncPackageState::Type ProcessLoadedPackagesFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething, int32 FlushRequestID = INDEX_NONE);

	void IncludePackageInSyncLoadContext(uint64 ContextId, FAsyncPackage2* Package);
	FAsyncLoadingSyncLoadContext* UpdateSyncLoadContext(FAsyncLoadingThreadState2& ThreadState);

	bool CreateAsyncPackagesFromQueue(FAsyncLoadingThreadState2& ThreadState);

	FAsyncPackage2* CreateAsyncPackage(const FAsyncPackageDesc2& Desc)
	{
		UE_ASYNC_PACKAGE_DEBUG(Desc);

		return new FAsyncPackage2(Desc, *this, GraphAllocator, EventSpecs.GetData());
	}

	void InitializeAsyncPackageFromPackageStore(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FAsyncPackage2* AsyncPackage, const FPackageStoreEntry& PackageStoreEntry, const TArrayView<FName>& ImportedPackageNames)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(InitializeAsyncPackageFromPackageStore);
		UE_ASYNC_PACKAGE_DEBUG(AsyncPackage->Desc);
		
		FAsyncPackageData& Data = AsyncPackage->Data;
		FAsyncPackageHeaderData& HeaderData = AsyncPackage->HeaderData;

		const int32 ExportCount = PackageStoreEntry.ExportInfo.ExportCount;
		const int32 ExportBundleCount = PackageStoreEntry.ExportInfo.ExportBundleCount;
		const uint64 ExportBundleHeadersMemSize = Align(sizeof(FExportBundleHeader) * ExportBundleCount, 8);
		const int32 ExportBundleEntriesCount = ExportCount * FExportBundleEntry::ExportCommandType_Count;
		const uint64 ExportBundleEntriesMemSize = Align(sizeof(FExportBundleEntry) * ExportBundleEntriesCount, 8);
		const int32 ImportedPackagesCount = PackageStoreEntry.ImportedPackageIds.Num();
		const uint64 ImportedPackageIdsMemSize = Align(sizeof(FPackageId) * ImportedPackagesCount, 8);
		const uint64 ImportedPackageNamesMemSize = Align(sizeof(FName) * ImportedPackageNames.Num(), 8);
#if WITH_EDITOR
		const int32 OptionalSegmentExportCount = PackageStoreEntry.OptionalSegmentExportInfo.ExportCount;
		const int32 OptionalSegmentExportBundleCount = PackageStoreEntry.OptionalSegmentExportInfo.ExportBundleCount;
		const uint64 OptionalSegmentExportBundleHeadersMemSize = Align(sizeof(FExportBundleHeader) * OptionalSegmentExportBundleCount, 8);
		const int32 OptionalSegmentExportBundleEntriesCount = OptionalSegmentExportCount * FExportBundleEntry::ExportCommandType_Count;
		const uint64 OptionalSegmentExportBundleEntriesMemSize = Align(sizeof(FExportBundleEntry) * OptionalSegmentExportCount * FExportBundleEntry::ExportCommandType_Count, 8);
		const int32 OptionalSegmentImportedPackagesCount = PackageStoreEntry.OptionalSegmentImportedPackageIds.Num();
		const uint64 OptionalSegmentImportedPackageIdsMemSize = Align(sizeof(FPackageId) * OptionalSegmentImportedPackagesCount, 8);

		const int32 TotalImportedPackagesCount = ImportedPackagesCount + OptionalSegmentImportedPackagesCount;
		const int32 TotalExportCount = ExportCount + OptionalSegmentExportCount;
		AsyncPackage->Data.ExportBundleCount = ExportBundleCount + OptionalSegmentExportBundleCount;
#else
		const int32 TotalImportedPackagesCount = ImportedPackagesCount;
		const int32 TotalExportCount = ExportCount;
		AsyncPackage->Data.ExportBundleCount = ExportBundleCount;
#endif
		const int32 ExportBundleNodeCount = AsyncPackage->Data.ExportBundleCount * EEventLoadNode2::ExportBundle_NumPhases;
		const int32 ShaderMapHashesCount = PackageStoreEntry.ShaderMapHashes.Num();

		const uint64 ImportedPackagesMemSize = Align(sizeof(FAsyncPackage2*) * TotalImportedPackagesCount, 8);
		const uint64 ExportsMemSize = Align(sizeof(FExportObject) * TotalExportCount, 8);
		const uint64 ExportBundleNodesMemSize = Align(sizeof(FEventLoadNode2) * ExportBundleNodeCount, 8);
		const uint64 ShaderMapHashesMemSize = Align(sizeof(FSHAHash) * ShaderMapHashesCount, 8);
		const uint64 ExportToBundleMappingMemSize = Align(sizeof(FAsyncPackageExportToBundleMapping) * TotalExportCount, 8);
		const uint64 MemoryBufferSize =
#if WITH_EDITOR
			OptionalSegmentExportBundleHeadersMemSize +
			OptionalSegmentExportBundleEntriesMemSize +
			OptionalSegmentImportedPackageIdsMemSize +
#endif
			ExportBundleHeadersMemSize +
			ExportBundleEntriesMemSize +
			ImportedPackageIdsMemSize +
			ImportedPackageNamesMemSize +
			ImportedPackagesMemSize +
			ExportsMemSize +
			ExportBundleNodesMemSize +
			ShaderMapHashesMemSize +
			ExportToBundleMappingMemSize;

		Data.MemoryBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(MemoryBufferSize));

		uint8* DataPtr = Data.MemoryBuffer;

		Data.Exports = MakeArrayView(reinterpret_cast<FExportObject*>(DataPtr), TotalExportCount);
		DataPtr += ExportsMemSize;
		Data.ExportBundleNodes = MakeArrayView(reinterpret_cast<FEventLoadNode2*>(DataPtr), ExportBundleNodeCount);
		DataPtr += ExportBundleNodesMemSize;
		Data.ShaderMapHashes = MakeArrayView(reinterpret_cast<const FSHAHash*>(DataPtr), ShaderMapHashesCount);
		FMemory::Memcpy((void*)Data.ShaderMapHashes.GetData(), PackageStoreEntry.ShaderMapHashes.GetData(), sizeof(FSHAHash) * ShaderMapHashesCount);
		DataPtr += ShaderMapHashesMemSize;
		Data.ImportedAsyncPackages = MakeArrayView(reinterpret_cast<FAsyncPackage2**>(DataPtr), 0);
		DataPtr += ImportedPackagesMemSize;
		Data.ExportToBundleMappings = MakeArrayView(reinterpret_cast<FAsyncPackageExportToBundleMapping*>(DataPtr), TotalExportCount);
		DataPtr += ExportToBundleMappingMemSize;

		HeaderData.ExportCount = ExportCount;
		HeaderData.ExportBundleHeaders = MakeArrayView(reinterpret_cast<FExportBundleHeader*>(DataPtr), ExportBundleCount);
		DataPtr += ExportBundleHeadersMemSize;
		HeaderData.ExportBundleEntries = MakeArrayView(reinterpret_cast<FExportBundleEntry*>(DataPtr), ExportBundleEntriesCount);
		DataPtr += ExportBundleEntriesMemSize;
		HeaderData.ImportedPackageIds = MakeArrayView(reinterpret_cast<FPackageId*>(DataPtr), ImportedPackagesCount);
		FMemory::Memcpy((void*)HeaderData.ImportedPackageIds.GetData(), PackageStoreEntry.ImportedPackageIds.GetData(), sizeof(FPackageId) * ImportedPackagesCount);
		DataPtr += ImportedPackageIdsMemSize;
		if (ImportedPackageNamesMemSize)
		{
			HeaderData.ImportedPackageNames = MakeArrayView(reinterpret_cast<FName*>(DataPtr), ImportedPackageNames.Num());
			FMemory::Memcpy((void*)HeaderData.ImportedPackageNames.GetData(), ImportedPackageNames.GetData(), sizeof(FName) * ImportedPackageNames.Num());
			DataPtr += ImportedPackageNamesMemSize;
		}
#if WITH_EDITOR
		if (OptionalSegmentExportCount)
		{
			AsyncPackage->OptionalSegmentSerializationState.Emplace();
			FAsyncPackageHeaderData& OptionalSegmentHeaderData = AsyncPackage->OptionalSegmentHeaderData.Emplace();
			OptionalSegmentHeaderData.ExportCount = OptionalSegmentExportCount;
			OptionalSegmentHeaderData.ExportBundleHeaders = MakeArrayView(reinterpret_cast<FExportBundleHeader*>(DataPtr), OptionalSegmentExportBundleCount);
			DataPtr += OptionalSegmentExportBundleHeadersMemSize;
			OptionalSegmentHeaderData.ExportBundleEntries = MakeArrayView(reinterpret_cast<FExportBundleEntry*>(DataPtr), OptionalSegmentExportBundleEntriesCount);
			DataPtr += OptionalSegmentExportBundleEntriesMemSize;
			OptionalSegmentHeaderData.ImportedPackageIds = MakeArrayView(reinterpret_cast<FPackageId*>(DataPtr), OptionalSegmentImportedPackagesCount);
			FMemory::Memcpy((void*)OptionalSegmentHeaderData.ImportedPackageIds.GetData(), PackageStoreEntry.OptionalSegmentImportedPackageIds.GetData(), sizeof(FPackageId) * OptionalSegmentImportedPackagesCount);
			DataPtr += OptionalSegmentImportedPackageIdsMemSize;
		}
#endif
		check(DataPtr - Data.MemoryBuffer == MemoryBufferSize);
		AsyncPackage->CreateExportBundleNodes(EventSpecs.GetData());

		AsyncPackage->ConstructedObjects.Reserve(Data.Exports.Num() + 1); // +1 for UPackage, may grow dynamically beoynd that
		for (FExportObject& Export : Data.Exports)
		{
			Export = FExportObject();
		}

		AsyncPackage->ImportPackagesRecursive(ThreadState, IoBatch, PackageStore);
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
#if WITH_IOSTORE_IN_EDITOR
	IAsyncPackageLoader* PreviousAsyncPackageLoader;
#endif
	/** Cached ThreadContext so we don't have to access it again */
	FUObjectThreadContext& ThreadContext;

	FAsyncPackageScope2(FAsyncPackage2* InPackage)
		: ThreadContext(FUObjectThreadContext::Get())
	{
		PreviousPackage = ThreadContext.AsyncPackage;
		ThreadContext.AsyncPackage = InPackage;
#if WITH_IOSTORE_IN_EDITOR
		PreviousAsyncPackageLoader = ThreadContext.AsyncPackageLoader;
		ThreadContext.AsyncPackageLoader = &InPackage->AsyncLoadingThread;
#endif
	}
	~FAsyncPackageScope2()
	{
		ThreadContext.AsyncPackage = PreviousPackage;
#if WITH_IOSTORE_IN_EDITOR
		ThreadContext.AsyncPackageLoader = PreviousAsyncPackageLoader;
#endif
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

void FAsyncLoadingThread2::UpdatePackagePriority(FAsyncPackage2* Package, int32 NewPriority)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdatePackagePriority);
	Package->Desc.Priority = NewPriority;
	Package->SerializationState.IoRequest.UpdatePriority(NewPriority);
#if WITH_EDITOR
	if (Package->OptionalSegmentSerializationState.IsSet())
	{
		Package->OptionalSegmentSerializationState->IoRequest.UpdatePriority(NewPriority);
	}
#endif
}

FAsyncPackage2* FAsyncLoadingThread2::FindOrInsertPackage(FAsyncPackageDesc2& Desc, bool& bInserted, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindOrInsertPackage);
	FAsyncPackage2* Package = nullptr;
	bInserted = false;
	{
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
		Package = AsyncPackageLookup.FindRef(Desc.UPackageId);
		if (!Package)
		{
			Package = CreateAsyncPackage(Desc);
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
				Package->AddRequestID(Desc.RequestID);
			}
			if (Desc.Priority > Package->Desc.Priority)
			{
				UpdatePackagePriority(Package, Desc.Priority);
			}
			Package->Desc.ReferencerRequestId = Desc.ReferencerRequestId;
		}
		if (PackageLoadedDelegate.IsValid())
		{
			Package->AddCompletionCallback(MoveTemp(PackageLoadedDelegate));
		}
	}
	return Package;
}

void FAsyncLoadingThread2::IncludePackageInSyncLoadContext(uint64 ContextId, FAsyncPackage2* Package)
{
	if (Package->SyncLoadContextId == ContextId)
	{
		return;
	}

	if (Package->AsyncPackageLoadingState >= EAsyncPackageLoadingState2::DeferredPostLoadDone)
	{
		return;
	}

	Package->SyncLoadContextId = ContextId;
	for (FAsyncPackage2* ImportedPackage : Package->Data.ImportedAsyncPackages)
	{
		if (ImportedPackage && ImportedPackage->SyncLoadContextId != ContextId)
		{
			IncludePackageInSyncLoadContext(ContextId, ImportedPackage);
		}
	}
}

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
		EPackageStoreEntryStatus PendingPackageStatus = PackageStore.GetPackageStoreEntry(PendingPackage->Desc.PackageIdToLoad, PackageEntry);
		if (PendingPackageStatus == EPackageStoreEntryStatus::Ok)
		{
			InitializeAsyncPackageFromPackageStore(ThreadState, IoBatch, PendingPackage, PackageEntry, TArrayView<FName>());
			PendingPackage->StartLoading(IoBatch);
			It.RemoveCurrent();
		}
		else if (PendingPackageStatus == EPackageStoreEntryStatus::Missing)
		{
			// Initialize package with a fake package store entry
			FPackageStoreEntry FakePackageEntry;
			FakePackageEntry.ExportInfo.ExportCount = 1;
			FakePackageEntry.ExportInfo.ExportBundleCount = 1;
			InitializeAsyncPackageFromPackageStore(ThreadState, IoBatch, PendingPackage, FakePackageEntry, TArrayView<FName>());
			// Simulate StartLoading() getting back a failed IoRequest and let it go through all package states
			PendingPackage->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForIo;
			PendingPackage->bLoadHasFailed = true;
			PendingPackage->GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier();
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

			PackageStatus = PackageStore.GetPackageStoreEntry(PackageIdToLoad, PackageEntry);
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
					PackageStatus = PackageStore.GetPackageStoreEntry(PackageIdToLoad, PackageEntry);
				}
			}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
			bool bIsZenPackage = true;
			if (ShouldLoadPackageFromFileSystem(PackageNameToLoad, Request.PackagePath))
			{
				bIsZenPackage = false;
				PackageStatus = EPackageStoreEntryStatus::Ok;
			}
			if (bIsZenPackage && PackageStatus == EPackageStoreEntryStatus::Ok && !PackageEntry.UncookedPackageName.IsNone())
			{
				// This means that the package was added as an uncooked package to the store at one point and was later deleted from disk
				PackageStatus = EPackageStoreEntryStatus::Missing;
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
				QueueMissingPackage(PackageDesc, MoveTemp(Request.PackageLoadedDelegate));
			}
			else
			{
				bool bInserted;
				FAsyncPackage2* Package = FindOrInsertPackage(PackageDesc, bInserted, MoveTemp(Request.PackageLoadedDelegate));
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
						check(PackageStatus == EPackageStoreEntryStatus::Ok);
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
						if (!bIsZenPackage)
						{
							Package->InitializeLinkerLoadState(Request.GetInstancingContext());
						}
						else
#endif
						{
							InitializeAsyncPackageFromPackageStore(ThreadState, IoBatch, Package, PackageEntry, TArrayView<FName>());
						}
						Package->StartLoading(IoBatch);
					}
				}
				else
				{
					UE_ASYNC_PACKAGE_LOG_VERBOSE(Verbose, PackageDesc, TEXT("CreateAsyncPackages: UpdatePackage"),
						TEXT("Package is alreay being loaded."));
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
	: BarrierCount(InBarrierCount)
	, Spec(InSpec)
	, Package(InPackage)
	, ImportOrExportIndex(InImportOrExportIndex)
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
		Spec->EventQueue->Push(this);
	}
}

EEventLoadNodeExecutionResult FEventLoadNode2::Execute(FAsyncLoadingThreadState2& ThreadState)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteEvent);
	check(BarrierCount.load(std::memory_order_relaxed) == 0);
	EEventLoadNodeExecutionResult Result;
	{
		TGuardValue<FEventLoadNode2*> GuardCurrentlyExecutingEventNode(ThreadState.CurrentlyExecutingEventNode, this);
		Result = Spec->Func(ThreadState, Package, ImportOrExportIndex);
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
			ThreadState.NodesToFire.Pop(false)->Fire(&ThreadState);
		}
		ThreadState.bShouldFireNodes = true;
	}
}

int32 FEventLoadNode2::ReferencerRequestId() const
{
	return Package->ReferencerRequestId();
}

FAsyncLoadEventQueue2::FAsyncLoadEventQueue2()
{
}

FAsyncLoadEventQueue2::~FAsyncLoadEventQueue2()
{
}

void FAsyncLoadEventQueue2::Push(FEventLoadNode2* Node)
{
	// switch `Tail` to the new node and only then link the old tail to the new one. The list is not fully linked between these ops,
	// this is explicitly handled by the consumer by waiting for the link
	FEventLoadNode2* Prev = Tail.exchange(Node, std::memory_order_acq_rel); // sync with consumer in Deplete()
	check(Prev->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next
	Prev->Next.store(Node, std::memory_order_relaxed);
	if (Zenaphore)
	{
		bool bWasEmpty = Prev == &Sentinel;
		if (bWasEmpty)
		{
			Zenaphore->NotifyOne();
		}
	}
}

void FAsyncLoadEventQueue2::Deplete()
{
	FEventLoadNode2* First = Sentinel.Next.load(std::memory_order_relaxed);
	if (First == nullptr)
	{
		return; // empty
	}

	// reset the head so the next consumption can detect that the queue is empty
	// `Sentinel.Next` is not touched by producers right now because it's already not null
	Sentinel.Next.store(nullptr, std::memory_order_relaxed);

	// reset the queue to the empty state. this redirects producers to start from `Sentinel` again.
	// take note of the tail on resetting it because the list can be still not fully linked and so `Node.Next == nullptr` can't be 
	// used to detect the end of the list
	FEventLoadNode2* Last = Tail.exchange(&Sentinel, std::memory_order_acq_rel); // after setting Sentinel.Next to nullptr and before producers' tail modifications
	check(Last->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next
	// the previously queued items are detached from the instance (as a linked list, though potentially not fully linked yet)

	check(Last != &Sentinel); // can't be empty because of `First != nullptr` above

	if (LocalHead)
	{
		check(LocalTail);
		check(LocalTail->Next.load(std::memory_order_relaxed) == nullptr);
		LocalTail->Next.store(First, std::memory_order_relaxed);
	}
	else
	{
		LocalHead = First;
	}
	LocalTail = Last;
}

bool FAsyncLoadEventQueue2::PopAndExecute(FAsyncLoadingThreadState2& ThreadState)
{
	check(!bIsInPopAndExecute);
	TGuardValue<bool> GuardIsInPopAndExecute(bIsInPopAndExecute, true);
	if (TimedOutEventNode)
	{
		EEventLoadNodeExecutionResult Result = TimedOutEventNode->Execute(ThreadState);
		if (Result != EEventLoadNodeExecutionResult::Timeout)
		{
			TimedOutEventNode = nullptr;
		}
		return true;
	}

	if (!LocalHead)
	{
		Deplete();
	}
	if (LocalHead)
	{
		FEventLoadNode2* Next = nullptr;
		if (LocalHead != LocalTail)
		{
			// producers can still be updating `Next`, wait until the link to the next element is established
			do
			{
				Next = LocalHead->Next.load(std::memory_order_relaxed);
			} while (Next == nullptr);
		}
		FEventLoadNode2* Node = LocalHead;
		LocalHead = Next;
		if (!LocalHead)
		{
			LocalTail = nullptr;
		}

		EEventLoadNodeExecutionResult Result = Node->Execute(ThreadState);
		if (Result == EEventLoadNodeExecutionResult::Timeout)
		{
			TimedOutEventNode = Node;
		}
		return true;
	}
	return false;
}

bool FAsyncLoadEventQueue2::ExecuteSyncLoadEvents(FAsyncLoadingThreadState2& ThreadState, FAsyncLoadingSyncLoadContext& SyncLoadContext)
{
	int32 ThisCallCounter = ++ExecuteSyncLoadEventsCallCounter;

	auto ShouldExecuteNode = [&SyncLoadContext](FEventLoadNode2* Node) -> bool
	{
		return Node->Package->SyncLoadContextId == SyncLoadContext.ContextId;
	};

	bool bDidSomething = false;
	if (TimedOutEventNode && ShouldExecuteNode(TimedOutEventNode))
	{
		EEventLoadNodeExecutionResult Result = TimedOutEventNode->Execute(ThreadState);
		check(Result == EEventLoadNodeExecutionResult::Complete); // we can't timeout during a sync load operation
		TimedOutEventNode = nullptr;
		bDidSomething = true;
	}

	Deplete();

	FEventLoadNode2* Node = LocalHead;
	FEventLoadNode2* Prev = nullptr;
	while (Node)
	{
		FEventLoadNode2* Next = nullptr;
		if (Node != LocalTail)
		{
			// producers can still be updating `Next`, wait until the link to the next element is established
			do
			{
				Next = Node->Next.load(std::memory_order_relaxed);
			} while (Next == nullptr);
		}

		if (ShouldExecuteNode(Node))
		{
			if (Prev)
			{
				Prev->Next.store(Next, std::memory_order_relaxed);
			}
			if (Node == LocalHead)
			{
				LocalHead = Next;
				if (!LocalHead)
				{
					LocalTail = nullptr;
				}
			}
			if (Node == LocalTail)
			{
				LocalTail = Prev;
			}

			EEventLoadNodeExecutionResult Result = Node->Execute(ThreadState);
			check(Result == EEventLoadNodeExecutionResult::Complete); // we can't timeout during a sync load operation
			if (ExecuteSyncLoadEventsCallCounter != ThisCallCounter)
			{
				// ExecuteSyncLoadEvents was called recursively and our view of the list might have been compromised, start over
				return true;
			}
			bDidSomething = true;
		}
		else
		{
			Prev = Node;
		}
		Node = Next;
	}
	if (!bDidSomething && ThreadState.bIsAsyncLoadingThread)
	{
		return PopAndExecute(ThreadState);
	}
	return bDidSomething;
}

FScopedAsyncPackageEvent2::FScopedAsyncPackageEvent2(FAsyncPackage2* InPackage)
	:Package(InPackage)
{
	check(Package);

	// Update the thread context with the current package. This is used by NotifyConstructedDuringAsyncLoading.
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	PreviousPackage = static_cast<FAsyncPackage2*>(ThreadContext.AsyncPackage);
	ThreadContext.AsyncPackage = Package;
#if WITH_IOSTORE_IN_EDITOR
	PreviousAsyncPackageLoader = ThreadContext.AsyncPackageLoader;
	ThreadContext.AsyncPackageLoader = &InPackage->AsyncLoadingThread;
#endif
	Package->BeginAsyncLoad();
}

FScopedAsyncPackageEvent2::~FScopedAsyncPackageEvent2()
{
	Package->EndAsyncLoad();

	// Restore the package from the outer scope
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	ThreadContext.AsyncPackage = PreviousPackage;
#if WITH_IOSTORE_IN_EDITOR
	ThreadContext.AsyncPackageLoader = PreviousAsyncPackageLoader;
#endif
}

FUObjectSerializeContext* FAsyncPackage2::GetSerializeContext()
{
	return FUObjectThreadContext::Get().GetSerializeContext();
}

void FAsyncPackage2::SetupSerializedArcs(const FAsyncPackageHeaderData& Header, const TArrayView<FEventLoadNode2>& ExportBundleNodes, const TArrayView<FAsyncPackage2*>& ImportedAsyncPackages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SetupExternalArcs);

	FSimpleArchive ArcsArchive(Header.ArcsData.GetData(), Header.ArcsData.Num());
	int32 InternalArcsCount;
	ArcsArchive << InternalArcsCount;
	for (int32 InternalArcIndex = 0; InternalArcIndex < InternalArcsCount; ++InternalArcIndex)
	{
		int32 FromExportBundleIndex;
		ArcsArchive << FromExportBundleIndex;
		int32 ToExportBundleIndex;
		ArcsArchive << ToExportBundleIndex;
		uint32 FromNodeIndexBase = FromExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases;
		uint32 ToNodeIndexBase = ToExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases;
		for (int32 Phase = 0; Phase < EEventLoadNode2::ExportBundle_NumPhases; ++Phase)
		{
			uint32 ToNodeIndex = ToNodeIndexBase + Phase;
			uint32 FromNodeIndex = FromNodeIndexBase + Phase;
			ExportBundleNodes[ToNodeIndex].DependsOn(&ExportBundleNodes[FromNodeIndex]);
		}
	}
	for (const FAsyncPackage2* ImportedPackage : ImportedAsyncPackages)
	{
		int32 ExternalArcCount;
		ArcsArchive << ExternalArcCount;

		int32 PreviousFromExportBundleIndex = -1;
		int32 PreviousToExportBundleIndex = -1;

		for (int32 ExternalArcIndex = 0; ExternalArcIndex < ExternalArcCount; ++ExternalArcIndex)
		{
			int32 FromImportIndex;
			uint8 FromCommandType;
			int32 ToExportBundleIndex;
			ArcsArchive << FromImportIndex;
			ArcsArchive << FromCommandType;
			ArcsArchive << ToExportBundleIndex;
			if (ImportedPackage)
			{
				check(FromImportIndex < Header.ImportMap.Num());
				check(FromCommandType < FExportBundleEntry::ExportCommandType_Count);
				check(ToExportBundleIndex < ExportBundleNodes.Num());
				
				FPackageObjectIndex GlobalImportIndex = Header.ImportMap[FromImportIndex];
				FPackageImportReference PackageImportRef = GlobalImportIndex.ToPackageImportRef();
				const uint64 ImportedPublicExportHash = Header.ImportedPublicExportHashes[PackageImportRef.GetImportedPublicExportHashIndex()];
				for (const FAsyncPackageExportToBundleMapping& ExportToBundleMapping : ImportedPackage->Data.ExportToBundleMappings)
				{
					if (ExportToBundleMapping.ExportHash == ImportedPublicExportHash)
					{
						int32 FromExportBundleIndex = ExportToBundleMapping.BundleIndex[FromCommandType];
						if (PreviousFromExportBundleIndex != FromExportBundleIndex || PreviousToExportBundleIndex != ToExportBundleIndex)
						{
							PreviousFromExportBundleIndex = FromExportBundleIndex;
							PreviousToExportBundleIndex = ToExportBundleIndex;
							uint32 FromNodeIndexBase = FromExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases;
							uint32 ToNodeIndexBase = ToExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases;
							for (int32 Phase = 0; Phase < EEventLoadNode2::ExportBundle_NumPhases; ++Phase)
							{
								uint32 ToNodeIndex = ToNodeIndexBase + Phase;
								uint32 FromNodeIndex = FromNodeIndexBase + Phase;
								ExportBundleNodes[ToNodeIndex].DependsOn(&ImportedPackage->Data.ExportBundleNodes[FromNodeIndex]);
							}
						}
						break;
					}
				}
			}
		}
	}
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


void FGlobalImportStore::RegistrationComplete()
{
#if DO_CHECK
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindAllScriptObjectsDebug);
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
				ScriptObjects.Add(PackageGlobalImportIndex, Package);
				UE_LOG(LogStreaming, Display, TEXT("Script package %s (0x%016llX) is missing a NotifyRegistrationEvent from the initial load phase."),
					*Package->GetFullName(),
					PackageGlobalImportIndex.Value());
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
						ScriptObjects.Add(GlobalImportIndex, Object);
						ensureMsgf(false, TEXT("Script object %s (0x%016llX) is missing a NotifyRegistrationEvent from the initial load phase."),
							*Object->GetFullName(),
							GlobalImportIndex.Value());
					}
				}
			}
		}
	}
#endif
	ScriptObjects.Shrink();
}

void FAsyncPackage2::ImportPackagesRecursiveInner(FAsyncLoadingThreadState2& ThreadState, FIoBatch& IoBatch, FPackageStore& PackageStore, const TArrayView<FPackageId>& ImportedPackageIds, const TArrayView<FName>& ImportedPackageNames, int32& ImportedPackageIndex)
{
	const int32 ImportedPackageCount = ImportedPackageIds.Num();
	if (!ImportedPackageCount)
	{
		return;
	}
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
#if WITH_EDITORONLY_DATA && ALT2_ENABLE_LINKERLOAD_SUPPORT
		if (bHasImportedPackageNames && LinkerLoadState.IsSet())
		{
			// Use the instancing context remap from disk package name to upackage name
			const FLinkerInstancingContext& InstancingContext = LinkerLoadState->Linker->GetInstancingContext();
			ImportedPackageUPackageName = InstancingContext.RemapPackage(ImportedPackageNameToLoad);
			if (ImportedPackageUPackageName != ImportedPackageNameToLoad)
			{
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
		ImportedPackageStatus = PackageStore.GetPackageStoreEntry(ImportedPackageIdToLoad, ImportedPackageEntry);
#if WITH_EDITOR
		if (ImportedPackageStatus == EPackageStoreEntryStatus::Ok && !ImportedPackageEntry.UncookedPackageName.IsNone())
		{
			ImportedPackageNameToLoad = ImportedPackageEntry.UncookedPackageName;
			if (ImportedPackageUPackageName.IsNone())
			{
				ImportedPackageUPackageName = ImportedPackageNameToLoad;
			}
		}
#endif

		ImportStore.AddImportedPackageReference(ImportedPackageId, ImportedPackageUPackageName);

		FPackagePath ImportedPackagePath;
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
		bool bIsZenPackage = !LinkerLoadState.IsSet();
		bool bIsZenPackageImport = true;
		if (AsyncLoadingThread.ShouldLoadPackageFromFileSystem(ImportedPackageNameToLoad, ImportedPackagePath))
		{
			bIsZenPackageImport = false;
			ImportedPackageStatus = EPackageStoreEntryStatus::Ok;
		}
		if (bIsZenPackageImport && ImportedPackageStatus == EPackageStoreEntryStatus::Ok && !ImportedPackageEntry.UncookedPackageName.IsNone())
		{
			// This means that the package was added as an uncooked package to the store at one point and was later deleted from disk
			ImportedPackageStatus = EPackageStoreEntryStatus::Missing;
		}
#else
		constexpr bool bIsZenPackage = true;
		constexpr bool bIsZenPackageImport = true;
#endif

		FLoadedPackageRef& ImportedPackageRef = AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(ImportedPackageId);
#if WITH_EDITOR
		if (AsyncLoadingThread.UncookedPackageLoader && ImportedPackageStatus == EPackageStoreEntryStatus::Ok && !ImportedPackageEntry.UncookedPackageName.IsNone())
		{
			UPackage* UncookedPackage = ImportedPackageRef.GetPackage();
			if (!ImportedPackageRef.AreAllPublicExportsLoaded())
			{
				UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ImportPackages: LoadUncookedImport"), TEXT("Loading imported uncooked package '%s' '0x%llX'"), *ImportedPackageEntry.UncookedPackageName.ToString(), ImportedPackageId.ValueForDebugging());
				check(IsInGameThread());
				IoBatch.Issue(); // The batch might already contain requests for packages being imported from the uncooked one we're going to load so make sure that those are started before blocking
				ImportedPackagePath = FPackagePath::FromPackageNameUnchecked(ImportedPackageEntry.UncookedPackageName);
				ImportedPackagePath.SetHeaderExtension(static_cast<EPackageExtension>(ImportedPackageEntry.UncookedPackageHeaderExtension));
				int32 ImportRequestId = AsyncLoadingThread.UncookedPackageLoader->LoadPackage(ImportedPackagePath, NAME_None, FLoadPackageAsyncDelegate(), PKG_None, INDEX_NONE, 0, nullptr);
				AsyncLoadingThread.UncookedPackageLoader->FlushLoading(ImportRequestId);
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
					TEXT("Failed to load uncooked imported package with id '0x%llX' ('%s')"), ImportedPackageId.Value(), *ImportedPackageEntry.UncookedPackageName.ToString());
			}
			Data.ImportedAsyncPackages[ImportedPackageIndex++] = nullptr;
			continue;
		}
#endif

		FAsyncPackage2* ImportedPackage = nullptr;
		bool bInserted = false;
		if (ImportedPackageRef.AreAllPublicExportsLoaded())
		{
			ImportedPackage = AsyncLoadingThread.FindAsyncPackage(ImportedPackageId);
			if (!ImportedPackage)
			{
				Data.ImportedAsyncPackages[ImportedPackageIndex++] = nullptr;
				continue;
			}
			bInserted = false;
		}
		else if (ImportedPackageStatus == EPackageStoreEntryStatus::Missing)
		{
			if (!ImportedPackageRef.GetPackage()) // If we found a package it's not actually missing but we can't load it anyway
			{
				UE_ASYNC_PACKAGE_LOG(Warning, Desc, TEXT("ImportPackages: SkipPackage"),
					TEXT("Skipping non mounted imported package with id '0x%llX'"), ImportedPackageId.Value());
				ImportedPackageRef.SetIsMissingPackage();
			}
			Data.ImportedAsyncPackages[ImportedPackageIndex++] = nullptr;
			continue;
		}
		else
		{
			FAsyncPackageDesc2 PackageDesc = FAsyncPackageDesc2::FromPackageImport(Desc, ImportedPackageUPackageName, ImportedPackageId, ImportedPackageIdToLoad, MoveTemp(ImportedPackagePath));
			ImportedPackage = AsyncLoadingThread.FindOrInsertPackage(PackageDesc, bInserted);
		}

		checkf(ImportedPackage, TEXT("Failed to find or insert imported package with id '0x%llX'"), ImportedPackageId.Value());
		TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(this, ImportedPackage);

		if (bInserted)
		{
			UE_ASYNC_PACKAGE_LOG(Verbose, Desc, TEXT("ImportPackages: AddPackage"),
			TEXT("Start loading imported package with id '0x%llX'"), ImportedPackageId.ValueForDebugging());
			++AsyncLoadingThread.PackagesWithRemainingWorkCounter;
			TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, AsyncLoadingThread.PackagesWithRemainingWorkCounter);
		}
		else
		{
			UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("ImportPackages: UpdatePackage"),
				TEXT("Imported package with id '0x%llX' is already being loaded."), ImportedPackageId.ValueForDebugging());
		}
		ImportedPackage->AddRef();
		Data.ImportedAsyncPackages[ImportedPackageIndex++] = ImportedPackage;

		if (bIsZenPackage != bIsZenPackageImport)
		{
			// When importing a linker load package from a zen package or vice versa we need to wait for all the exports in the imported package to be created
			// and serialized before we can start processing our own exports
			GetPackageNode(Package_SetupDependencies).DependsOn(&ImportedPackage->GetPackageNode(Package_ExportsSerialized));
		}
		else if (bIsZenPackage)
		{
			// When importing a zen package from another zen package we need the summary of the imported package to be ready before we can hook up our export
			// bundle node dependenices to the imported package
			GetPackageNode(Package_SetupDependencies).DependsOn(&ImportedPackage->GetPackageNode(Package_ProcessSummary));
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
					ImportedPackage->InitializeLinkerLoadState(bIsZenPackage ? nullptr : &LinkerLoadState->Linker->GetInstancingContext());
				}
				else
#endif
				{
					AsyncLoadingThread.InitializeAsyncPackageFromPackageStore(ThreadState, IoBatch, ImportedPackage, ImportedPackageEntry, TArrayView<FName>());
				}
				ImportedPackage->StartLoading(IoBatch);
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

	int32 ImportedPackageCount = HeaderData.ImportedPackageIds.Num();
#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		ImportedPackageCount += OptionalSegmentHeaderData->ImportedPackageIds.Num();
	}
#endif
	if (!ImportedPackageCount)
	{
		return;
	}

	int32 ImportedPackageIndex = 0;

	Data.ImportedAsyncPackages = MakeArrayView(Data.ImportedAsyncPackages.GetData(), ImportedPackageCount);
	
	ImportPackagesRecursiveInner(ThreadState, IoBatch, PackageStore, HeaderData.ImportedPackageIds, HeaderData.ImportedPackageNames, ImportedPackageIndex);
#if WITH_EDITOR
	if (OptionalSegmentHeaderData.IsSet())
	{
		ImportPackagesRecursiveInner(ThreadState, IoBatch, PackageStore, OptionalSegmentHeaderData->ImportedPackageIds, OptionalSegmentHeaderData->ImportedPackageNames, ImportedPackageIndex);
	}
#endif

	if (SyncLoadContextId)
	{
		for (FAsyncPackage2* ImportedPackage : Data.ImportedAsyncPackages)
		{
			if (ImportedPackage)
			{
				AsyncLoadingThread.IncludePackageInSyncLoadContext(SyncLoadContextId, ImportedPackage);
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
	FLinkerLoad* Linker = FLinkerLoad::FindExistingLinkerForPackage(LinkerRoot);
	if (!Linker)
	{
		uint32 LinkerFlags = (LOAD_Async | LOAD_NoVerify | LOAD_SkipLoadImportedPackages);
		if ((!FApp::IsGame() || GIsEditor) && (Desc.PackageFlags & PKG_PlayInEditor) != 0)
		{
			LinkerFlags |= LOAD_PackageForPIE;
		}
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
	check(Linker);
	check(Linker->LinkerRoot == LinkerRoot);
	LinkerLoadState->Linker = Linker;
#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
	Linker->ResetStatusInfo();
#endif
}

void FAsyncPackage2::DetachLinker()
{
	if (LinkerLoadState.IsSet() && LinkerLoadState->Linker)
	{
		LinkerLoadState->Linker = nullptr;
	}
}
#endif

void FAsyncPackage2::StartLoading(FIoBatch& IoBatch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StartLoading);
	TRACE_LOADTIME_BEGIN_LOAD_ASYNC_PACKAGE(this);

	LoadStartTime = FPlatformTime::Seconds();

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForIo;

	FIoReadOptions ReadOptions;
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (LinkerLoadState.IsSet())
	{
#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
		static_cast<FLinkerLoadArchive2*>(LinkerLoadState->Linker->GetLoader())->BeginRead(&GetPackageNode(EEventLoadNode2::Package_ProcessSummary));
#else
		GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier();
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
				GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier();
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
			GetPackageNode(EEventLoadNode2::Package_ProcessSummary).ReleaseBarrier();
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
					GetPackageNode(Package_ExportsSerialized).ReleaseBarrier();
					if (LocalPendingIoRequestsCounter == 0)
					{
						LocalAsyncLoadingThread.AltZenaphore.NotifyOne();
					}
				});
		};
		FCoreDelegates::PreloadPackageShaderMaps.ExecuteIfBound(Data.ShaderMapHashes, ReadShaderMapFunc);
	}
}

static void ReadAsyncPackageHeader(FAsyncPackageSerializationState& SerializationState, FAsyncPackageHeaderData& HeaderData)
{
	const uint8* PackageHeaderDataPtr = SerializationState.IoRequest.GetResultOrDie().Data();
	const FZenPackageSummary* PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageHeaderDataPtr);
	HeaderData.PackageSummary = PackageSummary;

	TArrayView<const uint8> PackageHeaderDataView(PackageHeaderDataPtr + sizeof(FZenPackageSummary), PackageSummary->HeaderSize - sizeof(FZenPackageSummary));
	FMemoryReaderView PackageHeaderDataReader(PackageHeaderDataView);
	if (PackageSummary->bHasVersioningInfo)
	{
		HeaderData.VersioningInfo.Emplace();
		PackageHeaderDataReader << HeaderData.VersioningInfo.GetValue();
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackageNameMap);
		HeaderData.NameMap.Load(PackageHeaderDataReader, FMappedName::EType::Package);
	}
	HeaderData.PackageName = HeaderData.NameMap.GetName(PackageSummary->Name);

	HeaderData.CookedHeaderSize = PackageSummary->CookedHeaderSize;
	HeaderData.ImportedPublicExportHashes = TArrayView<const uint64>(
		reinterpret_cast<const uint64*>(PackageHeaderDataPtr + PackageSummary->ImportedPublicExportHashesOffset),
		(PackageSummary->ImportMapOffset - PackageSummary->ImportedPublicExportHashesOffset) / sizeof(uint64));
	HeaderData.ImportMap = TArrayView<const FPackageObjectIndex>(
		reinterpret_cast<const FPackageObjectIndex*>(PackageHeaderDataPtr + PackageSummary->ImportMapOffset),
		(PackageSummary->ExportMapOffset - PackageSummary->ImportMapOffset) / sizeof(FPackageObjectIndex));
	HeaderData.ExportMap = TArrayView<const FExportMapEntry>(
		reinterpret_cast<const FExportMapEntry*>(PackageHeaderDataPtr + PackageSummary->ExportMapOffset),
		(PackageSummary->ExportBundleEntriesOffset - PackageSummary->ExportMapOffset) / sizeof(FExportMapEntry));
	check(HeaderData.ExportMap.Num() == HeaderData.ExportCount);

	const uint64 ExportBundleHeadersOffset = PackageSummary->GraphDataOffset;
	const uint64 ExportBundleHeadersSize = sizeof(FExportBundleHeader) * HeaderData.ExportBundleHeaders.Num();
	const uint64 ExportBundleEntriesSize = sizeof(FExportBundleEntry) * HeaderData.ExportBundleEntries.Num();
	const uint64 ArcsDataOffset = ExportBundleHeadersOffset + ExportBundleHeadersSize;
	const uint64 ArcsDataSize = PackageSummary->HeaderSize - ArcsDataOffset;
	check(ExportBundleEntriesSize == PackageSummary->GraphDataOffset - PackageSummary->ExportBundleEntriesOffset);
	HeaderData.ArcsData = MakeArrayView(PackageHeaderDataPtr + ArcsDataOffset, ArcsDataSize);
	FMemory::Memcpy(HeaderData.ExportBundleHeaders.GetData(), PackageHeaderDataPtr + ExportBundleHeadersOffset, ExportBundleHeadersSize);
	FMemory::Memcpy(HeaderData.ExportBundleEntries.GetData(), PackageHeaderDataPtr + PackageSummary->ExportBundleEntriesOffset, ExportBundleEntriesSize);

	SerializationState.AllExportDataPtr = PackageHeaderDataPtr + PackageSummary->HeaderSize;
}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
void FAsyncPackage2::ConditionalProcessLinkerLoadPackageImportsAndExports(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalProcessLinkerLoadPackageExports);
	constexpr bool bExcludeZenPackages = true;
	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesSetupState, EAsyncPackageLoadingState2::ProcessExportBundles, AsyncLoadingThread.ConditionalProcessLinkerLoadPackageExportsTick, bExcludeZenPackages,
		[](FAsyncPackage2* Package)
		{
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;
			Package->GetExportBundleNode(ExportBundle_Process, 0).ReleaseBarrier();
		});
}

EEventLoadNodeExecutionResult FAsyncPackage2::ProcessLinkerLoadPackageSummary(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessLinkerLoadPackageSummary);

#if ALT2_ENABLE_NEW_ARCHIVE_FOR_LINKERLOAD
	if (LinkerLoadState->Linker->GetLoader()->IsError())
	{
		bLoadHasFailed = true;
		return EEventLoadNodeExecutionResult::Complete;
	}
	LinkerLoadState->Linker->bUseTimeLimit = false;
	FLinkerLoad::ELinkerStatus LinkerResult = LinkerLoadState->Linker->ProcessPackageSummary(nullptr);
#else
	FLinkerLoad::ELinkerStatus LinkerResult = LinkerLoadState->Linker->Tick(/* RemainingTimeLimit */ 0.0, /* bUseTimeLimit */ false, /* bUseFullTimeLimit */ false, nullptr);
#endif
	check(LinkerResult != FLinkerLoad::LINKER_TimedOut); // TODO: Add support for timeouts here
	if (LinkerResult == FLinkerLoad::LINKER_Failed)
	{
		bLoadHasFailed = true;
		return EEventLoadNodeExecutionResult::Complete;
	}
	check(LinkerLoadState->Linker->HasFinishedInitialization());

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
				NameLen = LinkerLoadState->Linker->ImportMap[PathComponents.Pop(false)].ObjectName.ToString(NameStr);
				FPathViews::Append(FullPath, FStringView(NameStr, NameLen));
				LinkerLoadState->LinkerLoadHeaderData.ImportMap[ImportIndex] = FPackageObjectIndex::FromScriptPath(FullPath);
			}
		}
		else
		{
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
					PathComponents.Pop(false);
				}
				TStringBuilder<256> PackageRelativeExportPath;
				while (!PathComponents.IsEmpty())
				{
					NameLen = LinkerLoadState->Linker->ImportMap[PathComponents.Pop(false)].ObjectName.ToString(NameStr);
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

	LinkerLoadState->LinkerLoadHeaderData.PublicExportHashes.SetNum(LinkerLoadState->Linker->ExportMap.Num());
	for (int32 ExportIndex = 0; ExportIndex < LinkerLoadState->Linker->ExportMap.Num(); ++ExportIndex)
	{
		FObjectExport& ObjectExport = LinkerLoadState->Linker->ExportMap[ExportIndex];
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
				uint32 NameLen = LinkerLoadState->Linker->ExportMap[FullPath.Pop(false)].ObjectName.ToString(NameStr);
				for (uint32 I = 0; I < NameLen; ++I)
				{
					NameStr[I] = TChar<TCHAR>::ToLower(NameStr[I]);
				}
				PackageRelativeExportPath.AppendChar('/');
				PackageRelativeExportPath.Append(FStringView(NameStr, NameLen));
			}
			uint64 PublicExportHash = CityHash64(reinterpret_cast<const char*>(PackageRelativeExportPath.GetData() + 1), (PackageRelativeExportPath.Len() - 1) * sizeof(TCHAR));
			LinkerLoadState->LinkerLoadHeaderData.PublicExportHashes[ExportIndex] = PublicExportHash;
		}
	}

	FPackageStoreEntry PackageStoreEntry;
	PackageStoreEntry.ExportInfo.ExportCount = LinkerLoadState->Linker->ExportMap.Num();
	PackageStoreEntry.ExportInfo.ExportBundleCount = 1;
	PackageStoreEntry.ImportedPackageIds = ImportedPackageIds;

	FIoBatch IoBatch = AsyncLoadingThread.IoDispatcher.NewBatch();
	{
		FPackageStoreReadScope _(AsyncLoadingThread.PackageStore);
		AsyncLoadingThread.InitializeAsyncPackageFromPackageStore(ThreadState, IoBatch, this, PackageStoreEntry, ImportedPackageNames);
	}
	IoBatch.Issue();

	HeaderData.ImportedPublicExportHashes = LinkerLoadState->LinkerLoadHeaderData.ImportedPublicExportHashes;
	HeaderData.ImportMap = LinkerLoadState->LinkerLoadHeaderData.ImportMap;

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::SetupDependencies;
	GetPackageNode(Package_SetupDependencies).ReleaseBarrier();
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::ProcessLinkerLoadPackageImportsAndExports(FAsyncLoadingThreadState2& ThreadState)
{
	ON_SCOPE_EXIT
	{
		FUObjectSerializeContext * LoadContext = GetSerializeContext();
		TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
		ThreadObjLoaded.Reset();
	};

	if (!ProcessLinkerLoadPackageImports(ThreadState))
	{
		return EEventLoadNodeExecutionResult::Timeout;
	}
	if (!ProcessLinkerLoadPackageExports(ThreadState))
	{
		return EEventLoadNodeExecutionResult::Timeout;
	}

	if (ExternalReadDependencies.Num() == 0)
	{
		AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;
		GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(&ThreadState);
	}
	else
	{
		AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForExternalReads;
		AsyncLoadingThread.ExternalReadQueue.Enqueue(this);
	}
	return EEventLoadNodeExecutionResult::Complete;
}

bool FAsyncPackage2::ProcessLinkerLoadPackageImports(FAsyncLoadingThreadState2& ThreadState)
{
	if (LinkerLoadState->bIsCurrentlyProcessingImports)
	{
		return true;
	}
	TGuardValue<bool> GuardIsCurrentlyProcessingImports(LinkerLoadState->bIsCurrentlyProcessingImports, true);
	const int32 ImportedPackagesCount = Data.ImportedAsyncPackages.Num();
	while (LinkerLoadState->ProcessingImportedPackageIndex < ImportedPackagesCount)
	{
		FAsyncPackage2* ImportedPackage = Data.ImportedAsyncPackages[LinkerLoadState->ProcessingImportedPackageIndex];
		if (ImportedPackage && ImportedPackage->LinkerLoadState.IsSet() && ImportedPackage->LinkerLoadState->Linker)
		{
			if (!ImportedPackage->ProcessLinkerLoadPackageImports(ThreadState))
			{
				return false;
			}
			if (!ImportedPackage->ProcessLinkerLoadPackageExports(ThreadState))
			{
				return false;
			}
		}
		++LinkerLoadState->ProcessingImportedPackageIndex;
	}

	const int32 ImportCount = LinkerLoadState->Linker->ImportMap.Num();
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
			if (!LinkerImport.XObject)
			{
				LinkerImport.XObject = FromImportStore;
			}
		}
	}
	return true;
}

bool FAsyncPackage2::ProcessLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState)
{
#if WITH_EDITORONLY_DATA
	// Create metadata object
	int32 MetaDataIndex = LinkerLoadState->MetaDataIndex.IsSet() ? LinkerLoadState->MetaDataIndex.GetValue() : -1;
	if (MetaDataIndex < 0)
	{
		MetaDataIndex = LinkerLoadState->Linker->LoadMetaDataFromExportMap(false);
		if (MetaDataIndex >= 0)
		{
			FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[MetaDataIndex];
			FExportObject& ExportObject = Data.Exports[MetaDataIndex];
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
		LinkerLoadState->MetaDataIndex = MetaDataIndex;
	}
#endif

	// Create exports
	const int32 ExportCount = LinkerLoadState->Linker->ExportMap.Num();
	while (LinkerLoadState->CreateExportIndex < ExportCount)
	{
		const int32 ExportIndex = LinkerLoadState->CreateExportIndex++;
#if WITH_EDITORONLY_DATA
		if (ExportIndex == MetaDataIndex)
		{
			continue;
		}
#endif
		FObjectExport& LinkerExport = LinkerLoadState->Linker->ExportMap[ExportIndex];
		FExportObject& ExportObject = Data.Exports[ExportIndex];
		if (UObject* Object = LinkerLoadState->Linker->CreateExport(ExportIndex))
		{
			ExportObject.Object = Object;
			ExportObject.bWasFoundInMemory = true; // Make sure that the async flags are cleared in ClearConstructedObjects
			EInternalObjectFlags FlagsToSet = EInternalObjectFlags::Async;
			uint64 PublicExportHash = LinkerLoadState->LinkerLoadHeaderData.PublicExportHashes[ExportIndex];
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

	// Serialize exports
	while (LinkerLoadState->SerializeExportIndex < ExportCount)
	{
		const int32 ExportIndex = LinkerLoadState->SerializeExportIndex++;
		FExportObject& ExportObject = Data.Exports[ExportIndex];
		if (UObject* Object = ExportObject.Object)
		{
			if (Object->HasAnyFlags(RF_NeedLoad))
			{
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

EEventLoadNodeExecutionResult FAsyncPackage2::PostLoadLinkerLoadPackageExports(FAsyncLoadingThreadState2& ThreadState)
{
	FAsyncLoadingTickScope2 InAsyncLoadingTick(AsyncLoadingThread);

	const int32 ExportCount = LinkerLoadState->Linker->ExportMap.Num();
	while (LinkerLoadState->PostLoadExportIndex < ExportCount)
	{
		const int32 ExportIndex = LinkerLoadState->PostLoadExportIndex++;
		const FObjectExport& Export = LinkerLoadState->Linker->ExportMap[ExportIndex];
		UObject* Object = Export.Object;
		if (Object)
		{
			Object->ConditionalPostLoad();
		}
	}

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
	ThreadObjLoaded.Empty();

	AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoadDone;
	ConditionalFinishLoading(ThreadState);
	return EEventLoadNodeExecutionResult::Complete;;
}
#endif

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ProcessPackageSummary(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ProcessPackageSummary);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForIo);
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessPackageSummary;

	FScopedAsyncPackageEvent2 Scope(Package);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		EEventLoadNodeExecutionResult Result = Package->ProcessLinkerLoadPackageSummary(ThreadState);
		if (!Package->bLoadHasFailed)
		{
			return Result;
		}
	}
#endif
	if (Package->bLoadHasFailed)
	{
		if (Package->Desc.bCanBeImported)
		{
			FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId);
			PackageRef.SetHasFailed();
		}
	}
	else
	{
		check(Package->ExportBundleEntryIndex == 0);

		ReadAsyncPackageHeader(Package->SerializationState, Package->HeaderData);
#if WITH_EDITOR
		FAsyncPackageHeaderData* OptionalSegmentHeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
		if (OptionalSegmentHeaderData)
		{
			ReadAsyncPackageHeader(*Package->OptionalSegmentSerializationState, *OptionalSegmentHeaderData);
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
			FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId);
			if (PublicExportsCount)
			{
				PackageRef.ReserveSpaceForPublicExports(PublicExportsCount);
			}
		}

		for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.ExportBundleCount; ++ExportBundleIndex)
		{
			const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
			int32 LocalExportBundleIndex = ExportBundleIndex;
			int32 ExportIndexOffset = 0;
#if WITH_EDITOR
			if (ExportBundleIndex >= Package->HeaderData.ExportBundleHeaders.Num())
			{
				check(OptionalSegmentHeaderData);
				HeaderData = OptionalSegmentHeaderData;
				LocalExportBundleIndex -= Package->HeaderData.ExportBundleHeaders.Num();
				ExportIndexOffset = Package->HeaderData.ExportMap.Num();
			}
#endif
			const FExportBundleHeader& ExportBundle = HeaderData->ExportBundleHeaders[LocalExportBundleIndex];
			for (int32 EntryIndex = ExportBundle.FirstEntryIndex, EntryEnd = ExportBundle.FirstEntryIndex + ExportBundle.EntryCount; EntryIndex < EntryEnd; ++EntryIndex)
			{
				const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntries[EntryIndex];
				const FExportMapEntry& ExportMapEntry = HeaderData->ExportMap[BundleEntry.LocalExportIndex];
				FAsyncPackageExportToBundleMapping& ExportToBundleMapping = Package->Data.ExportToBundleMappings[BundleEntry.LocalExportIndex + ExportIndexOffset];
				ExportToBundleMapping.ExportHash = ExportMapEntry.PublicExportHash;
				ExportToBundleMapping.BundleIndex[BundleEntry.CommandType] = ExportBundleIndex;
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

		TRACE_LOADTIME_PACKAGE_SUMMARY(Package, Package->HeaderData.PackageName, Package->HeaderData.PackageSummary->HeaderSize, Package->HeaderData.ImportMap.Num(), Package->HeaderData.ExportMap.Num());
	}

	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::SetupDependencies;
	Package->GetPackageNode(Package_SetupDependencies).ReleaseBarrier();
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_SetupDependencies(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_SetupDependencies);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::SetupDependencies);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		if (!Package->AsyncLoadingThread.bHasRegisteredAllScriptObjects)
		{
			Package->SetupScriptDependencies();
		}
		Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;
		Package->ConditionalProcessLinkerLoadPackageImportsAndExports(ThreadState);
		return EEventLoadNodeExecutionResult::Complete;
	}
#endif

	if (!Package->bLoadHasFailed)
	{
		if (!Package->AsyncLoadingThread.bHasRegisteredAllScriptObjects)
		{
			Package->SetupScriptDependencies();
		}
		TArrayView<FEventLoadNode2> ExportBundleNodesView = Package->Data.ExportBundleNodes;
		TArrayView<FAsyncPackage2*> ImportedAsyncPackagesView = Package->Data.ImportedAsyncPackages;
		Package->SetupSerializedArcs(Package->HeaderData, ExportBundleNodesView.Left(Package->HeaderData.ExportBundleHeaders.Num() * EEventLoadNode2::ExportBundle_NumPhases), ImportedAsyncPackagesView.Left(Package->HeaderData.ImportedPackageIds.Num()));
#if WITH_EDITOR
		if (Package->OptionalSegmentHeaderData.IsSet())
		{
			Package->SetupSerializedArcs(*Package->OptionalSegmentHeaderData, ExportBundleNodesView.Right(Package->OptionalSegmentHeaderData->ExportBundleHeaders.Num() * EEventLoadNode2::ExportBundle_NumPhases), ImportedAsyncPackagesView.Right(Package->OptionalSegmentHeaderData->ImportedPackageIds.Num()));
		}
#endif
	}
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ProcessExportBundles;
	for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.ExportBundleCount; ++ExportBundleIndex)
	{
		Package->GetExportBundleNode(ExportBundle_Process, ExportBundleIndex).ReleaseBarrier();
	}
	return EEventLoadNodeExecutionResult::Complete;
}

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ProcessExportBundle(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32 InExportBundleIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ProcessExportBundle);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles);

	FScopedAsyncPackageEvent2 Scope(Package);

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		return Package->ProcessLinkerLoadPackageImportsAndExports(ThreadState);
	}
#endif

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

	check(InExportBundleIndex < Package->Data.ExportBundleCount);
	
	if (!Package->bLoadHasFailed)
	{
#if WITH_EDITOR
		const FAsyncPackageHeaderData* HeaderData;
		const FExportBundleHeader* ExportBundle;
		FAsyncPackageSerializationState* SerializationState;
		TArrayView<FExportObject> Exports = Package->Data.Exports;
		if (InExportBundleIndex >= Package->HeaderData.ExportBundleHeaders.Num())
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
			ExportBundle = &HeaderData->ExportBundleHeaders[InExportBundleIndex - Package->HeaderData.ExportBundleHeaders.Num()];
			Exports.RightInline(HeaderData->ExportCount);
			SerializationState = Package->OptionalSegmentSerializationState.GetPtrOrNull();
			check(SerializationState);
		}
		else
		{
			HeaderData = &Package->HeaderData;
			ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
			SerializationState = &Package->SerializationState;
			Exports.LeftInline(Package->HeaderData.ExportCount);
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
		const FExportBundleHeader* ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
		FAsyncPackageSerializationState* SerializationState = &Package->SerializationState;
		const TArrayView<FExportObject>& Exports = Package->Data.Exports;
#endif
		const FIoBuffer& IoBuffer = SerializationState->IoRequest.GetResultOrDie();
		const uint64 AllExportDataSize = IoBuffer.DataSize() - (SerializationState->AllExportDataPtr - IoBuffer.Data());
		if (Package->ExportBundleEntryIndex == 0)
		{
			SerializationState->CurrentExportDataPtr = SerializationState->AllExportDataPtr + ExportBundle->SerialOffset;
		}
		FExportArchive Ar(SerializationState->AllExportDataPtr, SerializationState->CurrentExportDataPtr, AllExportDataSize);
		{
			Ar.SetUEVer(Package->LinkerRoot->GetLinkerPackageVersion());
			Ar.SetLicenseeUEVer(Package->LinkerRoot->GetLinkerLicenseeVersion());
			// Ar.SetEngineVer(Summary.SavedByEngineVersion); // very old versioning scheme
			if (!Package->LinkerRoot->GetLinkerCustomVersions().GetAllVersions().IsEmpty())
			{
				Ar.SetCustomVersions(Package->LinkerRoot->GetLinkerCustomVersions());
			}
			Ar.SetUseUnversionedPropertySerialization((Package->LinkerRoot->GetPackageFlags() & PKG_UnversionedProperties) != 0);
			Ar.SetIsLoadingFromCookedPackage((Package->LinkerRoot->GetPackageFlags() & PKG_Cooked) != 0);
			Ar.SetIsLoading(true);
			Ar.SetIsPersistent(true);
			if (Package->LinkerRoot->GetPackageFlags() & PKG_FilterEditorOnly)
			{
				Ar.SetFilterEditorOnly(true);
			}
			Ar.ArAllowLazyLoading = true;

			// FExportArchive special fields
			Ar.PackageDesc = &Package->Desc;
			Ar.HeaderData = HeaderData;
			Ar.ImportStore = &Package->ImportStore;
			Ar.Exports = Exports;
			Ar.ExternalReadDependencies = &Package->ExternalReadDependencies;

			// Check if the package is instanced
			FName PackageNameToLoad = Package->Desc.PackagePathToLoad.GetPackageFName();
			if (Package->Desc.UPackageName != PackageNameToLoad)
			{
				PackageNameToLoad.ToString(Ar.InstancedPackageSourceName);
				Package->Desc.UPackageName.ToString(Ar.InstancedPackageInstanceName);
			}
		}

		while (Package->ExportBundleEntryIndex < int32(ExportBundle->EntryCount))
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntries[ExportBundle->FirstEntryIndex + Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_ProcessExportBundle")))
			{
				return EEventLoadNodeExecutionResult::Timeout;
			}
			const FExportMapEntry& ExportMapEntry = HeaderData->ExportMap[BundleEntry.LocalExportIndex];
			FExportObject& Export = Exports[BundleEntry.LocalExportIndex];
			Export.bFiltered = FilterExport(ExportMapEntry.FilterFlags);

			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Create)
			{
				Package->EventDrivenCreateExport(*HeaderData, Exports, BundleEntry.LocalExportIndex);
			}
			else
			{
				check(BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize);

				const uint64 CookedSerialSize = ExportMapEntry.CookedSerialSize;
				UObject* Object = Export.Object;

				check(SerializationState->CurrentExportDataPtr + CookedSerialSize <= IoBuffer.Data() + IoBuffer.DataSize());
				check(Object || Export.bFiltered || Export.bExportLoadFailed);

				Ar.ExportBufferBegin(Object, ExportMapEntry.CookedSerialOffset, ExportMapEntry.CookedSerialSize);

				const int64 Pos = Ar.Tell();
				UE_ASYNC_PACKAGE_CLOG(
					CookedSerialSize > uint64(Ar.TotalSize() - Pos), Fatal, Package->Desc, TEXT("ObjectSerializationError"),
					TEXT("%s: Serial size mismatch: Expected read size %d, Remaining archive size: %d"),
					Object ? *Object->GetFullName() : TEXT("null"), CookedSerialSize, uint64(Ar.TotalSize() - Pos));

				const bool bSerialized = Package->EventDrivenSerializeExport(*HeaderData, Exports, BundleEntry.LocalExportIndex, Ar);
				if (!bSerialized)
				{
					Ar.Skip(CookedSerialSize);
				}
				UE_ASYNC_PACKAGE_CLOG(
					CookedSerialSize != uint64(Ar.Tell() - Pos), Fatal, Package->Desc, TEXT("ObjectSerializationError"),
					TEXT("%s: Serial size mismatch: Expected read size %d, Actual read size %d"),
					Object ? *Object->GetFullName() : TEXT("null"), CookedSerialSize, uint64(Ar.Tell() - Pos));

				Ar.ExportBufferEnd();

				check((Object && !Object->HasAnyFlags(RF_NeedLoad)) || Export.bFiltered || Export.bExportLoadFailed);

				SerializationState->CurrentExportDataPtr += CookedSerialSize;
			}
			++Package->ExportBundleEntryIndex;
		}
	}
	
	Package->ExportBundleEntryIndex = 0;

	if (++Package->ProcessedExportBundlesCount == Package->Data.ExportBundleCount)
	{
		Package->ProcessedExportBundlesCount = 0;
		Package->HeaderData.OnReleaseHeaderBuffer();
		Package->SerializationState.ReleaseIoRequest();
#if WITH_EDITOR
		if (Package->OptionalSegmentHeaderData.IsSet())
		{
			Package->OptionalSegmentHeaderData->OnReleaseHeaderBuffer();
			Package->OptionalSegmentSerializationState->ReleaseIoRequest();
		}
#endif

		if (Package->ExternalReadDependencies.Num() == 0)
		{
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles);
			Package->GetPackageNode(Package_ExportsSerialized).ReleaseBarrier(&ThreadState);
		}
		else
		{
			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles);
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::WaitingForExternalReads;
			Package->AsyncLoadingThread.ExternalReadQueue.Enqueue(Package);
		}
	}

	return EEventLoadNodeExecutionResult::Complete;
}

UObject* FAsyncPackage2::EventDrivenIndexToObject(const FAsyncPackageHeaderData& Header, const TArrayView<const FExportObject>& Exports, FPackageObjectIndex Index, bool bCheckSerialized)
{
	UObject* Result = nullptr;
	if (Index.IsNull())
	{
		return Result;
	}
	if (Index.IsExport())
	{
		Result = Exports[Index.ToExport()].Object;
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
		UE_CLOG(Result->HasAnyInternalFlags(EInternalObjectFlags::Unreachable), LogStreaming, Fatal, TEXT("Returning an object  (%s) from EventDrivenIndexToObject that is unreachable."), *Result->GetFullName());
	}
#endif
	return Result;
}


void FAsyncPackage2::EventDrivenCreateExport(const FAsyncPackageHeaderData& Header, const TArrayView<FExportObject>& Exports, int32 LocalExportIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateExport);

	const FExportMapEntry& Export = Header.ExportMap[LocalExportIndex];
	FExportObject& ExportObject = Exports[LocalExportIndex];
	UObject*& Object = ExportObject.Object;
	check(!Object);

	TRACE_LOADTIME_CREATE_EXPORT_SCOPE(this, &Object);

	FName ObjectName;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ObjectNameFixup);
		ObjectName = Header.NameMap.GetName(Export.ObjectName);
	}

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

	bool bIsCompleteyLoaded = false;
	UClass* LoadClass = Export.ClassIndex.IsNull() ? UClass::StaticClass() : CastEventDrivenIndexToObject<UClass>(Header, Exports, Export.ClassIndex, true);
	UObject* ThisParent = Export.OuterIndex.IsNull() ? LinkerRoot : EventDrivenIndexToObject(Header, Exports, Export.OuterIndex, false);

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
		ExportObject.SuperObject = EventDrivenIndexToObject(Header, Exports, Export.SuperIndex, false);
		if (!ExportObject.SuperObject)
		{
			UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find SuperStruct object for %s"), *ObjectName.ToString());
			ExportObject.bExportLoadFailed = true;
			return;
		}
	}
	// Find the Archetype object for the one we are loading.
	check(!Export.TemplateIndex.IsNull());
	ExportObject.TemplateObject = EventDrivenIndexToObject(Header, Exports, Export.TemplateIndex, true);
	if (!ExportObject.TemplateObject)
	{
		UE_ASYNC_PACKAGE_LOG(Error, Desc, TEXT("CreateExport"), TEXT("Could not find template object for %s"), *ObjectName.ToString());
		ExportObject.bExportLoadFailed = true;
		return;
	}

	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(GetLinkerRoot(), ELLMTagSet::Assets);
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(LoadClass, ELLMTagSet::AssetClasses);
    UE_TRACE_METADATA_SCOPE_ASSET(GetLinkerRoot(), LoadClass);

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

		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateExport"), TEXT("Created %s export %s. Tracked as 0x%llX:0x%llX"),
			Object->HasAnyFlags(RF_Public) ? TEXT("public") : TEXT("private"), *Object->GetPathName(), Desc.UPackageId.Value(), Export.PublicExportHash);
	}
	else
	{
		UE_ASYNC_PACKAGE_LOG_VERBOSE(VeryVerbose, Desc, TEXT("CreateExport"), TEXT("Created %s export %s. Not tracked."),
			Object->HasAnyFlags(RF_Public) ? TEXT("public") : TEXT("private"), *Object->GetPathName());
	}
	Object->SetInternalFlags(FlagsToSet);
}

bool FAsyncPackage2::EventDrivenSerializeExport(const FAsyncPackageHeaderData& Header, const TArrayView<FExportObject>& Exports, int32 LocalExportIndex, FExportArchive& Ar)
{
	LLM_SCOPE(ELLMTag::UObject);
	TRACE_CPUPROFILER_EVENT_SCOPE(SerializeExport);

	const FExportMapEntry& Export = Header.ExportMap[LocalExportIndex];
	FExportObject& ExportObject = Exports[LocalExportIndex];
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

	const UClass* LoadClass = Export.ClassIndex.IsNull() ? UClass::StaticClass() : CastEventDrivenIndexToObject<UClass>(Header, Exports, Export.ClassIndex, true);
	UE_TRACE_METADATA_SCOPE_ASSET(Object, LoadClass);
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Object, ELLMTagSet::Assets);
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(LoadClass, ELLMTagSet::AssetClasses);

	// cache archetype
	// prevents GetArchetype from hitting the expensive GetArchetypeFromRequiredInfoImpl
	check(ExportObject.TemplateObject);
	CacheArchetypeForObject(Object, ExportObject.TemplateObject);

	Object->ClearFlags(RF_NeedLoad);

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	UObject* PrevSerializedObject = LoadContext->SerializedObject;
	LoadContext->SerializedObject = Object;

	Ar.TemplateForGetArchetypeFromLoader = ExportObject.TemplateObject;

	if (Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SerializeDefaultObject);
		Object->GetClass()->SerializeDefaultObject(Object, Ar);
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SerializeObject);
		Object->Serialize(Ar);
	}
	Ar.TemplateForGetArchetypeFromLoader = nullptr;

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

EEventLoadNodeExecutionResult FAsyncPackage2::Event_ExportsDone(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, int32)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Event_ExportsDone);
	UE_ASYNC_PACKAGE_DEBUG(Package->Desc);
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::ProcessExportBundles || Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::WaitingForExternalReads);
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::ExportsDone;

	if (!Package->bLoadHasFailed && Package->Desc.bCanBeImported)
	{
		FLoadedPackageRef& PackageRef = Package->AsyncLoadingThread.GlobalImportStore.FindPackageRefChecked(Package->Desc.UPackageId);
		PackageRef.SetAllPublicExportsLoaded();
	}

	if (!Package->Data.ShaderMapHashes.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReleasePreloadedShaderMaps);
		FCoreDelegates::ReleasePreloadedPackageShaderMaps.ExecuteIfBound(Package->Data.ShaderMapHashes);
	}

	Package->ConditionalBeginPostLoad(ThreadState);
	return EEventLoadNodeExecutionResult::Complete;
}

bool FAsyncPackage2::HaveAllDependenciesReachedStateDebug(FAsyncLoadingThreadState2& ThreadState, FAsyncPackage2* Package, TSet<FAsyncPackage2*>& VisitedPackages, EAsyncPackageLoadingState2 WaitForPackageState, bool bExcludeZenPackages)
{
	for (FAsyncPackage2* ImportedPackage : Package->Data.ImportedAsyncPackages)
	{
		if (ImportedPackage)
		{
#if ALT2_ENABLE_LINKERLOAD_SUPPORT
			if (bExcludeZenPackages && !ImportedPackage->LinkerLoadState.IsSet())
			{
				continue;
			}
#endif
			if (ThreadState.PackagesExcludedFromStateDependencyChecks.Contains(ImportedPackage))
			{
				continue;
			}
			if (VisitedPackages.Contains(ImportedPackage))
			{
				continue;
			}
			VisitedPackages.Add(ImportedPackage);

			if (ImportedPackage->AsyncPackageLoadingState < WaitForPackageState)
			{
				return false;
			}

			if (!HaveAllDependenciesReachedStateDebug(ThreadState, ImportedPackage, VisitedPackages, WaitForPackageState, bExcludeZenPackages))
			{
				return false;
			}
		}
	}
	return true;
}

bool FAsyncPackage2::HaveAllDependenciesReachedState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32 CurrentTick, bool bExcludeZenPackages)
{
	FAllDependenciesState& ThisState = this->*StateMemberPtr;
	if (ThisState.bAllDone)
	{
		return true;
	}
	if (AsyncPackageLoadingState < WaitForPackageState)
	{
		return false;
	}
	ThisState.UpdateTick(CurrentTick);
	UpdateDependenciesStateRecursive(ThreadState, StateMemberPtr, WaitForPackageState, CurrentTick, this, bExcludeZenPackages);
	check(ThisState.bAllDone || (ThisState.WaitingForPackage && ThisState.WaitingForPackage->AsyncPackageLoadingState <= WaitForPackageState));
	return ThisState.bAllDone;
}

void FAsyncPackage2::UpdateDependenciesStateRecursive(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32 CurrentTick, FAsyncPackage2* Root, bool bExcludeZenPackages)
{
	FAllDependenciesState& ThisState = this->*StateMemberPtr;

	check(!ThisState.bVisitedMark);
	check(!ThisState.bAllDone);
	check(!ThisState.bAnyNotDone);

	ThisState.bVisitedMark = true;

	if (FAsyncPackage2* WaitingForPackage = ThisState.WaitingForPackage)
	{
		if (ThreadState.PackagesExcludedFromStateDependencyChecks.Contains(WaitingForPackage))
		{
			ThisState.WaitingForPackage = nullptr;
		}
		else
		{
			FAllDependenciesState& WaitingForPackageState = WaitingForPackage->*StateMemberPtr;
			if (WaitingForPackage->AsyncPackageLoadingState < WaitForPackageState)
			{
				ThisState.bAnyNotDone = true;
				return;
			}
			else if (!WaitingForPackageState.bAllDone)
			{
				WaitingForPackageState.UpdateTick(CurrentTick);
				if (!WaitingForPackageState.bVisitedMark)
				{
					WaitingForPackage->UpdateDependenciesStateRecursive(ThreadState, StateMemberPtr, WaitForPackageState, CurrentTick, Root, bExcludeZenPackages);
				}
				if (WaitingForPackageState.bAnyNotDone)
				{
					ThisState.bAnyNotDone = true;
					return;
				}
			}
		}
	}

	bool bAllDone = true;
	FAsyncPackage2* WaitingForPackage = nullptr;
	for (FAsyncPackage2* ImportedPackage : Data.ImportedAsyncPackages)
	{
		if (!ImportedPackage)
		{
			continue;
		}

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
		if (bExcludeZenPackages && !ImportedPackage->LinkerLoadState.IsSet())
		{
			continue;
		}
#endif

		if (ThreadState.PackagesExcludedFromStateDependencyChecks.Contains(ImportedPackage))
		{
			continue;
		}

		FAllDependenciesState& ImportedPackageState = ImportedPackage->*StateMemberPtr;

		if (ImportedPackageState.bAllDone)
		{
			continue;
		}

		ImportedPackageState.UpdateTick(CurrentTick);

		if (ImportedPackage->AsyncPackageLoadingState < WaitForPackageState)
		{
			ImportedPackageState.bAnyNotDone = true;
		}
		else if (!ImportedPackageState.bVisitedMark)
		{
			ImportedPackage->UpdateDependenciesStateRecursive(ThreadState, StateMemberPtr, WaitForPackageState, CurrentTick, Root, bExcludeZenPackages);
		}

		if (ImportedPackageState.bAnyNotDone)
		{
			ThisState.bAnyNotDone = true;
			WaitingForPackage = ImportedPackage;
			break;
		}
		else if (!ImportedPackageState.bAllDone)
		{
			bAllDone = false;
		}
	}
	if (WaitingForPackage)
	{
		check(WaitingForPackage != this);
		FAllDependenciesState::AddToWaitList(StateMemberPtr, WaitingForPackage, this);
	}
	else if (bAllDone || this == Root)
	{
		// If we're the root an not waiting for any package we're done
		ThisState.bAllDone = true;
	}
	else
	{
		// We didn't find any imported package that was not done but we could have a circular dependency back to the root which could either be done or end up waiting
		// for another package. Make us wait for the root so that we are ticked when it completes.
		FAllDependenciesState::AddToWaitList(StateMemberPtr, Root, this);
	}
}

void FAsyncPackage2::WaitForAllDependenciesToReachState(FAsyncLoadingThreadState2& ThreadState, FAllDependenciesState FAsyncPackage2::* StateMemberPtr, EAsyncPackageLoadingState2 WaitForPackageState, uint32& CurrentTickVariable, bool bExcludeZenPackages, TFunctionRef<void(FAsyncPackage2*)> OnStateReached)
{
	if (HaveAllDependenciesReachedState(ThreadState, StateMemberPtr, WaitForPackageState, CurrentTickVariable++, bExcludeZenPackages))
	{
		FAsyncPackage2* FirstPackageReadyToProceed = this;

		while (FirstPackageReadyToProceed)
		{
			FAsyncPackage2* PackageReadyToProceed = FirstPackageReadyToProceed;
			FAllDependenciesState& PackageReadyToProceedState = PackageReadyToProceed->*StateMemberPtr;
			FirstPackageReadyToProceed = PackageReadyToProceedState.NextLink;

			if (PackageReadyToProceed->AsyncPackageLoadingState > WaitForPackageState)
			{
				continue;
			}

#if DO_CHECK
			TSet<FAsyncPackage2*> VisitedPackages;
			check(HaveAllDependenciesReachedStateDebug(ThreadState, this, VisitedPackages, WaitForPackageState, bExcludeZenPackages));
#endif

			while (FAsyncPackage2* WaitingPackage = PackageReadyToProceedState.PackagesWaitingForThisHead)
			{
				FAllDependenciesState& WaitingPackageState = WaitingPackage->*StateMemberPtr;
				check(WaitingPackageState.WaitingForPackage == PackageReadyToProceed);
				if (WaitingPackage->HaveAllDependenciesReachedState(ThreadState, StateMemberPtr, WaitForPackageState, CurrentTickVariable++, bExcludeZenPackages))
				{
					FAllDependenciesState::RemoveFromWaitList(StateMemberPtr, PackageReadyToProceed, WaitingPackage);
					WaitingPackageState.NextLink = FirstPackageReadyToProceed;
					FirstPackageReadyToProceed = WaitingPackage;
				}
			}
			check(!PackageReadyToProceedState.PackagesWaitingForThisTail);
			check(PackageReadyToProceed->AsyncPackageLoadingState == WaitForPackageState);
			PackageReadyToProceed->AsyncPackageLoadingState = static_cast<EAsyncPackageLoadingState2>(static_cast<uint32>(WaitForPackageState) + 1);
			OnStateReached(PackageReadyToProceed);
		}
	}
}

void FAsyncPackage2::ConditionalBeginPostLoad(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginPostLoad);
	constexpr bool bExcludeZenPackages = false;
	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesSerializedState, EAsyncPackageLoadingState2::ExportsDone, AsyncLoadingThread.ConditionalBeginPostLoadTick, bExcludeZenPackages,
		[](FAsyncPackage2* Package)
		{
			for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.ExportBundleCount; ++ExportBundleIndex)
			{
				Package->GetExportBundleNode(EEventLoadNode2::ExportBundle_PostLoad, ExportBundleIndex).ReleaseBarrier();
			}
		});
}

void FAsyncPackage2::ConditionalFinishLoading(FAsyncLoadingThreadState2& ThreadState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalFinishLoading);
	constexpr bool bExcludeZenPackages = false;
	WaitForAllDependenciesToReachState(ThreadState, &FAsyncPackage2::AllDependenciesFullyLoadedState, EAsyncPackageLoadingState2::DeferredPostLoadDone, AsyncLoadingThread.ConditionalFinishLoadingTick, bExcludeZenPackages,
		[](FAsyncPackage2* Package)
		{
			Package->AsyncLoadingThread.LoadedPackagesToProcess.Add(Package);
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
		// No async postload for now
		Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoad;
		Package->GetExportBundleNode(ExportBundle_DeferredPostLoad, 0).ReleaseBarrier();
		return EEventLoadNodeExecutionResult::Complete;
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
	
	check(InExportBundleIndex < Package->Data.ExportBundleCount);

	EEventLoadNodeExecutionResult LoadingState = EEventLoadNodeExecutionResult::Complete;

	if (!Package->bLoadHasFailed)
	{
		// Begin async loading, simulates BeginLoad
		Package->BeginAsyncLoad();

		SCOPED_LOADTIMER(PostLoadObjectsTime);

		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		TGuardValue<bool> GuardIsRoutingPostLoad(ThreadContext.IsRoutingPostLoad, true);

		const bool bAsyncPostLoadEnabled = FAsyncLoadingThreadSettings::Get().bAsyncPostLoadEnabled;
		const bool bIsMultithreaded = Package->AsyncLoadingThread.IsMultithreaded();

#if WITH_EDITOR
		const FAsyncPackageHeaderData* HeaderData;
		const FExportBundleHeader* ExportBundle;
		TArrayView<FExportObject> Exports = Package->Data.Exports;
		if (InExportBundleIndex >= Package->HeaderData.ExportBundleHeaders.Num())
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
			ExportBundle = &HeaderData->ExportBundleHeaders[InExportBundleIndex - Package->HeaderData.ExportBundleHeaders.Num()];
			Exports.RightInline(HeaderData->ExportCount);
		}
		else
		{
			HeaderData = &Package->HeaderData;
			ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
			Exports.LeftInline(Package->HeaderData.ExportCount);
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
		const FExportBundleHeader* ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
		const TArrayView<FExportObject>& Exports = Package->Data.Exports;
#endif

		while (Package->ExportBundleEntryIndex < int32(ExportBundle->EntryCount))
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntries[ExportBundle->FirstEntryIndex + Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_PostLoadExportBundle")))
			{
				LoadingState = EEventLoadNodeExecutionResult::Timeout;
				break;
			}
			
			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				do
				{
					FExportObject& Export = Exports[BundleEntry.LocalExportIndex];
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
						ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
						{
							TRACE_LOADTIME_POSTLOAD_EXPORT_SCOPE(Object);
							Object->ConditionalPostLoad();
						}
						ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;
					}
				} while (false);
			}
			++Package->ExportBundleEntryIndex;
		}

		// End async loading, simulates EndLoad
		Package->EndAsyncLoad();
	}
	
	if (LoadingState == EEventLoadNodeExecutionResult::Timeout)
	{
		return LoadingState;
	}

	Package->ExportBundleEntryIndex = 0;

	if (++Package->ProcessedExportBundlesCount == Package->Data.ExportBundleCount)
	{
		Package->ProcessedExportBundlesCount = 0;
		if (Package->LinkerRoot && !Package->bLoadHasFailed)
		{
			UE_ASYNC_PACKAGE_LOG(Verbose, Package->Desc, TEXT("AsyncThread: FullyLoaded"),
				TEXT("Async loading of package is done, and UPackage is marked as fully loaded."));
			// mimic old loader behavior for now, but this is more correctly also done in FinishUPackage
			// called from ProcessLoadedPackagesFromGameThread just before complection callbacks
			Package->LinkerRoot->MarkAsFullyLoaded();
		}

		check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::PostLoad);
		Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredPostLoad;
		for (int32 ExportBundleIndex = 0; ExportBundleIndex < Package->Data.ExportBundleCount; ++ExportBundleIndex)
		{
			Package->GetExportBundleNode(ExportBundle_DeferredPostLoad, ExportBundleIndex).ReleaseBarrier();
		}
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

#if ALT2_ENABLE_LINKERLOAD_SUPPORT
	if (Package->LinkerLoadState.IsSet())
	{
		return Package->PostLoadLinkerLoadPackageExports(ThreadState);
	}
#endif

	check(InExportBundleIndex < Package->Data.ExportBundleCount);
	EEventLoadNodeExecutionResult LoadingState = EEventLoadNodeExecutionResult::Complete;

	if (Package->bLoadHasFailed)
	{
		FSoftObjectPath::InvalidateTag();
		FUniqueObjectGuid::InvalidateTag();
	}
	else
	{
		TGuardValue<bool> GuardIsRoutingPostLoad(PackageScope.ThreadContext.IsRoutingPostLoad, true);
		FAsyncLoadingTickScope2 InAsyncLoadingTick(Package->AsyncLoadingThread);

#if WITH_EDITOR
		const FAsyncPackageHeaderData* HeaderData;
		const FExportBundleHeader* ExportBundle;
		TArrayView<FExportObject> Exports = Package->Data.Exports;
		if (InExportBundleIndex >= Package->HeaderData.ExportBundleHeaders.Num())
		{
			HeaderData = Package->OptionalSegmentHeaderData.GetPtrOrNull();
			check(HeaderData);
			ExportBundle = &HeaderData->ExportBundleHeaders[InExportBundleIndex - Package->HeaderData.ExportBundleHeaders.Num()];
			Exports.RightInline(HeaderData->ExportCount);
		}
		else
		{
			HeaderData = &Package->HeaderData;
			ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
			Exports.LeftInline(Package->HeaderData.ExportCount);
		}
#else
		const FAsyncPackageHeaderData* HeaderData = &Package->HeaderData;
		const FExportBundleHeader* ExportBundle = &Package->HeaderData.ExportBundleHeaders[InExportBundleIndex];
		const TArrayView<FExportObject>& Exports = Package->Data.Exports;
#endif

		while (Package->ExportBundleEntryIndex < int32(ExportBundle->EntryCount))
		{
			const FExportBundleEntry& BundleEntry = HeaderData->ExportBundleEntries[ExportBundle->FirstEntryIndex + Package->ExportBundleEntryIndex];
			if (ThreadState.IsTimeLimitExceeded(TEXT("Event_DeferredPostLoadExportBundle")))
			{
				LoadingState = EEventLoadNodeExecutionResult::Timeout;
				break;
			}

			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				do
				{
					FExportObject& Export = Exports[BundleEntry.LocalExportIndex];
					if (Export.bFiltered | Export.bExportLoadFailed)
					{
						break;
					}

					UObject* Object = Export.Object;
					check(Object);
					check(!Object->HasAnyFlags(RF_NeedLoad));
					if (Object->HasAnyFlags(RF_NeedPostLoad))
					{
						PackageScope.ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
						{
							TRACE_LOADTIME_POSTLOAD_EXPORT_SCOPE(Object);
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

	if (++Package->ProcessedExportBundlesCount == Package->Data.ExportBundleCount)
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
	check(ExportBundleIndex < uint32(Data.ExportBundleCount));
	uint32 ExportBundleNodeIndex = ExportBundleIndex * EEventLoadNode2::ExportBundle_NumPhases + Phase;
	return Data.ExportBundleNodes[ExportBundleNodeIndex];
}

FAsyncLoadingSyncLoadContext* FAsyncLoadingThread2::UpdateSyncLoadContext(FAsyncLoadingThreadState2& ThreadState)
{
	if (ThreadState.bIsAsyncLoadingThread)
	{
		FAsyncLoadingSyncLoadContext* CreatedOnMainThread;
		while (ThreadState.SyncLoadContextsCreatedOnGameThread.Dequeue(CreatedOnMainThread))
		{
			ThreadState.SyncLoadContextStack.Push(CreatedOnMainThread);
		}
	}
	if (ThreadState.SyncLoadContextStack.IsEmpty())
	{
		return nullptr;
	}
	FAsyncLoadingSyncLoadContext* SyncLoadContext = ThreadState.SyncLoadContextStack.Top();
	if (ThreadState.bIsAsyncLoadingThread && !ContainsRequestID(SyncLoadContext->RequestId))
	{
		SyncLoadContext->ReleaseRef();
		ThreadState.SyncLoadContextStack.Pop();
		if (ThreadState.SyncLoadContextStack.IsEmpty())
		{
			return nullptr;
		}
		SyncLoadContext = ThreadState.SyncLoadContextStack.Top();
	}
	if (ThreadState.bCanAccessAsyncLoadingThreadData && !SyncLoadContext->bHasFoundRequestedPackage)
	{
		if (ThreadState.bIsAsyncLoadingThread)
		{
			// Ensure that we've created the package we're waiting for
			CreateAsyncPackagesFromQueue(ThreadState);
		}
		if (FAsyncPackage2* RequestedPackage = RequestIdToPackageMap.FindRef(SyncLoadContext->RequestId))
		{
			SyncLoadContext->bHasFoundRequestedPackage = true;
			SyncLoadContext->RequestedPackageDebug = RequestedPackage;
			IncludePackageInSyncLoadContext(SyncLoadContext->ContextId, RequestedPackage);
		}
	}
	return SyncLoadContext;
}

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething)
{
	SCOPED_LOADTIMER(AsyncLoadingTime);

	check(IsInGameThread());

	FAsyncLoadingTickScope2 InAsyncLoadingTick(*this);
	uint32 LoopIterations = 0;

	RemoveUnreachableObjects(UnreachableObjects);
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

			if (FAsyncLoadingSyncLoadContext* SyncLoadContext = UpdateSyncLoadContext(ThreadState))
			{
				if (EventQueue.ExecuteSyncLoadEvents(ThreadState, *SyncLoadContext))
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

				EAsyncPackageState::Type Result = Package->ProcessExternalReads(FAsyncPackage2::ExternalReadAction_Wait);
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

EAsyncPackageState::Type FAsyncLoadingThread2::ProcessLoadedPackagesFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool& bDidSomething, int32 FlushRequestID)
{
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;

	// This is for debugging purposes only. @todo remove
	volatile int32 CurrentAsyncLoadingCounter = AsyncLoadingTickCounter;

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
	for (;;)
	{
		FPlatformMisc::PumpEssentialAppMessages();

		if (ThreadState.IsTimeLimitExceeded(TEXT("ProcessAsyncLoadingFromGameThread")))
		{
			Result = EAsyncPackageState::TimeOut;
			break;
		}

		bool bLocalDidSomething = false;
		if (FAsyncLoadingSyncLoadContext* SyncLoadContext = UpdateSyncLoadContext(ThreadState))
		{
			bLocalDidSomething |= MainThreadEventQueue.ExecuteSyncLoadEvents(ThreadState, *SyncLoadContext);
		}
		else
		{
			bLocalDidSomething |= MainThreadEventQueue.PopAndExecute(ThreadState);
		}

		bLocalDidSomething |= LoadedPackagesToProcess.Num() > 0;
		TArray<FAsyncPackage2*, TInlineAllocator<4>> PackagesReadyForCallback;
#if WITH_EDITOR
		TSet<UPackage*> CompletedUPackages;
#endif
		for (int32 PackageIndex = 0; PackageIndex < LoadedPackagesToProcess.Num(); ++PackageIndex)
		{
			SCOPED_LOADTIMER(ProcessLoadedPackagesTime);
			FAsyncPackage2* Package = LoadedPackagesToProcess[PackageIndex];
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

			FSoftObjectPath::InvalidateTag();
			FUniqueObjectGuid::InvalidateTag();

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
				if (!Package->bLoadHasFailed)
				{
#if WITH_EDITOR
					// In the editor we need to find any assets and packages and add them to list for later callback
					Package->GetLoadedAssetsAndPackages(LoadedAssets, CompletedUPackages);
#endif
					Package->ClearConstructedObjects();
				}
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
			TRACE_LOADTIME_END_LOAD_ASYNC_PACKAGE(Package);

			PackagesReadyForCallback.Add(Package);
		}

		// Call callbacks in a batch in a stack-local array. This is to ensure that callbacks that trigger
		// on each package load and call FlushAsyncLoading do not stack overflow by adding one FlushAsyncLoading
		// call per LoadedPackageToProcess onto the stack
		for (FAsyncPackage2* Package : PackagesReadyForCallback)
		{
			// Call external callbacks
			const EAsyncLoadingResult::Type LoadingResult = Package->HasLoadFailed() ? EAsyncLoadingResult::Failed : EAsyncLoadingResult::Succeeded;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(PackageCompletionCallbacks);
				Package->CallCompletionCallbacks(LoadingResult);
			}

			check(!CompletedPackages.Contains(Package));
			CompletedPackages.Add(Package);
			RemovePendingRequests(Package->RequestIDs);

			UE_ASYNC_PACKAGE_LOG(Verbose, Package->Desc, TEXT("GameThread: LoadCompleted"),
				TEXT("All loading of package is done, and the async package and load request will be deleted."));
		}
#if WITH_EDITOR
		// Call the global delegate for package endloads and set the bHasBeenLoaded flag that is used to
		// check which packages have reached this state
		for (UPackage* CompletedUPackage : CompletedUPackages)
		{
			CompletedUPackage->SetHasBeenEndLoaded(true);
		}
		FCoreUObjectDelegates::OnEndLoadPackage.Broadcast(
			FEndLoadPackageContext{ CompletedUPackages.Array(), 0, false /* bSynchronous */ });
#endif
		
		{
			TArray<FFailedPackageRequest> LocalFailedPackageRequests;
			{
				FScopeLock _(&FailedPackageRequestsCritical);
				Swap(LocalFailedPackageRequests, FailedPackageRequests);
			}

			bLocalDidSomething |= LocalFailedPackageRequests.Num() > 0;
			for (FFailedPackageRequest& FailedPackageRequest : LocalFailedPackageRequests)
			{
				FailedPackageRequest.Callback->ExecuteIfBound(FailedPackageRequest.PackageName, nullptr, EAsyncLoadingResult::Failed);
				RemovePendingRequests(TArrayView<int32>(&FailedPackageRequest.RequestID, 1));
				--PackagesWithRemainingWorkCounter;
				TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);
			}
		}

		bLocalDidSomething |= CompletedPackages.Num() > 0;
		for (int32 PackageIndex = 0; PackageIndex < CompletedPackages.Num(); ++PackageIndex)
		{
			FAsyncPackage2* Package = CompletedPackages[PackageIndex];
			UE_ASYNC_PACKAGE_DEBUG(Package->Desc);

			check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState2::Complete);
			Package->AsyncPackageLoadingState = EAsyncPackageLoadingState2::DeferredDelete;

			CompletedPackages.RemoveAtSwap(PackageIndex--);
			Package->ClearImportedPackages();
			Package->ReleaseRef();
		}
		
		if (!bLocalDidSomething)
		{
			break;
		}

		bDidSomething = true;
		
		if (FlushRequestID != INDEX_NONE && !ContainsRequestID(FlushRequestID))
		{
			// The only package we care about has finished loading, so we're good to exit
			break;
		}
	}

	if (Result == EAsyncPackageState::Complete)
	{
#if WITH_EDITORONLY_DATA
		// This needs to happen after loading new blueprints in the editor, and this is handled in EndLoad for synchronous loads
		FBlueprintSupport::FlushReinstancingQueue();
#endif

#if WITH_EDITOR
		// In editor builds, call the asset load callback. This happens in both editor and standalone to match EndLoad
		TSet<FWeakObjectPtr> TempLoadedAssets = LoadedAssets;
		LoadedAssets.Reset();

		// Make a copy because LoadedAssets could be modified by one of the OnAssetLoaded callbacks
		for (const FWeakObjectPtr& WeakAsset : TempLoadedAssets)
		{
			// It may have been unloaded/marked pending kill since being added, ignore those cases
			if (UObject* LoadedAsset = WeakAsset.Get())
			{
				FCoreUObjectDelegates::OnAssetLoaded.Broadcast(LoadedAsset);
			}
		}
#endif

		// We're not done until all packages have been deleted
		Result = CompletedPackages.Num() ? EAsyncPackageState::PendingImports  : EAsyncPackageState::Complete;
	}

	return Result;
}

EAsyncPackageState::Type FAsyncLoadingThread2::TickAsyncLoadingFromGameThread(FAsyncLoadingThreadState2& ThreadState, bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 FlushRequestID, bool& bDidSomething)
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
		ThreadState.SetTimeLimit(bUseTimeLimit, TimeLimit);

		const bool bIsMultithreaded = FAsyncLoadingThread2::IsMultithreaded();
		bool bMayProcessPendingCDOs = PendingCDOs.Num() > 0 && (PendingIoRequestsCounter.Load() == 0);
		double TickStartTime = FPlatformTime::Seconds();

		{
			Result = ProcessLoadedPackagesFromGameThread(ThreadState, bDidSomething, FlushRequestID);
			double TimeLimitUsedForProcessLoaded = FPlatformTime::Seconds() - TickStartTime;
			UE_CLOG(!GIsEditor && bUseTimeLimit && TimeLimitUsedForProcessLoaded > .1f, LogStreaming, Warning, TEXT("Took %6.2fms to ProcessLoadedPackages"), float(TimeLimitUsedForProcessLoaded) * 1000.0f);
		}

		if (!bIsMultithreaded && Result != EAsyncPackageState::TimeOut)
		{
			Result = TickAsyncThreadFromGameThread(ThreadState, bDidSomething);
		}

		if (Result != EAsyncPackageState::TimeOut)
		{
			if (!bDidSomething && bMayProcessPendingCDOs)
			{
				// Only process/create pending CDOs when absolutely required to unblock the current FlushAsyncLoading stack during initial load,
				// i.e. when we are not making any progress and we are sure that there are no outstanding IO requests.
				if (PendingIoRequestsCounter.Load() == 0)
				{
					bDidSomething = ProcessPendingCDOs();
				}
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

	return Result;
}

FAsyncLoadingThread2::FAsyncLoadingThread2(FIoDispatcher& InIoDispatcher, IAsyncPackageLoader* InUncookedPackageLoader)
	: Thread(nullptr)
	, IoDispatcher(InIoDispatcher)
	, UncookedPackageLoader(InUncookedPackageLoader)
	, PackageStore(FPackageStore::Get())
{
#if !WITH_EDITOR
	IsEventDrivenLoaderEnabled(); // make sure the one time init inside runs
	GEventDrivenLoaderEnabled = true;
#endif

	EventQueue.SetZenaphore(&AltZenaphore);

	AsyncPackageLookup.Reserve(DefaultAsyncPackagesReserveCount);
	PendingPackages.Reserve(DefaultAsyncPackagesReserveCount);
	RequestIdToPackageMap.Reserve(DefaultAsyncPackagesReserveCount);

	EventSpecs.AddDefaulted(EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_NumPhases);
	EventSpecs[EEventLoadNode2::Package_ProcessSummary] = { &FAsyncPackage2::Event_ProcessPackageSummary, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_SetupDependencies] = { &FAsyncPackage2::Event_SetupDependencies, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_ExportsSerialized] = { &FAsyncPackage2::Event_ExportsDone, &EventQueue, true };

	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_Process] = { &FAsyncPackage2::Event_ProcessExportBundle, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_PostLoad] = { &FAsyncPackage2::Event_PostLoadExportBundle, &EventQueue, false };
	EventSpecs[EEventLoadNode2::Package_NumPhases + EEventLoadNode2::ExportBundle_DeferredPostLoad] = { &FAsyncPackage2::Event_DeferredPostLoadExportBundle, &MainThreadEventQueue, false };

	CancelLoadingEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadResumedEvent = FPlatformProcess::GetSynchEventFromPool();
	AsyncLoadingTickCounter = 0;

	FCoreUObjectInternalDelegates::GetOnLeakedPackageRenameDelegate().AddRaw(this, &FAsyncLoadingThread2::OnLeakedPackageRename);
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FAsyncLoadingThread2::OnPreGarbageCollect);

	FAsyncLoadingThreadState2::TlsSlot = FPlatformTLS::AllocTlsSlot();
	GameThreadState = MakeUnique<FAsyncLoadingThreadState2>(GraphAllocator, IoDispatcher);
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
	FCoreUObjectInternalDelegates::GetOnLeakedPackageRenameDelegate().RemoveAll(this);

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
	while (!bStopRequested)
	{
		if (CurrentState == EMainState::Suspended)
		{
			// suspended, sleep until loading can be resumed
			while (!bStopRequested)
			{
				if (!bSuspendRequested.Load(EMemoryOrder::SequentiallyConsistent) && !IsGarbageCollectionWaiting())
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
			while (!bStopRequested)
			{
				if (bShouldSuspend || bSuspendRequested.Load(EMemoryOrder::Relaxed) || IsGarbageCollectionWaiting())
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
						EAsyncPackageState::Type Result = Package->ProcessExternalReads(FAsyncPackage2::ExternalReadAction_Wait);
						check(Result == EAsyncPackageState::Complete);
						bShouldWaitForExternalReads = false;
						continue;
					}


					if (QueuedPackagesCounter || !PendingPackages.IsEmpty())
					{
						if (CreateAsyncPackagesFromQueue(ThreadState))
						{
							// Fall through to FAsyncLoadEventQueue2 processing unless we need to suspend
							if (bSuspendRequested.Load(EMemoryOrder::Relaxed) || IsGarbageCollectionWaiting())
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
							if (FAsyncLoadingSyncLoadContext* SyncLoadContext = UpdateSyncLoadContext(ThreadState))
							{
								if (EventQueue.ExecuteSyncLoadEvents(ThreadState, *SyncLoadContext))
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
							if (bSuspendRequested.Load(EMemoryOrder::Relaxed) || IsGarbageCollectionWaiting())
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
							EAsyncPackageState::Type Result = (*Package)->ProcessExternalReads(FAsyncPackage2::ExternalReadAction_Poll);
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
	bSuspendRequested = true;
	bStopRequested = true;
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
	if (!bSuspendRequested)
	{
		bSuspendRequested = true;
		if (IsMultithreaded())
		{
			TRACE_LOADTIME_SUSPEND_ASYNC_LOADING();
			AltZenaphore.NotifyAll();
			ThreadSuspendedEvent->Wait();
		}
	}
}

void FAsyncLoadingThread2::ResumeLoading()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ResumeLoading);
	check(IsInGameThread() && !IsInSlateThread());
	if (bSuspendRequested)
	{
		bSuspendRequested = false;
		if (IsMultithreaded())
		{
			ThreadResumedEvent->Wait();
			TRACE_LOADTIME_RESUME_ASYNC_LOADING();
		}
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
					GlobalImportStore.VerifyPackageForRemoval(Package);
				}
				GlobalImportStore.VerifyObjectForRemoval(Object);
			}
		});
	}
}

void FAsyncLoadingThread2::OnLeakedPackageRename(UPackage* Package)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(OnLeakedPackageRename);
	check(IsInGameThread());

	// We don't care about levelstreaming /Temp/ packages that are never imported by other packages
	if (!Package->CanBeImported())
	{
		return;
	}

	// Code such as LoadMap or LevelStreaming is about to rename a loaded package which was detected as leaking so that we can load another copy of it.
	UE_LOG(LogStreaming, Display, TEXT("Renaming leaked package %s (0x%llX)"), *Package->GetName(), Package->GetPackageId().ValueForDebugging());

	if (!FGCCSyncObject::Get().IsGCLocked())
	{
		// If this function is called outside of a GC (CheckAndHandleStaleWorldObjectReferences), then flush async loading so that we can be sure nothing is modifying the stores,
		// and that nothing is depending on this package. This will also empty the deferred delete queue.
		FlushAsyncLoading(); 
	}
	else
	{
		// If this function is called from within a GC the ALT will be inactive and not touching the deferred delete queue.
		// Flush the delete queue to clear references to the renamed package, if there are still references after this we're not done with loading the package and it shouldn't have been renamed.
		ProcessDeferredDeletePackagesQueue();
	}

	// We need to protect UnreachableObjects since the GC will wake up the ALT from NotifyUnreachableObjects to run RemoveUnreachableObjects on the ALT.
	// FlushAsyncLoading and ProcessDeferredDeletePackagesQueue will not empty the UnreachableObjects array that was just filled by GC.
	FScopeLock UnreachableObjectsLock(&UnreachableObjectsCritical);

	// unreachable objects from last GC should typically have been processed already,
	// if not handle them here before processing new ones
	RemoveUnreachableObjects(UnreachableObjects);

	// If a package that can be imported was leaked and renamed, then go ahead and remove it.
	// Note that it must exist in the global import store at this point since it is normally only trimmed during GC.
	GlobalImportStore.RemovePackage(Package);

	// Clear the CanBeImportedFlag so that this package is only removed once,
	// else we would try and remove it again during GC, which would instead remove the reloaded package if it exists
	Package->SetCanBeImportedFlag(false);
}

void FAsyncLoadingThread2::OnPreGarbageCollect()
{
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

	// Mark objects created during async loading process (e.g. from within PostLoad or CreateExport) as async loaded so they 
	// cannot be found. This requires also keeping track of them so we can remove the async loading flag later one when we 
	// finished routing PostLoad to all objects.
	if (!bSubObjectThatAlreadyExists)
	{
		Object->SetInternalFlags(EInternalObjectFlags::AsyncLoading);
	}
	FAsyncPackage2* AsyncPackage2 = (FAsyncPackage2*)ThreadContext.AsyncPackage;
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
	AddRequestID(Desc.RequestID);

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
	for (int32 ExportBundleIndex = 0; ExportBundleIndex < Data.ExportBundleCount; ++ExportBundleIndex)
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

	checkf(RefCount == 0, TEXT("RefCount is not 0 when deleting package %s"),
		*Desc.PackagePathToLoad.GetPackageFName().ToString());

	checkf(ConstructedObjects.Num() == 0, TEXT("ClearConstructedObjects() has not been called for package %s"),
		*Desc.PackagePathToLoad.GetPackageFName().ToString());

	FMemory::Free(Data.MemoryBuffer);
}

void FAsyncPackage2::ReleaseRef()
{
	check(RefCount > 0);
	if (--RefCount == 0)
	{
		FAsyncLoadingThread2& AsyncLoadingThreadLocal = AsyncLoadingThread;
		AsyncLoadingThreadLocal.DeferredDeletePackages.Enqueue(this);
		AsyncLoadingThreadLocal.AltZenaphore.NotifyOne();
	}
}

void FAsyncPackage2::ClearImportedPackages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ClearImportedPackages);
	for (FAsyncPackage2* ImportedAsyncPackage : Data.ImportedAsyncPackages)
	{
		if (ImportedAsyncPackage)
		{
			ImportedAsyncPackage->ReleaseRef();
		}
	}
	Data.ImportedAsyncPackages = MakeArrayView(Data.ImportedAsyncPackages.GetData(), 0);
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

void FAsyncPackage2::AddRequestID(int32 Id)
{
	if (Id > 0)
	{
		if (Desc.RequestID == INDEX_NONE)
		{
			// For debug readability
			Desc.RequestID = Id;
		}
		RequestIDs.Add(Id);
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

#if WITH_EDITOR
void FAsyncPackage2::GetLoadedAssetsAndPackages(TSet<FWeakObjectPtr>& AssetList, TSet<UPackage*>& PackageList)
{
	for (UObject* Object : ConstructedObjects)
	{
		if (IsValid(Object) && Object->IsAsset())
		{
			AssetList.Add(Object);
		}
	}

	// All ConstructedObjects belong to this package, so we only have to consider the single package in this->LinkerRoot
	if (LinkerRoot && !LinkerRoot->HasAnyFlags(RF_Transient) && !LinkerRoot->HasAnyPackageFlags(PKG_InMemoryOnly))
	{
		PackageList.Add(LinkerRoot);
	}
}
#endif

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

	// temp packages are never stored or found in global import store
	FLoadedPackageRef* PackageRef = nullptr;

	// Try to find existing package or create it if not already present.
	UPackage* ExistingPackage = nullptr;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFind);
		if (Desc.bCanBeImported)
		{
			PackageRef = AsyncLoadingThread.GlobalImportStore.FindPackageRef(Desc.UPackageId);
			UE_ASYNC_PACKAGE_CLOG(!PackageRef, Fatal, Desc, TEXT("CreateUPackage"), TEXT("Package has been destroyed by GC."));
			LinkerRoot = PackageRef->GetPackage();
#if DO_CHECK
			if (LinkerRoot)
			{
				UPackage* FoundPackage = FindObjectFast<UPackage>(nullptr, Desc.UPackageName);
				checkf(LinkerRoot == FoundPackage,
					TEXT("LinkerRoot '%s' (%p) is different from FoundPackage '%s' (%p)"),
					*LinkerRoot->GetName(), LinkerRoot, FoundPackage ? *FoundPackage->GetName() : TEXT("null"), FoundPackage);
			}
#endif
		}
		if (!LinkerRoot)
		{
			// Packages can be created outside the loader, i.e from ResolveName via StaticLoadObject
			ExistingPackage = FindObjectFast<UPackage>(nullptr, Desc.UPackageName);
		}
	}
	if (!LinkerRoot)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPackageCreate);
		if (ExistingPackage)
		{
			LinkerRoot = ExistingPackage;
		}
		else 
		{
			LinkerRoot = NewObject<UPackage>(/*Outer*/nullptr, Desc.UPackageName);
			bCreatedLinkerRoot = true;
		}
		if (PackageRef)
		{
			PackageRef->SetPackage(LinkerRoot);
		}
	}

	LinkerRoot->SetFlags(RF_Public | RF_WasLoaded);
	LinkerRoot->SetLoadedPath(Desc.PackagePathToLoad);
	LinkerRoot->SetCanBeImportedFlag(Desc.bCanBeImported);
	LinkerRoot->SetPackageId(Desc.UPackageId);
	LinkerRoot->SetPackageFlags(Desc.PackageFlags);
#if WITH_EDITOR
	LinkerRoot->SetPIEInstanceID(Desc.PIEInstanceID);
#endif

	EInternalObjectFlags FlagsToSet = EInternalObjectFlags::Async;
	if (Desc.bCanBeImported)
	{
		FlagsToSet |= EInternalObjectFlags::LoaderImport;
	}
	LinkerRoot->SetInternalFlags(FlagsToSet);

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

EAsyncPackageState::Type FAsyncPackage2::ProcessExternalReads(EExternalReadAction Action)
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
	GetPackageNode(Package_ExportsSerialized).ReleaseBarrier();
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
	if (LinkerRoot)
	{
		if (!bLoadHasFailed)
		{
			// Mark package as having been fully loaded and update load time.
			LinkerRoot->MarkAsFullyLoaded();
			LinkerRoot->SetLoadTime(FPlatformTime::Seconds() - LoadStartTime);
		}
		else
		{
			// Clean up UPackage so it can't be found later
			if (bCreatedLinkerRoot && !LinkerRoot->IsRooted())
			{
				LinkerRoot->ClearFlags(RF_NeedPostLoad | RF_NeedLoad | RF_NeedPostLoadSubobjects);
				LinkerRoot->MarkAsGarbage();
				LinkerRoot->Rename(*MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass()).ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional);
			}
		}
	}
}

void FAsyncPackage2::CallCompletionCallbacks(EAsyncLoadingResult::Type LoadingResult)
{
	checkSlow(IsInGameThread());

	UPackage* LoadedPackage = (!bLoadHasFailed) ? LinkerRoot : nullptr;
	for (FCompletionCallback& CompletionCallback : CompletionCallbacks)
	{
		CompletionCallback->ExecuteIfBound(Desc.UPackageName, LoadedPackage, LoadingResult);
	}
	CompletionCallbacks.Empty();
}

UPackage* FAsyncPackage2::GetLoadedPackage()
{
	UPackage* LoadedPackage = (!bLoadHasFailed) ? LinkerRoot : nullptr;
	return LoadedPackage;
}

void FAsyncPackage2::Cancel()
{
	// Call any completion callbacks specified.
	bLoadHasFailed = true;
	const EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Canceled;
	CallCompletionCallbacks(Result);

	if (LinkerRoot)
	{
		if (bCreatedLinkerRoot)
		{
			LinkerRoot->ClearFlags(RF_WasLoaded);
			LinkerRoot->bHasBeenFullyLoaded = false;
			LinkerRoot->Rename(*MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass()).ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional);
		}
	}
}

void FAsyncPackage2::AddCompletionCallback(TUniquePtr<FLoadPackageAsyncDelegate>&& Callback)
{
	// This is to ensure that there is no one trying to subscribe to a already loaded package
	//check(!bLoadHasFinished && !bLoadHasFailed);
	CompletionCallbacks.Emplace(MoveTemp(Callback));
}

int32 FAsyncLoadingThread2::LoadPackage(const FPackagePath& InPackagePath, FName InCustomName, FLoadPackageAsyncDelegate InCompletionDelegate, EPackageFlags InPackageFlags, int32 InPIEInstanceID, int32 InPackagePriority, const FLinkerInstancingContext* InInstancingContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackage);

	const FName PackageNameToLoad = InPackagePath.GetPackageFName();
	if (InCustomName == PackageNameToLoad)
	{
		InCustomName = NAME_None;
	}
	if (FCoreDelegates::OnAsyncLoadPackage.IsBound())
	{
		const FName PackageName = InCustomName.IsNone() ? PackageNameToLoad : InCustomName;
		FCoreDelegates::OnAsyncLoadPackage.Broadcast(PackageName.ToString());
	}

	// Generate new request ID and add it immediately to the global request list (it needs to be there before we exit
	// this function, otherwise it would be added when the packages are being processed on the async thread).
	const int32 RequestId = IAsyncPackageLoader::GetNextRequestId();
	TRACE_LOADTIME_BEGIN_REQUEST(RequestId);
	AddPendingRequest(RequestId);

	// Allocate delegate on Game Thread, it is not safe to copy delegates by value on other threads
	TUniquePtr<FLoadPackageAsyncDelegate> CompletionDelegate = InCompletionDelegate.IsBound()
		? MakeUnique<FLoadPackageAsyncDelegate>(MoveTemp(InCompletionDelegate))
		: TUniquePtr<FLoadPackageAsyncDelegate>();

	PackageRequestQueue.Enqueue(FPackageRequest::Create(RequestId, InPackageFlags, InPIEInstanceID, InPackagePriority, InInstancingContext, InPackagePath, InCustomName, MoveTemp(CompletionDelegate)));
	++QueuedPackagesCounter;
	++PackagesWithRemainingWorkCounter;

	TRACE_COUNTER_SET(AsyncLoadingQueuedPackages, QueuedPackagesCounter);
	TRACE_COUNTER_SET(AsyncLoadingPackagesWithRemainingWork, PackagesWithRemainingWorkCounter);

	AltZenaphore.NotifyOne();

	return RequestId;
}

void FAsyncLoadingThread2::QueueMissingPackage(FAsyncPackageDesc2& PackageDesc, TUniquePtr<FLoadPackageAsyncDelegate>&& PackageLoadedDelegate)
{
	const FName FailedPackageName = PackageDesc.UPackageName;

	static TSet<FName> SkippedPackages;
	bool bIsAlreadySkipped = false;

	SkippedPackages.Add(FailedPackageName, &bIsAlreadySkipped);

	if (!bIsAlreadySkipped)
	{
		UE_LOG(LogStreaming, Warning,
			TEXT("LoadPackage: SkipPackage: %s (0x%llX) - The package to load does not exist on disk or in the loader"),
			*FailedPackageName.ToString(), PackageDesc.PackageIdToLoad.ValueForDebugging());
	}

	if (PackageLoadedDelegate.IsValid())
	{
		FScopeLock LockMissingPackages(&FailedPackageRequestsCritical);
		FailedPackageRequests.Add(FFailedPackageRequest
		{
			PackageDesc.RequestID,
			FailedPackageName,
			MoveTemp(PackageLoadedDelegate)
		});
	}
	else
	{
		RemovePendingRequests(TArrayView<int32>(&PackageDesc.RequestID, 1));
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
	TickAsyncLoadingFromGameThread(ThreadState, bUseTimeLimit, bUseFullTimeLimit, TimeLimit, INDEX_NONE, bDidSomething);
	return IsAsyncLoadingPackages() ? EAsyncPackageState::TimeOut : EAsyncPackageState::Complete;
}

void FAsyncLoadingThread2::FlushLoading(int32 RequestId)
{
	if (IsAsyncLoadingPackages())
	{
		// Flushing async loading while loading is suspend will result in infinite stall
		UE_CLOG(bSuspendRequested, LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended"));

		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

		if (RequestId != INDEX_NONE && !ContainsRequestID(RequestId))
		{
			return;
		}

		FCoreDelegates::OnAsyncLoadingFlush.Broadcast();

		double StartTime = FPlatformTime::Seconds();
		double LogFlushTime = StartTime;

		FAsyncPackage2* CurrentlyExecutingPackage = nullptr;
		if (GameThreadState->CurrentlyExecutingEventNode)
		{
			UE_CLOG(RequestId == INDEX_NONE, LogStreaming, Fatal, TEXT("Flushing async loading while creating, serializing or postloading an object is not permitted"));
			CurrentlyExecutingPackage = GameThreadState->CurrentlyExecutingEventNode->GetPackage();
			GameThreadState->PackagesExcludedFromStateDependencyChecks.Push(CurrentlyExecutingPackage);
		}

		FAsyncLoadingSyncLoadContext* SyncLoadContext = nullptr;
		if (RequestId != INDEX_NONE && GOnlyProcessRequiredPackagesWhenSyncLoading)
		{
			SyncLoadContext = new FAsyncLoadingSyncLoadContext(RequestId);
			SyncLoadContext->RequestingPackageDebug = CurrentlyExecutingPackage;
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
				EAsyncPackageState::Type Result = TickAsyncLoadingFromGameThread(*GameThreadState, false, false, 0, RequestId, bDidSomething);
				if (RequestId != INDEX_NONE && !ContainsRequestID(RequestId))
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
					// Update the heartbeat and sleep. If we're not multithreading, the heartbeat is updated after each package has been processed
					FThreadHeartBeat::Get().HeartBeat();
					FPlatformProcess::SleepNoStats(0.0001f);

					// Flush logging when runing cook-on-the-fly and waiting for packages
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
			SyncLoadContext->ReleaseRef();
			GameThreadState->SyncLoadContextStack.Pop();
		}

		if (CurrentlyExecutingPackage)
		{
			check(GameThreadState->PackagesExcludedFromStateDependencyChecks.Top() == CurrentlyExecutingPackage);
			GameThreadState->PackagesExcludedFromStateDependencyChecks.Pop();
		}

		check(RequestId != INDEX_NONE || !IsAsyncLoadingPackages());
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
	UE_CLOG(bSuspendRequested, LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended"));

	bool bUseTimeLimit = TimeLimit > 0.0f;
	double TimeLoadingPackage = 0.0f;

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

#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
PRAGMA_ENABLE_OPTIMIZATION
#endif
