// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_WORKER

namespace UE::Net
{

class FAutomationTestRunner
{
public:
	FAutomationTestRunner();
	
	void RunTests(const TCHAR* TestFilter = nullptr);
};

}
#endif
