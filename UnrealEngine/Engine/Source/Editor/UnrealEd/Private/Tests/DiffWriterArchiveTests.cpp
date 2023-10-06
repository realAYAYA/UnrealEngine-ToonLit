// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Cooker/DiffWriterArchive.h"

#include "Memory/SharedBuffer.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace UE::DiffWriterArchive::Tests
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

FDiffWriterArchiveWriter::FPackageData ToPackageData(FLargeMemoryWriter& Ar)
{
	return FDiffWriterArchiveWriter::FPackageData
	{
		Ar.GetData(),
		Ar.TotalSize()
	};
}

} // namespace UE::DiffWriterArchive::Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffWriterArchiveTestsCallstacks, "System.Core.Cooker.DiffWriterArchive.Callstacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffWriterArchiveTestsCallstacks::RunTest(const FString& Parameters)
{
	using namespace UE::DiffWriterArchive::Tests;

	const int32 MaxDiffsToLog = 1000;
	FDiffWriterCallstacks Callstacks(nullptr);

	FLargeMemoryWriter Memory;
	FBasicDiffState State = FBasicDiffState {1,2,3};
	FDiffWriterArchiveWriter StackTraceWriter(Memory, Callstacks, 0);
	State.Serialize(StackTraceWriter);

	TestTrueExpr(Callstacks.Num() == 3);
	TestTrueExpr(Callstacks.TotalCapturedSize() == sizeof(FBasicDiffState));
	TestTrueExpr(Callstacks.GetCallstack(0).Offset == 0);
	TestTrueExpr(Callstacks.GetCallstack(0).Callstack == 0);
	TestTrueExpr(Callstacks.GetCallstack(0).bIgnore == false);
	TestTrueExpr(Callstacks.GetCallstack(1).Offset == 4);
	TestTrueExpr(Callstacks.GetCallstack(1).Callstack == 0);
	TestTrueExpr(Callstacks.GetCallstack(1).bIgnore == false);
	TestTrueExpr(Callstacks.GetCallstack(2).Offset == 8);
	TestTrueExpr(Callstacks.GetCallstack(2).Callstack == 0);
	TestTrueExpr(Callstacks.GetCallstack(2).bIgnore == false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffWriterArchiveTestsBasic, "System.Core.Cooker.DiffWriterArchive.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffWriterArchiveTestsBasic::RunTest(const FString& Parameters)
{
	using namespace UE::DiffWriterArchive::Tests;

	const int32 MaxDiffsToLog = 1000;

	// SECTION("DiffMap - identical")
	{
		FDiffWriterCallstacks Callstacks(nullptr);
		FDiffWriterDiffMap DiffMap;

		FLargeMemoryWriter InitialState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			State.Serialize(InitialState);
		}

		FLargeMemoryWriter NewState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			FDiffWriterArchiveWriter StackTraceWriter(NewState, Callstacks);
			State.Serialize(StackTraceWriter);
		}

		FDiffWriterArchiveWriter::FPackageData InitialData = ToPackageData(InitialState);
		FDiffWriterArchiveWriter::FPackageData NewData = ToPackageData(NewState);
		const bool bIsIdentical = FDiffWriterArchiveWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
		TestTrueExpr(bIsIdentical);
	}

	// SECTION("DiffMap - mismatch")
	{
		FDiffWriterCallstacks Callstacks(nullptr);
		FDiffWriterDiffMap DiffMap;

		FLargeMemoryWriter InitialState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,3};
			State.Serialize(InitialState);
		}

		FLargeMemoryWriter NewState;
		{
			FBasicDiffState State = FBasicDiffState {1,2,4};
			FDiffWriterArchiveWriter StackTraceWriter(NewState, Callstacks);
			State.Serialize(StackTraceWriter);
		}

		FDiffWriterArchiveWriter::FPackageData InitialData = ToPackageData(InitialState);
		FDiffWriterArchiveWriter::FPackageData NewData = ToPackageData(NewState);
		const bool bIsIdentical = FDiffWriterArchiveWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
		TestTrueExpr(bIsIdentical == false);
	}

	// SECTION("Compare - mismatch")
	{
		FDiffWriterCallstacks Callstacks(nullptr);
		FDiffWriterCallstacks CallstacksWithStackTrace(nullptr);
		FDiffWriterDiffMap DiffMap;

		FBasicDiffState InitialState = FBasicDiffState {1,2,3};
		FLargeMemoryWriter InitialMemory;
		InitialMemory.SetByteSwapping(false);

		InitialState.Serialize(InitialMemory);

		FBasicDiffState NewState = FBasicDiffState {1,22,33};
		TUniquePtr<FLargeMemoryWriter> NewMemory = MakeUnique<FLargeMemoryWriter>();
		NewMemory->SetByteSwapping(false);

		{
			FDiffWriterArchiveWriter StackTraceWriter(*NewMemory, Callstacks);
			NewState.Serialize(StackTraceWriter);
		}

		// Generate diff map
		{
			FDiffWriterArchiveWriter::FPackageData InitialData = ToPackageData(InitialMemory);
			FDiffWriterArchiveWriter::FPackageData NewData = ToPackageData(*NewMemory);
			const bool bIsIdentical = FDiffWriterArchiveWriter::GenerateDiffMap(InitialData, NewData, Callstacks, MaxDiffsToLog, DiffMap);
			TestTrueExpr(bIsIdentical == false);
		}
		
		NewMemory = MakeUnique<FLargeMemoryWriter>();
		NewMemory->SetByteSwapping(false);
		{
			FDiffWriterArchiveWriter StackTraceWriter(*NewMemory, CallstacksWithStackTrace, &DiffMap);
			NewState.Serialize(StackTraceWriter);
		}

		// Compare using callstacks and diffmap
		{
			FDiffWriterArchiveWriter::FPackageData InitialData = ToPackageData(InitialMemory);
			FDiffWriterArchiveWriter::FPackageData NewData = ToPackageData(*NewMemory);
			int32 DiffsLogged = 0;
			TMap<FName, FArchiveDiffStats> DiffStats;
			const bool bSuppressLogging = true; // Suppress warning logs when running tests on CI
			UE::DiffWriterArchive::FMessageCallback MessageCallback =
				[](ELogVerbosity::Type Verbosity, FStringView Message)
			{};
			FDiffWriterArchiveWriter::Compare(
				InitialData,
				NewData,
				CallstacksWithStackTrace,
				DiffMap,
				TEXT("BasicDiffTest"),
				nullptr,
				MaxDiffsToLog,
				DiffsLogged,
				DiffStats,
				MessageCallback,
				bSuppressLogging);

			TestTrueExpr(DiffStats[NAME_None].NumDiffs == 2);
		}
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
