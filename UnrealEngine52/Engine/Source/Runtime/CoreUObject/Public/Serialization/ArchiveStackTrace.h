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
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FLinkerLoad;
class FProperty;
class FUObjectThreadContext;
class UObject;
struct FObjectExport;
struct FObjectImport;
struct FUObjectSerializeContext;

/** Structure that holds stats from comparing two packages */
struct COREUOBJECT_API FArchiveDiffStats
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
class COREUOBJECT_API FArchiveStackTraceIgnoreScope
{
	const bool bIgnore;
public:
	FArchiveStackTraceIgnoreScope(bool bInIgnore = true);
	~FArchiveStackTraceIgnoreScope();
};

/**
 * Disables collecting both offsets and stack traces when collecting serialize callstacks.
 * Typically used when appending data from one stack tracing archive to another.
 */
class COREUOBJECT_API FArchiveStackTraceDisabledScope
{
public:
	FArchiveStackTraceDisabledScope();
	~FArchiveStackTraceDisabledScope();
};

struct COREUOBJECT_API FArchiveDiffInfo
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

class COREUOBJECT_API FArchiveDiffMap : public TArray<FArchiveDiffInfo>
{
public:
	bool ContainsOffset(int64 Offset) const
	{
		for (const FArchiveDiffInfo& Diff : *this)
		{
			if (Diff.Offset <= Offset && Offset < (Diff.Offset + Diff.Size))
			{
				return true;
			}
		}

		return false;
	}
};

/** Holds offsets to captured callstacks. */
class COREUOBJECT_API FArchiveCallstacks
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
	
	explicit FArchiveCallstacks(UObject* InAsset);

	/** Returns the asset class name. */
	FName GetAssetClass() const;

	/** Returns the total number of callstacks. */
	int32 Num() const
	{
		return CallstackAtOffsetMap.Num();
	}

	/** Capture and append the current callstack. */
	void Add(
		int64 Offset,
		int64 Length,
		UObject* SerializedObject,
		FProperty* SerializedProperty,
		TArrayView<const FName> DebugDataStack,
		bool bIsCollectingCallstacks,
		bool bCollectCurrentCallstack,
		int32 StackIgnoreCount);

	/** Append other callstacks. */
	void Append(const FArchiveCallstacks& Other, int64 Offset = 0);

	/** Finds a callstack associated with data at the specified offset */
	int32 GetCallstackIndexAtOffset(int64 Offset, int32 MinOffsetIndex = 0) const;

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
	ANSICHAR* AddUniqueCallstack(bool bIsCollectingCallstacks, UObject* SerializedObject, FProperty* SerializedProperty, uint32& OutCallstackCRC);

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

/** Archive proxy that captures callstacks for each serialize call. */
class COREUOBJECT_API FArchiveStackTraceWriter
	: public FArchiveProxy
{
public:
	FArchiveStackTraceWriter(
		FArchive& InInner,
		FArchiveCallstacks& InCallstacks,
		const FArchiveDiffMap* InDiffMap = nullptr,
		int64 InDiffMapStartOffset = 0);

	FORCENOINLINE virtual void Serialize(void* Data, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	virtual FUObjectSerializeContext* GetSerializeContext() override;
	
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

	/** Compares two packages and logs the differences and calltacks. */
	static void Compare(
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
	static bool GenerateDiffMap(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FArchiveCallstacks& Callstacks,
		int32 MaxDiffsToFind,
		FArchiveDiffMap& OutDiffMap);

	/** Logs any mismatching header data. */
	static void DumpPackageHeaderDiffs(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FString& AssetFilename,
		const int32 MaxDiffsToLog);

	/** Returns a new linker for loading the specified package. */
	static FLinkerLoad* CreateLinkerForPackage(
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
};

/**
 * Memory backed stack trace writer.
 */
class COREUOBJECT_API FArchiveStackTraceMemoryWriter final
	: public FLargeMemoryWriter
{
public:
	FArchiveStackTraceMemoryWriter(
		FArchiveCallstacks& Callstacks,
		const FArchiveDiffMap* DiffMap = nullptr,
		const int64 DiffMapOffset = 0,
		const int64 PreAllocateBytes = 0,
		bool bIsPersistent = false,
		const TCHAR* Filename = nullptr);

	FORCENOINLINE virtual void Serialize(void* Memory, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	virtual FUObjectSerializeContext* GetSerializeContext() override;

private:
	FArchiveStackTraceWriter StackTraceWriter;
};

/**
 * Archive that stores a callstack for each of the Serialize calls and has the ability to compare itself to an existing
 * package on disk and dump all the differences to log.
 */
class COREUOBJECT_API FArchiveStackTrace
	: public FLargeMemoryWriter
{
public:
	using FPackageData = FArchiveStackTraceWriter::FPackageData;

	FArchiveStackTrace(UObject* InAsset, const TCHAR* InFilename, bool bInCollectCallstacks = true, const FArchiveDiffMap* InDiffMap = nullptr);
	virtual ~FArchiveStackTrace();

	FArchiveCallstacks& GetCallstacks()
	{
		return Callstacks;
	}

	const FArchiveCallstacks& GetCallstacks() const
	{
		return Callstacks;
	}

	FORCENOINLINE virtual void Serialize(void* Memory, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	virtual FUObjectSerializeContext* GetSerializeContext() override;

	/** Compares this archive with the given bytes from disk or FPackageData. Dumps all differences to log. */
	void CompareWith(const TCHAR* InFilename, const int64 TotalHeaderSize, const TCHAR* CallstackCutoffText,
		const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats);
	void CompareWith(const FPackageData& SourcePackage, const TCHAR* FileDisplayName, const int64 TotalHeaderSize,
		const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats);

	/** Generates a map of all differences between this archive and the given bytes from disk or FPackageData. */
	bool GenerateDiffMap(const TCHAR* InFilename, int64 TotalHeaderSize, int32 MaxDiffsToFind, FArchiveDiffMap& OutDiffMap);
	bool GenerateDiffMap(const FPackageData& SourcePackage, int64 TotalHeaderSize, int32 MaxDiffsToFind,
		FArchiveDiffMap& OutDiffMap);

	/** Compares the provided buffer with the given bytes from disk or FPackageData. */
	static bool IsIdentical(const TCHAR* InFilename, int64 BufferSize, const uint8* BufferData);
	static bool IsIdentical(const FPackageData& SourcePackage, int64 BufferSize, const uint8* BufferData);

	/** Helper function to load package contents into memory. Supports EDL packages. */
	static bool LoadPackageIntoMemory(const TCHAR* InFilename, FPackageData& OutPackageData,
		TUniquePtr<uint8>& OutLoadedBytes);

private:
	FArchiveCallstacks Callstacks;
	FArchiveStackTraceWriter StackTraceWriter;
};

class COREUOBJECT_API FArchiveStackTraceReader : public FLargeMemoryReader
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

	FArchiveStackTraceReader(const TCHAR* InFilename, const uint8* InData, const int64 Num);

	virtual void Serialize(void* OutData, int64 Num) override;
	const TArray<FSerializeData>& GetSerializeTrace() const
	{
		return SerializeTrace;
	}
	static FArchiveStackTraceReader* CreateFromFile(const TCHAR* InFilename);
};
