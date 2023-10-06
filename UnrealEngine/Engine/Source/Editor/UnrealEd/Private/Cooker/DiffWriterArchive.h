// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Logging/LogVerbosity.h"
#include "Serialization/Archive.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Templates/RefCounting.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

class FLinkerLoad;
class FProperty;
class FUObjectThreadContext;
class UObject;
struct FUObjectSerializeContext;


namespace UE::DiffWriterArchive
{

extern const TCHAR* const IndentToken;
extern const TCHAR* const NewLineToken;
typedef TUniqueFunction<void(ELogVerbosity::Type, FStringView)> FMessageCallback;

}

struct FDiffWriterDiffInfo
{
	int64 Offset;
	int64 Size;
	FDiffWriterDiffInfo()
		: Offset(0)
		, Size(0)
	{
	}
	FDiffWriterDiffInfo(int64 InOffset, int64 InSize)
		: Offset(InOffset)
		, Size(InSize)
	{
	}
	bool operator==(const FDiffWriterDiffInfo& InOther) const
	{
		return Offset == InOther.Offset;
	}
	bool operator<(const FDiffWriterDiffInfo& InOther) const
	{
		return Offset < InOther.Offset;
	}
	friend FArchive& operator << (FArchive& Ar, FDiffWriterDiffInfo& InDiffInfo)
	{
		Ar << InDiffInfo.Offset;
		Ar << InDiffInfo.Size;
		return Ar;
	}
};

