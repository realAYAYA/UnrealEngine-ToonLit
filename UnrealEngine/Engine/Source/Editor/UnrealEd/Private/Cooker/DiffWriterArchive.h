// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Logging/LogVerbosity.h"
#include "PackageStoreOptimizer.h"
#include "Serialization/Archive.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Templates/RefCounting.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

class FDiffWriterArchiveTestsCallstacks;
class FLinkerLoad;
class FProperty;
class FUObjectThreadContext;
class UObject;
struct FUObjectSerializeContext;


namespace UE::DiffWriter
{

class FAccumulator;
class FDiffArchive;

typedef TUniqueFunction<void(ELogVerbosity::Type, FStringView)> FMessageCallback;
using EPackageHeaderFormat = ICookedPackageWriter::EPackageHeaderFormat;
using FPackageData = UE::ArchiveStackTrace::FPackageData;

extern const TCHAR* const IndentToken;
extern const TCHAR* const NewLineToken;

enum class EOffsetFrame
{
	Linker,
	Exports,
};

struct FDiffInfo
{
	int64 Offset;
	int64 Size;

	FDiffInfo()
		: Offset(0)
		, Size(0)
	{
	}
	FDiffInfo(int64 InOffset, int64 InSize)
		: Offset(InOffset)
		, Size(InSize)
	{
	}
	bool operator==(const FDiffInfo& InOther) const
	{
		return Offset == InOther.Offset;
	}
	bool operator<(const FDiffInfo& InOther) const
	{
		return Offset < InOther.Offset;
	}
	friend FArchive& operator << (FArchive& Ar, FDiffInfo& InDiffInfo)
	{
		Ar << InDiffInfo.Offset;
		Ar << InDiffInfo.Size;
		return Ar;
	}
};

class FDiffMap : public TArray<FDiffInfo>
{
public:
	bool ContainsOffset(int64 Offset) const
	{
		for (const FDiffInfo& Diff : *this)
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
class FCallstacks
{
public:
	/** Offset and callstack pair */
	struct FCallstackAtOffset
	{
		/** Offset of a Serialize call */
		int64 Offset = -1;
		/** Callstack CRC for the Serialize call */
		uint32 Callstack = 0;
		/** Collected inside of a scope that indicates diff should be recorded but logging should be suppressed */
		bool bSuppressLogging = false;
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
	
	FCallstacks();

	/** Returns the total number of callstacks. */
	int32 Num() const
	{
		return CallstackAtOffsetMap.Num();
	}
	void Reset();

	FORCENOINLINE void RecordSerialize(EOffsetFrame OffsetFrame, int64 CurrentOffset, int64 Length,
		const FAccumulator& Accumulator, FDiffArchive& Ar, int32 StackIgnoreCount);

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

	/**
	 * Remove offset->callstack entries reported for a range of offsets. Only removes entries that start within the
	 * range, does not remove entries that start before the range but end in or after it.
	 */
	void RemoveRange(int64 StartOffset, int64 Length);

	/** Append other offset->callstacks entries and callstacks they refer to. */
	void Append(const FCallstacks& Other, int64 OtherStartOffset);

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
			return EndOffset - CallstackAtOffsetMap[InOffsetIndex].Offset;
		}
	}

	int64 GetEndOffset() const
	{
		return EndOffset;
	}

private:
	/** Adds a unique callstack to UniqueCallstacks map */
	ANSICHAR* AddUniqueCallstack(bool bIsCollectingCallstacks, UObject* SerializedObject, FProperty* SerializedProperty, uint32& OutCallstackCRC);

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
	int64 EndOffset;
};

/** Global data (e.g. the FPackageId of every object in /Script) used during diffing */
struct FAccumulatorGlobals
{
public:
	// Zen variables
	TMap<FPackageObjectIndex, FPackageStoreOptimizer::FScriptObjectData> ScriptObjectsMap;

	// Shared variables
	ICookedPackageWriter* PackageWriter = nullptr;
	EPackageHeaderFormat Format = EPackageHeaderFormat::PackageFileSummary;
	bool bInitialized = false;

public:
	FAccumulatorGlobals(ICookedPackageWriter* InnerPackageWriter = nullptr);
	void Initialize(EPackageHeaderFormat Format);
};

/**
 * Collects the memory version of a saved package, compares it with an existing package on disk, and reports callstack
 * for the Serialize call at each offset where they differ.
 * 
 * It works by saving a package twice. The first pass collects the serialization offsets without the stack traces and
 * creates a FDiffMap, and records sizes necessary to remap offsets during Serialize to the final offset in the package
 * on disk. In the second pass, the diff map is read during each call to Serialize to decide whether we need to collect
 * the stack trace for that call.
 */
class FAccumulator : public FRefCountBase
{
public:
	FAccumulator(FAccumulatorGlobals& InGlobals, UObject* InAsset, FName InPackageName, int32 InMaxDiffsToLog,
		bool bInIgnoreHeaderDiffs, FMessageCallback&& InMessageCallback, EPackageHeaderFormat InPackageHeaderFormat);
	virtual ~FAccumulator();

