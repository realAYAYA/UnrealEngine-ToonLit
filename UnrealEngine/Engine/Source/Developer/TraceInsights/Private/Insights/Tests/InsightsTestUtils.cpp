// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/Tests/InsightsTestUtils.h"

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/ModuleService.h"

#include "Insights/Common/Stopwatch.h"
#include "Insights/IUnrealInsightsModule.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

FInsightsTestUtils::FInsightsTestUtils(FAutomationTestBase* InTest) :
	Test(InTest)
{
#if WITH_EDITOR
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TSharedPtr<TraceServices::IModuleService> ModuleService = TraceServicesModule.GetModuleService();
	ModuleService->SetModuleEnabled(FName("TraceModule_LoadTimeProfiler"), true);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::AnalyzeTrace(const TCHAR* Path) const
{
	IUnrealInsightsModule& TraceInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");

	if (!FPaths::FileExists(Path))
	{
		Test->AddError(FString::Printf(TEXT("File does not exist: %s."), Path));
		return false;
	}

	TraceInsightsModule.StartAnalysisForTraceFile(Path);
	auto Session = TraceInsightsModule.GetAnalysisSession();
	if (Session == nullptr)
	{
		Test->AddError(TEXT("Session analysis failed to start."));
		return false;
	}

	FStopwatch StopWatch;
	StopWatch.Start();

	double Duration = 0.0f;
	constexpr double MaxDuration = 75.0f;
	while (!Session->IsAnalysisComplete())
	{
		FPlatformProcess::Sleep(0.033f);

		if (Duration > MaxDuration)
		{
			Test->AddError(FString::Format(TEXT("Session analysis took longer than the maximum allowed time of {0} seconds. Aborting test."), { MaxDuration }));
			return false;
		}

		StopWatch.Update();
		Duration = StopWatch.GetAccumulatedTime();
	}

	StopWatch.Stop();
	Duration = StopWatch.GetAccumulatedTime();

	Test->AddInfo(FString::Format(TEXT("Session analysis took {0} seconds."), { Duration }));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
