// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanImportDummyTest, "MetaHuman.ProjectUtilities.UnitTests.DummyImportTest",
                                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanImportDummyTest::RunTest(const FString& Parameters)
{
	return true;
}