	void OnFirstSaveComplete(FStringView InLooseFilePath, int64 InHeaderSize, int64 InPreTransformHeaderSize,
		ICookedPackageWriter::FPreviousCookedBytesData&& InPreviousPackageData);
	void OnSecondSaveComplete(int64 InHeaderSize);
	bool HasDifferences() const;

	/** Compares results from the second save with the previous cook results in PreviousPackagedata.  */
	void CompareWithPrevious(const TCHAR* CallstackCutoffText, TMap<FName,FArchiveDiffStats>& OutStats);

	void SetHeaderSize(int64 InHeaderSize);
	void SetCollectingCallstacks(bool bInCollectingCallstacks);
	FName GetAssetClass() const;
	bool IsWriterUsingPostSaveTransforms() const;

private:
	void GenerateDiffMapForSection(const FPackageData& SourcePackage, const FPackageData& DestPackage, bool& bOutSectionIdentical);
	void GenerateDiffMap();
	/** Compares two packages and logs the differences and calltacks. */
	void CompareWithPreviousForSection(const FPackageData& SourcePackage, const FPackageData& DestPackage,
		const TCHAR* CallstackCutoffText, int32& InOutLoggedDiffs,TMap<FName, FArchiveDiffStats>& OutStats,
		const FString& SectionFilename);

private:
	FCallstacks LinkerCallstacks;
	FCallstacks ExportsCallstacks;
	ICookedPackageWriter::FPreviousCookedBytesData PreviousPackageData;
	FDiffArchive* LinkerArchive = nullptr;
	FDiffArchive* ExportsArchive = nullptr;
	TArray<uint8> FirstSaveLinkerData;
	int64 FirstSaveLinkerSize = 0;
	FAccumulatorGlobals& Globals;

	FDiffMap DiffMap;
	FMessageCallback MessageCallback;
	FName PackageName;
	FString Filename;
	UObject* Asset = nullptr;
	int64 HeaderSize = 0;
	int64 PreTransformHeaderSize = 0;
	int32 MaxDiffsToLog = 5;
	EPackageHeaderFormat PackageHeaderFormat = EPackageHeaderFormat::PackageFileSummary;
	bool bFirstSaveComplete = false;
	bool bHasDifferences = false;
	bool bIgnoreHeaderDiffs = false;

	friend class FCallstacks;
	friend class FDiffArchive;
	friend class FDiffArchiveForLinker;
	friend class FDiffArchiveForExports;
	friend class ::FDiffWriterArchiveTestsCallstacks;
};

class FDiffArchive : public FLargeMemoryWriter
{
public:
	FDiffArchive(FAccumulator& InAccumulator);

	// FLargeMemoryWriterBase interface
	virtual FString GetArchiveName() const override;
	virtual void PushDebugDataString(const FName& DebugData) override;
	virtual void PopDebugDataString() override;

	// FDiffArchive interface
	FAccumulator& GetAccumulator() { return *Accumulator; }
	TArray<FName>& GetDebugDataStack();


protected:
	TArray<FName> DebugDataStack;
	TRefCountPtr<FAccumulator> Accumulator;
};

/**
 * The archive written to by SavePackage, includes the header and exports.
 */
class FDiffArchiveForLinker : public FDiffArchive
{
public:
	FDiffArchiveForLinker(FAccumulator& InAccumulator);
	~FDiffArchiveForLinker();

	// FLargeMemoryWriter interface
	FORCENOINLINE virtual void Serialize(void* InData, int64 Num) override; // FORCENOINLINE so it can be counted during StackTrace
};

/**
 * The archive written to when SavePackage is writing the Serialize blobs for exports.
 * When cooking, exports are serialized into a separate archive. We collect the serialization
 * callstack offsets and stack traces into a separate callstack collection and append it
 * at the proper offset to the overall callstacks for the entire linker archive.
 */
class FDiffArchiveForExports : public FDiffArchive
{
public:
	FDiffArchiveForExports(FAccumulator& InAccumulator);
	~FDiffArchiveForExports();

	// FLargeMemoryWriter interface
	FORCENOINLINE virtual void Serialize(void* InData, int64 Num) override; // FORCENOINLINE so it can be counted during StackTrace
};

} // namespace UE::DiffWriter