// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Serialization/Archive.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FLinkerLoad;
class FProperty;
class FUObjectThreadContext;
class UObject;
struct FUObjectSerializeContext;

/** Structure that holds stats from comparing two packages */
struct FArchiveDiffStats
{
	/** Size of all of the differences between two packages */
	int64 DiffSize;
	/** Number of differences between two packages */
	int64 NumDiffs;
	/** Size of the source package file (the one we compared against) */
	int64 OriginalFileTotalSize;
	/** Size of the new package file */
	int64 NewFileTotalSize;

	FArchiveDiffStats()
		: DiffSize(0)
		, NumDiffs(0)
		, OriginalFileTotalSize(0)
		, NewFileTotalSize(0)
	{}
};

/** Ignores saving the stack trace when collecting serialize offsets .*/
class FArchiveStackTraceIgnoreScope
{
	const bool bIgnore;
public:
	COREUOBJECT_API FArchiveStackTraceIgnoreScope(bool bInIgnore = true);
	COREUOBJECT_API ~FArchiveStackTraceIgnoreScope();
};

/**
 * Disables collecting both offsets and stack traces when collecting serialize callstacks.
 * Typically used when appending data from one stack tracing archive to another.
 */
class FArchiveStackTraceDisabledScope
{
public:
	COREUOBJECT_API FArchiveStackTraceDisabledScope();
	COREUOBJECT_API ~FArchiveStackTraceDisabledScope();
};

namespace UE::ArchiveStackTrace
{

struct FPackageData
{
	uint8* Data = nullptr;
	int64 Size = 0;
	int64 HeaderSize = 0;
	int64 StartOffset = 0;
};

struct FDeleteByFree
{
	void operator()(void* Ptr) const
	{
		FMemory::Free(Ptr);
	}
};

/** Helper function to load package contents into memory. Supports EDL packages. */
COREUOBJECT_API bool LoadPackageIntoMemory(const TCHAR* InFilename,
	FPackageData& OutPackageData, TUniquePtr<uint8, FDeleteByFree>& OutLoadedBytes);

COREUOBJECT_API void ForceKillPackageAndLinker(FLinkerLoad* Linker);
COREUOBJECT_API bool ShouldIgnoreDiff();
COREUOBJECT_API bool ShouldBypassDiff();

} // namespace UE::ArchiveStackTrace

struct
// Deprecated: 5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveDiffInfo
{
	int64 Offset;
	int64 Size;
	FArchiveDiffInfo()
		: Offset(0)
		, Size(0)
	{
	}
	FArchiveDiffInfo(int64 InOffset, int64 InSize)
		: Offset(InOffset)
		, Size(InSize)
	{
	}
	bool operator==(const FArchiveDiffInfo& InOther) const
	{
		return Offset == InOther.Offset;
	}
	bool operator<(const FArchiveDiffInfo& InOther) const
	{
		return Offset < InOther.Offset;
	}
	friend FArchive& operator << (FArchive& Ar, FArchiveDiffInfo& InDiffInfo)
	{
		Ar << InDiffInfo.Offset;
		Ar << InDiffInfo.Size;
		return Ar;
	}
};

