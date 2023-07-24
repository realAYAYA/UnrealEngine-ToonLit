// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS && WITH_EDITORONLY_DATA

#include "TestHarness.h"
#include "TestMacros/Assertions.h"
#include "Memory/SharedBuffer.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include <catch2/generators/catch_generators.hpp>

namespace UE::CoreUObject::Serialization::Tests
{

struct FBasicDiffState
{
	int32 FirstParam = 0;
	int32 SecondParam = 0;
	int32 ThirdParam = 0;

	FORCENOINLINE void Serialize(FArchive& Ar)
	{
		Ar << FirstParam;
		Ar << SecondParam;
		Ar << ThirdParam;
	}
};

FArchiveStackTraceWriter::FPackageData ToPackageData(FLargeMemoryWriter& Ar)
{
	return FArchiveStackTraceWriter::FPackageData
	{
		Ar.GetData(),
		Ar.TotalSize()
	};
}

TEST_CASE("CoreUObject::Serialization::ArchiveStackTrace::Callstacks", "[CoreUObject][Serialization]")
{
	const int32 MaxDiffsToLog = 1000;
	FArchiveCallstacks Callstacks(nullptr);

	FLargeMemoryWriter Memory;
	FBasicDiffState State = FBasicDiffState {1,2,3};
	FArchiveStackTraceWriter StackTraceWriter(Memory, Callstacks, 0);
	State.Serialize(StackTraceWriter);

	CHECK(Callstacks.Num() == 3);
	CHECK(Callstacks.TotalCapturedSize() == sizeof(FBasicDiffState));
	CHECK(Callstacks.GetCallstack(0).Offset == 0);
	CHECK(Callstacks.GetCallstack(0).Callstack == 0);
	CHECK(Callstacks.GetCallstack(0).bIgnore == false);
	CHECK(Callstacks.GetCallstack(1).Offset == 4);
	CHECK(Callstacks.GetCallstack(1).Callstack == 0);
	CHECK(Callstacks.GetCallstack(1).bIgnore == false);
	CHECK(Callstacks.GetCallstack(2).Offset == 8);
	CHECK(Callstacks.GetCallstack(2).Callstack == 0);
	CHECK(Callstacks.GetCallstack(2).bIgnore == false);
}

TEST_CASE("CoreUObject::Serialization::ArchiveStackTrace::Basic", "[CoreUObject][Serialization]")
{
	const int32 MaxDiffsToLog = 1000;

	SECTION("DiffMap - identical")
	{
		FArchiveCallstacks Callstacks(nullptr);
		FArchiveDiffMap DiffMap;

		FLargeMemoryWriter InitialState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			State.Serialize(InitialState);
		}

		FLargeMemoryWriter NewState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			FArchiveStackTraceWriter StackTraceWriter(NewState, Callstacks);
			State.Serialize(StackTraceWriter);
		}

		FArchiveStackTraceWriter::FPackageData InitialData = ToPackageData(InitialState);
		FArchiveStackTraceWriter::FPackageData NewData = ToPackageData(NewState);
		const bool bIsIdentical = FArchiveStackTraceWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
		CHECK(bIsIdentical);
	}

	SECTION("DiffMap - mismatch")
	{
		FArchiveCallstacks Callstacks(nullptr);
		FArchiveDiffMap DiffMap;

		FLargeMemoryWriter InitialState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			State.Serialize(InitialState);
		}

		FLargeMemoryWriter NewState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,4};
			FArchiveStackTraceWriter StackTraceWriter(NewState, Callstacks);
			State.Serialize(StackTraceWriter);
		}

		FArchiveStackTraceWriter::FPackageData InitialData = ToPackageData(InitialState);
		FArchiveStackTraceWriter::FPackageData NewData = ToPackageData(NewState);
		const bool bIsIdentical = FArchiveStackTraceWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
		CHECK(bIsIdentical == false);
	}

	SECTION("Compare - mismatch")
	{
		FArchiveCallstacks Callstacks(nullptr);
		FArchiveCallstacks CallstacksWithStackTrace(nullptr);
		FArchiveDiffMap DiffMap;

		FBasicDiffState InitialState = FBasicDiffState {1,2,3};
		FLargeMemoryWriter InitialMemory;
		InitialMemory.SetByteSwapping(false);

		InitialState.Serialize(InitialMemory);

		FBasicDiffState NewState = FBasicDiffState {1,22,33};
		TUniquePtr<FLargeMemoryWriter> NewMemory = MakeUnique<FLargeMemoryWriter>();
		NewMemory->SetByteSwapping(false);

		{
			FArchiveStackTraceWriter StackTraceWriter(*NewMemory, Callstacks);
			NewState.Serialize(StackTraceWriter);
		}

		// Generate diff map
		{
			FArchiveStackTraceWriter::FPackageData InitialData = ToPackageData(InitialMemory);
			FArchiveStackTraceWriter::FPackageData NewData = ToPackageData(*NewMemory);
			const bool bIsIdentical = FArchiveStackTraceWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
			CHECK(bIsIdentical == false);
		}
		
		NewMemory = MakeUnique<FLargeMemoryWriter>();
		NewMemory->SetByteSwapping(false);
		{
			FArchiveStackTraceWriter StackTraceWriter(*NewMemory, CallstacksWithStackTrace, &DiffMap);
			NewState.Serialize(StackTraceWriter);
		}

		// Compare using callstacks and diffmap
		{
			FArchiveStackTraceWriter::FPackageData InitialData = ToPackageData(InitialMemory);
			FArchiveStackTraceWriter::FPackageData NewData = ToPackageData(*NewMemory);
			int32 DiffsLogged = 0;
			TMap<FName, FArchiveDiffStats> DiffStats;
			const bool bSuppressLogging = true; // Suppress warning logs when running tests on CI

			FArchiveStackTraceWriter::Compare(
				InitialData,
				NewData,
				CallstacksWithStackTrace,
				DiffMap,
				TEXT("BasicDiffTest"),
				nullptr,
				MaxDiffsToLog,
				DiffsLogged,
				DiffStats,
				bSuppressLogging);

			CHECK(DiffStats[NAME_None].NumDiffs == 2);
		}
	}
}

} // namespace UE::CoreUObject::Serialization::Tests

#endif // WITH_LOW_LEVEL_TESTS && WITH_EDITORONLY_DATA
