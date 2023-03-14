// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/FileManager.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFileManagerReaderTest, "System.Core.HAL.FileManager.Reader", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFileManagerReaderTest::RunTest(const FString& Parameters)
{
	return true;
}

#endif