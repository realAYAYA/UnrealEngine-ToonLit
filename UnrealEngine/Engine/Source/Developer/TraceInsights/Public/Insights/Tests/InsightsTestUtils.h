// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

class FAutomationTestBase;

class TRACEINSIGHTS_API FInsightsTestUtils
{
public:
	FInsightsTestUtils(FAutomationTestBase* Test);

	bool AnalyzeTrace(const TCHAR* Path) const;
	bool FileContainsString(const FString& PathToFile, const FString& ExpectedString, double Timeout) const;
	bool SetupUTS(double Timeout = 30.0, bool bUseFork = false) const;
	bool KillUTS(double Timeout = 30.0) const;
	bool StartTracing(FTraceAuxiliary::EConnectionType ConnectionType, double Timeout = 10.0) const;
	bool IsUnrealTraceServerReady(const TCHAR* Host = TEXT("localhost"), int32 Port = 0U) const;
	bool IsTraceHasLiveStatus(const FString& TraceName, const TCHAR* Host = TEXT("localhost"), int32 Port = 0U) const;
	void ResetSession() const;
	FString GetLiveTrace(const TCHAR* Host = TEXT("localhost"), int32 Port = 0U) const;

private:
	FAutomationTestBase* Test;
};
