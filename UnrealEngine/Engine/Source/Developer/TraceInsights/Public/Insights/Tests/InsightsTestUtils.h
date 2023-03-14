// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

class TRACEINSIGHTS_API FInsightsTestUtils
{
public:
	FInsightsTestUtils(FAutomationTestBase* Test);

	bool AnalyzeTrace(const TCHAR* Path) const;

private:
	FAutomationTestBase* Test;
};