class FDiffWriterDiffMap : public TArray<FDiffWriterDiffInfo>
{
public:
	bool ContainsOffset(int64 Offset) const
	{
		for (const FDiffWriterDiffInfo& Diff : *this)
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
class FDiffWriterCallstacks
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
	
	explicit FDiffWriterCallstacks(UObject* InAsset);

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
	void Append(const FDiffWriterCallstacks& Other, int64 Offset = 0);

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
class FDiffWriterArchiveWriter
	: public FArchiveProxy
{
public:
	FDiffWriterArchiveWriter(
		FArchive& InInner,
		FDiffWriterCallstacks& InCallstacks,
		const FDiffWriterDiffMap* InDiffMap = nullptr,
		int64 InDiffMapStartOffset = 0);

	virtual ~FDiffWriterArchiveWriter() override;

	FORCENOINLINE virtual void Serialize(void* Data, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	virtual FUObjectSerializeContext* GetSerializeContext() override;
	
	virtual void PushDebugDataString(const FName& DebugData) override
	{
		DebugDataStack.Push(DebugData);
	}

	virtual void PopDebugDataString() override
	{
		DebugDataStack.Pop();
	}

	const FDiffWriterDiffMap& GetDiffMap() const
	{
		static FDiffWriterDiffMap Empty;
		return DiffMap != nullptr ? *DiffMap : Empty;
	}

	void SetDisableInnerArchive(bool bDisable)
	{
		bInnerArchiveDisabled = bDisable;
	}

	int32 GetStackIgnoreCount() const { return StackIgnoreCount; }
	void SetStackIgnoreCount(const int32 IgnoreCount) { StackIgnoreCount = IgnoreCount; }

	using FPackageData = UE::ArchiveStackTrace::FPackageData;

	using EPackageHeaderFormat = ICookedPackageWriter::EPackageHeaderFormat;

	/** Compares two packages and logs the differences and calltacks. */
	static void Compare(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FDiffWriterCallstacks& Callstacks,
		const FDiffWriterDiffMap& DiffMap,
		const TCHAR* AssetFilename,
		const TCHAR* CallstackCutoffText,
		const int64 MaxDiffsToLog,
		int32& InOutDiffsLogged,
		TMap<FName, FArchiveDiffStats>& OutStats,
		const UE::DiffWriterArchive::FMessageCallback& MessageCallback,
		bool bSuppressLogging = false);

	/** Creates map with mismatching callstacks. */
	static bool GenerateDiffMap(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FDiffWriterCallstacks& Callstacks,
		int32 MaxDiffsToFind,
		FDiffWriterDiffMap& OutDiffMap);

	/** Logs any mismatching header data. */
	static void DumpPackageHeaderDiffs(
		const FPackageData& SourcePackage,
		const FPackageData& DestPackage,
		const FString& AssetFilename,
		const int32 MaxDiffsToLog,
		const EPackageHeaderFormat PackageHeaderFormat,
		const UE::DiffWriterArchive::FMessageCallback& MessageCallback);

	/** Returns a new linker for loading the specified package. */
	static FLinkerLoad* CreateLinkerForPackage(
		FUObjectSerializeContext* LoadContext,
		const FString& InPackageName,
		const FString& InFilename,
		const FPackageData& PackageData);

private:
	FDiffWriterCallstacks& Callstacks;
	const FDiffWriterDiffMap* DiffMap;
	TRefCountPtr<FUObjectSerializeContext> SerializeContext;
	TArray<FName> DebugDataStack;
	int64 DiffMapOffset;
	int32 StackIgnoreCount = 2;
	bool bInnerArchiveDisabled;
};

/**
 * Memory backed stack trace writer.
 */
class FDiffWriterArchiveMemoryWriter final
	: public FLargeMemoryWriter
{
public:
	FDiffWriterArchiveMemoryWriter(
		FDiffWriterCallstacks& Callstacks,
		const FDiffWriterDiffMap* DiffMap = nullptr,
		const int64 DiffMapOffset = 0,
		const int64 PreAllocateBytes = 0,
		bool bIsPersistent = false,
		const TCHAR* Filename = nullptr);

	FORCENOINLINE virtual void Serialize(void* Memory, int64 Length) override; // FORCENOINLINE so it can be counted during StackTrace
	virtual void SetSerializeContext(FUObjectSerializeContext* Context) override;
	virtual FUObjectSerializeContext* GetSerializeContext() override;

private:
	FDiffWriterArchiveWriter StackTraceWriter;
};

/**
 * Archive that stores a callstack for each of the Serialize calls and has the ability to compare itself to an existing
 * package on disk and dump all the differences to log.
 */
class FDiffWriterArchive
	: public FLargeMemoryWriter
{
public:
	using FPackageData = FDiffWriterArchiveWriter::FPackageData;

	FDiffWriterArchive(UObject* InAsset, const TCHAR* InFilename, UE::DiffWriterArchive::FMessageCallback&& InMessageCallback,
		bool bInCollectCallstacks = true, const FDiffWriterDiffMap* InDiffMap = nullptr);
	virtual ~FDiffWriterArchive();

	FDiffWriterCallstacks& GetCallstacks()
	{
		return Callstacks;
	}

	const FDiffWriterCallstacks& GetCallstacks() const
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
		const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats,
		const FDiffWriterArchiveWriter::EPackageHeaderFormat PackageHeaderFormat = FDiffWriterArchiveWriter::EPackageHeaderFormat::PackageFileSummary);

	/** Generates a map of all differences between this archive and the given bytes from disk or FPackageData. */
	bool GenerateDiffMap(const TCHAR* InFilename, int64 TotalHeaderSize, int32 MaxDiffsToFind, FDiffWriterDiffMap& OutDiffMap);
	bool GenerateDiffMap(const FPackageData& SourcePackage, int64 TotalHeaderSize, int32 MaxDiffsToFind,
		FDiffWriterDiffMap& OutDiffMap);

	/** Compares the provided buffer with the given bytes from disk or FPackageData. */
	static bool IsIdentical(const TCHAR* InFilename, int64 BufferSize, const uint8* BufferData);
	static bool IsIdentical(const FPackageData& SourcePackage, int64 BufferSize, const uint8* BufferData);

private:
	FDiffWriterCallstacks Callstacks;
	FDiffWriterArchiveWriter StackTraceWriter;
	UE::DiffWriterArchive::FMessageCallback MessageCallback;
};