class 
UE_DEPRECATED(5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveDiffMap : public TArray<FArchiveDiffInfo>
{
public:
	bool ContainsOffset(int64 Offset) const;
};

class
UE_DEPRECATED(5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveCallstacks
{
public:
	/** Offset and callstack pair */
	struct FCallstackAtOffset
	{
		/** Offset of a Serialize call */
		int64 Offset = -1;
		/** Callstack CRC for the Serialize call */
		uint32 Callstack = 0;
		/** Collected inside of skip scope */
		bool bIgnore = false;
	};

	/** Struct to hold the actual Serialize call callstack and any associated data */
	struct FCallstackData
	{
		/** Full callstack */
		TUniquePtr<ANSICHAR[]> Callstack;
		/** Full name of the currently serialized object */
		FString SerializedObjectName;
		/** The currently serialized property */
		FProperty* SerializedProp = nullptr;
		/** Name of the currently serialized property */
		FString SerializedPropertyName;

		FCallstackData() = default;
		FCallstackData(TUniquePtr<ANSICHAR[]>&& InCallstack, UObject* InSerializedObject, FProperty* InSerializedProperty);
		FCallstackData(FCallstackData&&) = default;
		FCallstackData(const FCallstackData&) = delete;

		FCallstackData& operator=(FCallstackData&&) = default;
		FCallstackData& operator=(const FCallstackData&) = delete;

		/** Converts the callstack and associated data to human readable string */
		FString ToString(const TCHAR* CallstackCutoffText) const;

		/** Clone the callstack data */
		FCallstackData Clone() const;
	};
	
	COREUOBJECT_API explicit FArchiveCallstacks(UObject* InAsset);

	/** Returns the asset class name. */
	COREUOBJECT_API FName GetAssetClass() const;

	/** Returns the total number of callstacks. */
	int32 Num() const
	{
		return CallstackAtOffsetMap.Num();
	}

	/** Capture and append the current callstack. */
	COREUOBJECT_API void Add(
		int64 Offset,
		int64 Length,
		UObject* SerializedObject,
		FProperty* SerializedProperty,
		TArrayView<const FName> DebugDataStack,
		bool bIsCollectingCallstacks,
		bool bCollectCurrentCallstack,
		int32 StackIgnoreCount);

	/** Append other callstacks. */
	COREUOBJECT_API void Append(const FArchiveCallstacks& Other, int64 Offset = 0);

	/** Finds a callstack associated with data at the specified offset */
	COREUOBJECT_API int32 GetCallstackIndexAtOffset(int64 Offset, int32 MinOffsetIndex = 0) const;

	/** Finds a callstack associated with data at the specified offset */
	const FCallstackAtOffset& GetCallstack(int32 CallstackIndex) const
	{
		return CallstackAtOffsetMap[CallstackIndex];
	}
	
	const FCallstackData& GetCallstackData(const FCallstackAtOffset& CallstackOffset) const
	{
		return UniqueCallstacks[CallstackOffset.Callstack];
	}

	/** Returns the size of serialized data at the specified offset. */
	int64 GetSerializedDataSizeForOffsetIndex(int32 InOffsetIndex) const
	{
		if (InOffsetIndex < CallstackAtOffsetMap.Num() - 1)
		{
			return CallstackAtOffsetMap[InOffsetIndex + 1].Offset - CallstackAtOffsetMap[InOffsetIndex].Offset;
		}
		else
		{
			return TotalSize - CallstackAtOffsetMap[InOffsetIndex].Offset;
		}
	}

	/** Returns total serialized bytes. */
	int64 TotalCapturedSize() const
	{
		return TotalSize;
	}

private:
	/** Adds a unique callstack to UniqueCallstacks map */
	COREUOBJECT_API ANSICHAR* AddUniqueCallstack(bool bIsCollectingCallstacks, UObject* SerializedObject, FProperty* SerializedProperty, uint32& OutCallstackCRC);

	/** The asset being serialized */
	UObject* Asset;
	/** List of offsets and their respective callstacks */
	TArray<FCallstackAtOffset> CallstackAtOffsetMap;
	/** Contains all unique callstacks for all Serialize calls */
	TMap<uint32, FCallstackData> UniqueCallstacks;
	/** Optimizes callstack comparison. If true the current and the last callstack should be compared as it may have changed */
	bool bCallstacksDirty;
	/** Maximum size of the stack trace */
	const SIZE_T StackTraceSize;
	/** Buffer for getting the current stack trace */
	TUniquePtr<ANSICHAR[]> StackTrace;
	/** Callstack associated with the previous Serialize call */
	ANSICHAR* LastSerializeCallstack;
	/** Total serialized bytes */
	int64 TotalSize;
};

class
UE_DEPRECATED(5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveStackTraceWriter
	: public FArchiveProxy
{
public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;

	COREUOBJECT_API FArchiveStackTraceWriter(
		FArchive& InInner,
		FArchiveCallstacks& InCallstacks,
		const FArchiveDiffMap* InDiffMap = nullptr,
		int64 InDiffMapStartOffset = 0);

	COREUOBJECT_API virtual ~FArchiveStackTraceWriter() override;

	COREUOBJECT_API FORCENOINLINE virtual void Serialize(void* Data, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	COREUOBJECT_API virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	COREUOBJECT_API virtual FUObjectSerializeContext* GetSerializeContext() override;
	
#if WITH_EDITOR
	virtual void PushDebugDataString(const FName& DebugData) override
	{
		DebugDataStack.Push(DebugData);
	}

	virtual void PopDebugDataString() override
	{
		DebugDataStack.Pop();
	}
#endif

	const FArchiveDiffMap& GetDiffMap() const
	{
		static FArchiveDiffMap Empty;
		return DiffMap != nullptr ? *DiffMap : Empty;
	}

	void SetDisableInnerArchive(bool bDisable)
	{
		bInnerArchiveDisabled = bDisable;
	}

	int32 GetStackIgnoreCount() const { return StackIgnoreCount; }
	void SetStackIgnoreCount(const int32 IgnoreCount) { StackIgnoreCount = IgnoreCount; }

	struct FPackageData
	{
		uint8* Data = nullptr;
		int64 Size = 0;
		int64 HeaderSize = 0;
		int64 StartOffset = 0;
	};

	using EPackageHeaderFormat = ICookedPackageWriter::EPackageHeaderFormat;

	/** Compares two packages and logs the differences and calltacks. */
	static COREUOBJECT_API void Compare(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FArchiveCallstacks& Callstacks,
		const FArchiveDiffMap& DiffMap,
		const TCHAR* AssetFilename,
		const TCHAR* CallstackCutoffText,
		const int64 MaxDiffsToLog,
		int32& InOutDiffsLogged,
		TMap<FName, FArchiveDiffStats>& OutStats,
		bool bSuppressLogging = false);

	/** Creates map with mismatching callstacks. */
	static COREUOBJECT_API bool GenerateDiffMap(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FArchiveCallstacks& Callstacks,
		int32 MaxDiffsToFind,
		FArchiveDiffMap& OutDiffMap);

	/** Logs any mismatching header data. */
	static COREUOBJECT_API void DumpPackageHeaderDiffs(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FString& AssetFilename,
		const int32 MaxDiffsToLog,
		const EPackageHeaderFormat PackageHeaderFormat = EPackageHeaderFormat::PackageFileSummary);

	/** Returns a new linker for loading the specified package. */
	static COREUOBJECT_API FLinkerLoad* CreateLinkerForPackage(
		FUObjectSerializeContext* LoadContext,
		const FString& InPackageName,
		const FString& InFilename,
		const FPackageData& PackageData);

private:
	FArchiveCallstacks& Callstacks;
	const FArchiveDiffMap* DiffMap;
	TRefCountPtr<FUObjectSerializeContext> SerializeContext;
#if WITH_EDITOR
	TArray<FName> DebugDataStack;
#endif
	int64 DiffMapOffset;
	int32 StackIgnoreCount = 2;
	bool bInnerArchiveDisabled;

PRAGMA_ENABLE_DEPRECATION_WARNINGS;

};

class
UE_DEPRECATED(5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveStackTraceMemoryWriter final
	: public FLargeMemoryWriter
{
public:

PRAGMA_DISABLE_DEPRECATION_WARNINGS;

	COREUOBJECT_API FArchiveStackTraceMemoryWriter(
		FArchiveCallstacks& Callstacks,
		const FArchiveDiffMap* DiffMap = nullptr,
		const int64 DiffMapOffset = 0,
		const int64 PreAllocateBytes = 0,
		bool bIsPersistent = false,
		const TCHAR* Filename = nullptr);

	COREUOBJECT_API FORCENOINLINE virtual void Serialize(void* Memory, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	COREUOBJECT_API virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	COREUOBJECT_API virtual FUObjectSerializeContext* GetSerializeContext() override;

private:
	FArchiveStackTraceWriter StackTraceWriter;

PRAGMA_ENABLE_DEPRECATION_WARNINGS;
};

/**
 * Archive that stores a callstack for each of the Serialize calls and has the ability to compare itself to an existing
 * package on disk and dump all the differences to log.
 */
class
UE_DEPRECATED(5.3, "FArchiveStackTrace was only used by DiffPackageWriter, and has been moved into a private helper class. Contact Epic if you need this class for another reason.")
FArchiveStackTrace
	: public FLargeMemoryWriter
{
public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;

	using FPackageData = FArchiveStackTraceWriter::FPackageData;

	COREUOBJECT_API FArchiveStackTrace(UObject* InAsset, const TCHAR* InFilename, bool bInCollectCallstacks = true, const FArchiveDiffMap* InDiffMap = nullptr);
	COREUOBJECT_API virtual ~FArchiveStackTrace();

	FArchiveCallstacks& GetCallstacks()
	{
		return Callstacks;
	}

	const FArchiveCallstacks& GetCallstacks() const
	{
		return Callstacks;
	}

	COREUOBJECT_API FORCENOINLINE virtual void Serialize(void* Memory, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	COREUOBJECT_API virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	COREUOBJECT_API virtual FUObjectSerializeContext* GetSerializeContext() override;

	/** Compares this archive with the given bytes from disk or FPackageData. Dumps all differences to log. */
	COREUOBJECT_API void CompareWith(const TCHAR* InFilename, const int64 TotalHeaderSize, const TCHAR* CallstackCutoffText,
		const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats);
	COREUOBJECT_API void CompareWith(const FPackageData& SourcePackage, const TCHAR* FileDisplayName, const int64 TotalHeaderSize,
		const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats,
		const FArchiveStackTraceWriter::EPackageHeaderFormat PackageHeaderFormat = FArchiveStackTraceWriter::EPackageHeaderFormat::PackageFileSummary);

	/** Generates a map of all differences between this archive and the given bytes from disk or FPackageData. */
	COREUOBJECT_API bool GenerateDiffMap(const TCHAR* InFilename, int64 TotalHeaderSize, int32 MaxDiffsToFind, FArchiveDiffMap& OutDiffMap);
	COREUOBJECT_API bool GenerateDiffMap(const FPackageData& SourcePackage, int64 TotalHeaderSize, int32 MaxDiffsToFind,
		FArchiveDiffMap& OutDiffMap);

	/** Compares the provided buffer with the given bytes from disk or FPackageData. */
	static COREUOBJECT_API bool IsIdentical(const TCHAR* InFilename, int64 BufferSize, const uint8* BufferData);
	static COREUOBJECT_API bool IsIdentical(const FPackageData& SourcePackage, int64 BufferSize, const uint8* BufferData);

	/** Helper function to load package contents into memory. Supports EDL packages. */
	static COREUOBJECT_API bool LoadPackageIntoMemory(const TCHAR* InFilename, FPackageData& OutPackageData,
		TUniquePtr<uint8, UE::ArchiveStackTrace::FDeleteByFree>& OutLoadedBytes);

private:
	FArchiveCallstacks Callstacks;
	FArchiveStackTraceWriter StackTraceWriter;

PRAGMA_ENABLE_DEPRECATION_WARNINGS;
};

class FArchiveStackTraceReader : public FLargeMemoryReader
{
public:
	struct FSerializeData
	{
		FSerializeData()
			: Offset(0)
			, Size(0)
			, Count(0)
			, Object(nullptr)
			, PropertyName(NAME_None)
		{}
		FSerializeData(int64 InOffset, int64 InSize, UObject* InObject, FProperty* InProperty);
		int64 Offset;
		int64 Size;
		int64 Count;
		UObject* Object;
		FName PropertyName;
		FString FullPropertyName;

		bool IsContiguousSerialization(const FSerializeData& Other) const
		{
			// Return whether this and other are neighboring bits of data for the serialization of the same instance of an object\property
			return Object == Other.Object && PropertyName == Other.PropertyName &&
				(Offset == Other.Offset || (Offset + Size) == Other.Offset); // This is to merge contiguous blocks
		}
	};
private:
	TArray<FSerializeData> SerializeTrace;
	/** Cached thread context */
	FUObjectThreadContext& ThreadContext;
public:

	COREUOBJECT_API FArchiveStackTraceReader(const TCHAR* InFilename, const uint8* InData, const int64 Num);

	COREUOBJECT_API virtual void Serialize(void* OutData, int64 Num) override;
	const TArray<FSerializeData>& GetSerializeTrace() const
	{
		return SerializeTrace;
	}
	static COREUOBJECT_API FArchiveStackTraceReader* CreateFromFile(const TCHAR* InFilename);
};
