// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/MTAccessDetector.h"
#include "Misc/AutomationTest.h"
#include "HAL/Thread.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#if ENABLE_MT_DETECTOR

PRAGMA_DISABLE_OPTIMIZATION

//----------------------------------------------------------------------//
// FRWAccessDetector tests
//----------------------------------------------------------------------//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_ConcurentReadTest, "System.Core.Misc.MTAccessDetector.ConcurrentReadAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_ConcurentReadTest::RunTest(const FString& Parameters)
{
	bool bReading1 = false;
	bool bReading2 = false;
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access 1"), MTAccessDetector.AcquireReadAccess());
		bReading1 = true;
		while (!bReading2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestTrue(TEXT("Releasing Read Access 1"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestTrue(TEXT("Aquiring Read Access 2"), MTAccessDetector.AcquireReadAccess());
	bReading2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Read Access 2"), MTAccessDetector.ReleaseReadAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_ConcurentWriteTest, "System.Core.Misc.MTAccessDetector.ConcurrentWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_ConcurentWriteTest::RunTest(const FString& Parameters)
{
	bool bWriting1 = false;
	bool bWriting2 = false;
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
		bWriting1 = true;
		while (!bWriting2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseWriteAccess());
		return true;
	});
	while (!bWriting1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_ConcurentReadWriteTest, "System.Core.Misc.MTAccessDetector.ConcurrentReadWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_ConcurentReadWriteTest::RunTest(const FString& Parameters)
{
	bool bReading = false;
	bool bWriting = false;
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
		bReading = true;
		while (!bWriting) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_WriteReentrance, "System.Core.Misc.MTAccessDetector.WriteReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_WriteReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring 1st Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestFalse(TEXT("Aquiring 2nd Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestFalse(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_ReadReentrance, "System.Core.Misc.MTAccessDetector.ReadReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_ReadReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestFalse(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
	Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

//----------------------------------------------------------------------//
// FRWRecursiveAccessDetector tests
//----------------------------------------------------------------------//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_RecursiveConcurentReadTest, "System.Core.Misc.MTAccessDetector.RecursiveConcurrentReadAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_RecursiveConcurentReadTest::RunTest(const FString& Parameters)
{
	bool bReading1 = false;
	bool bReading2 = false;
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access 1"), MTAccessDetector.AcquireReadAccess());
		bReading1 = true;
		while (!bReading2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestTrue(TEXT("Releasing Read Access 1"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestTrue(TEXT("Aquiring Read Access 2"), MTAccessDetector.AcquireReadAccess());
	bReading2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Read Access 2"), MTAccessDetector.ReleaseReadAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_RecursiveConcurentWriteTest, "System.Core.Misc.MTAccessDetector.RecursiveConcurrentWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_RecursiveConcurentWriteTest::RunTest(const FString& Parameters)
{
	bool bWriting1 = false;
	bool bWriting2 = false;
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
		bWriting1 = true;
		while (!bWriting2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseWriteAccess());
		return true;
	});
	while (!bWriting1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_RecursiveConcurentReadWriteTest, "System.Core.Misc.MTAccessDetector.RecursiveConcurrentReadWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_RecursiveConcurentReadWriteTest::RunTest(const FString& Parameters)
{
	bool bReading = false;
	bool bWriting = false;
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
		bReading = true;
		while (!bWriting) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_RecursiveWriteReentrance, "System.Core.Misc.MTAccessDetector.RecursiveWriteReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_RecursiveWriteReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring 1st Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestTrue(TEXT("Aquiring 2nd Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_RecursiveReadReentrance, "System.Core.Misc.MTAccessDetector.RecursiveReadReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_RecursiveReadReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestFalse(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
	Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

//----------------------------------------------------------------------//
// FRWFullyRecursiveAccessDetector tests
//----------------------------------------------------------------------//
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_FullyRecursiveConcurentReadTest, "System.Core.Misc.MTAccessDetector.FullyRecursiveConcurrentReadAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_FullyRecursiveConcurentReadTest::RunTest(const FString& Parameters)
{
	bool bReading1 = false;
	bool bReading2 = false;
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access 1"), MTAccessDetector.AcquireReadAccess());
		bReading1 = true;
		while (!bReading2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestTrue(TEXT("Releasing Read Access 1"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestTrue(TEXT("Aquiring Read Access 2"), MTAccessDetector.AcquireReadAccess());
	bReading2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Read Access 2"), MTAccessDetector.ReleaseReadAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_FullyRecursiveConcurentWriteTest, "System.Core.Misc.MTAccessDetector.FullyRecursiveConcurrentWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_FullyRecursiveConcurentWriteTest::RunTest(const FString& Parameters)
{
	bool bWriting1 = false;
	bool bWriting2 = false;
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
		bWriting1 = true;
		while (!bWriting2) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseWriteAccess());
		return true;
	});
	while (!bWriting1) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting2 = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_FullyRecursiveConcurentReadWriteTest, "System.Core.Misc.MTAccessDetector.FullyRecursiveConcurrentReadWriteAccess", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_FullyRecursiveConcurentReadWriteTest::RunTest(const FString& Parameters)
{
	bool bReading = false;
	bool bWriting = false;
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	FThread AccessThread = FThread(TEXT("AccessThread"), [&]()
	{
		Success |= TestTrue(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
		bReading = true;
		while (!bWriting) { FPlatformProcess::SleepNoStats(0.01f); }
		Success |= TestFalse(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
		return true;
	});
	while (!bReading) { FPlatformProcess::SleepNoStats(0.01f); }
	Success |= TestFalse(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	bWriting = true;
	AccessThread.Join();
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_FullyRecursiveWriteReentrance, "System.Core.Misc.MTAccessDetector.FullyRecursiveWriteReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_FullyRecursiveWriteReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring 1st Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestTrue(TEXT("Aquiring 2nd Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRWAccessDetector_FullyRecursiveReadReentrance, "System.Core.Misc.MTAccessDetector.FullyRecursiveReadReentrance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRWAccessDetector_FullyRecursiveReadReentrance::RunTest(const FString& Parameters)
{
	UE_MT_DECLARE_RW_FULLY_RECURSIVE_ACCESS_DETECTOR(MTAccessDetector);

	bool Success = false;
	Success |= TestTrue(TEXT("Aquiring Write Access"), MTAccessDetector.AcquireWriteAccess());
	Success |= TestTrue(TEXT("Aquiring Read Access"), MTAccessDetector.AcquireReadAccess());
	Success |= TestTrue(TEXT("Releasing Read Access"), MTAccessDetector.ReleaseReadAccess());
	Success |= TestTrue(TEXT("Releasing Write Access"), MTAccessDetector.ReleaseWriteAccess());

	return Success;
}

PRAGMA_ENABLE_OPTIMIZATION
#endif // ENABLE_MT_DETECTOR
