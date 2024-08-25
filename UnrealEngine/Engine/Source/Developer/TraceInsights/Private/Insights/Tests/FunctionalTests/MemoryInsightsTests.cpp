// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutomationDriverCommon.h"
#include "Algo/Find.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Insights/InsightsManager.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocGroupingByCallstack.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocNode.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryAlloc.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/MemoryProfiler/Widgets/SMemAllocTableTreeView.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerWindow.h"
#include "Insights/Table/ViewModels/TableTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "Insights/Table/Widgets/STableTreeView.h"
#include "Insights/Tests/InsightsTestUtils.h"
#include "Insights/Widgets/SStartPageWindow.h"
#include "Insights/Widgets/STimingView.h"
#include "Misc/AutomationTest.h"
#include "TraceServices/Model/AllocationsProvider.h"

DECLARE_LOG_CATEGORY_EXTERN(MemoryInsightsTests, Log, All);

#if !WITH_EDITOR

BEGIN_DEFINE_SPEC(FAutomationDriverUnrealInsightsHubMemoryInsightsTest, "System.Insights.Hub.MemoryInsights", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
FAutomationDriverPtr Driver;
TSharedPtr<SWindow> AutomationWindow;
END_DEFINE_SPEC(FAutomationDriverUnrealInsightsHubMemoryInsightsTest)
void FAutomationDriverUnrealInsightsHubMemoryInsightsTest::Define()
{
	BeforeEach([this]() {
		AutomationWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		const FString AutomationWindowName = TEXT("Automation");
		if (AutomationWindow->GetTitle().ToString().Contains(AutomationWindowName))
		{
			AutomationWindow->Minimize();
		}

		IUnrealInsightsModule& UnrealInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");
		if (IAutomationDriverModule::Get().IsEnabled())
		{
			IAutomationDriverModule::Get().Disable();
		}
		IAutomationDriverModule::Get().Enable();

		Driver = IAutomationDriverModule::Get().CreateDriver();
		});

	Describe("XMLReportsUpload", [this]()
		{
			It("should verify that user can upload xml reports in Memory Insights tab", EAsyncExecution::ThreadPool, FTimespan::FromSeconds(120), [this]()
				{
					FInsightsTestUtils Utils(this);
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
					if (!InsightsManager.IsValid())
					{
						AddError("Insights manager should not be null");
						return;
					}

					// Start tracing editor instance, not Lyra. There is no difference between them in this test.
					FString UEPath = FPlatformProcess::GenerateApplicationPath("UnrealEditor", EBuildConfiguration::Development);
					FString Parameters = TEXT("-trace=Bookmark,Memory -tracehost=127.0.0.1");
					constexpr bool bLaunchDetached = true;
					constexpr bool bLaunchHidden = false;
					constexpr bool bLaunchReallyHidden = false;
					uint32 ProcessID = 0;
					const int32 PriorityModifier = 0;
					const TCHAR* OptionalWorkingDirectory = nullptr;
					void* PipeWriteChild = nullptr;
					void* PipeReadChild = nullptr;
					FProcHandle EditorHandle = FPlatformProcess::CreateProc(*UEPath, *Parameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
					if (!EditorHandle.IsValid())
					{
						AddError("Editor should be started");
						return;
					}

					// Verify that LIVE trace appeared
					int Index = 0;
					auto TraceWaiter = [Driver = Driver, &Index](void) -> bool
					{
						auto Elements = Driver->FindElements(By::Id("TraceStatusColumnList"))->GetElements();
						for (int i = 0; i < Elements.Num(); ++i) {
							if (Elements[i]->GetText().ToString() == TEXT("LIVE")) {
								Index = i;
								return true;
							}
						}
						return false;
					};

					if (!Driver->Wait(Until::Condition(TraceWaiter, FWaitTimeout::InSeconds(10))))
					{
						AddError("Live trace should appear");
						FPlatformProcess::TerminateProc(EditorHandle);
						return;
					}

					FDriverElementRef TraceElement = Driver->FindElements(By::Id("TraceList"))->GetElements()[Index];
					const FString TraceName = TraceElement->GetText().ToString();

					const FString StoreDir = InsightsManager->GetStoreDir();
					const FString ProjectDir = FPaths::ProjectDir();
					const FString StoreTracePath = StoreDir / FString::Printf(TEXT("%s.utrace"), *TraceName);
					const FString StoreCachePath = StoreDir / FString::Printf(TEXT("%s.ucache"), *TraceName);
					const FString LogDirPath = ProjectDir / TEXT("TestResults");
					const FString LogPath = ProjectDir / TEXT("TestResults/Log.txt");
					const FString SuccessTestResult = TEXT("Test Completed. Result={Success}");

					// Test live trace
					FString TraceParameters = FString::Printf(TEXT("-InsightsTest -ABSLOG=\"%s\" -AutoQuit -ExecOnAnalysisCompleteCmd=\"Automation RunTests System.Insights.Trace.Analysis.MemoryInsights.UploadMemoryInsightsLLMXMLReportsTrace\" -OpenTraceFile=\"%s\""), *LogPath, *StoreTracePath);
					InsightsManager->OpenUnrealInsights(*TraceParameters);
					bool bLineFound = Utils.FileContainsString(LogPath, SuccessTestResult, 60.0f);
					TestTrue("Test for live trace should pass", bLineFound);

					IFileManager::Get().DeleteDirectory(*LogDirPath, false, true);
					FPlatformProcess::TerminateProc(EditorHandle);

					// Test stopped trace 
					InsightsManager->OpenUnrealInsights(*TraceParameters);
					bLineFound = Utils.FileContainsString(LogPath, SuccessTestResult, 60.0f);
					TestTrue("Test for stopped trace should pass", bLineFound);

					IFileManager::Get().DeleteDirectory(*LogDirPath, false, true);
					IFileManager::Get().Delete(*StoreTracePath);
					IFileManager::Get().Delete(*StoreCachePath);
				});
		});
	AfterEach([this]() {
		Driver.Reset();
		IAutomationDriverModule::Get().Disable();
		AutomationWindow->Restore();
		});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemoryInsightsUploadLLMXMLReportsTraceTest, "System.Insights.Trace.Analysis.MemoryInsights.UploadMemoryInsightsLLMXMLReportsTrace", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FMemoryInsightsUploadLLMXMLReportsTraceTest::RunTest(const FString& Parameters)
{
	const FString ReportGraphsXMLPath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/ReportGraphs.xml");
	const FString LLMReportTypesXMLPath = FPaths::RootDir() / TEXT("EngineTest/SourceAssets/Utrace/LLMReportTypes.xml");

	FMemorySharedState* SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	if (!SharedState)
	{
		AddError("ProfilerWindow should be valid. Please, run this test throught Insights Session automation tab");
		return false;
	}

	SharedState->RemoveAllMemTagGraphTracks();
	const int DefaultTracksAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	SharedState->RemoveAllMemTagGraphTracks();
	AddExpectedError("Failed to load Report");
	SharedState->CreateTracksFromReport(ReportGraphsXMLPath);
	SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	const int AfterReportGraphsUploadTrackAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	TestTrue("Tracks amount should be default ", DefaultTracksAmount == AfterReportGraphsUploadTrackAmount);

	SharedState->RemoveAllMemTagGraphTracks();
	SharedState->CreateTracksFromReport(LLMReportTypesXMLPath);
	SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	const int AfterLLMReportTypesUploadTrackAmount = SharedState->GetTimingView()->GetAllTracks().Num();
	TestTrue("Tracks should not be default", DefaultTracksAmount != AfterLLMReportTypesUploadTrackAmount);

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FWaitForRunningQuieryFinishedCommand, TSharedPtr<Insights::SMemAllocTableTreeView>, MemAllocTableTreeView, double, Timeout, FAutomationTestBase*, Test);
bool FWaitForRunningQuieryFinishedCommand::Update()
{
	if (!MemAllocTableTreeView->IsRunning())
	{
		return true;
	}

	if (FPlatformTime::Seconds() - StartTime >= Timeout)
	{
		Test->AddError(TEXT("FWaitForRunningQuieryFinishedCommand timed out"));
		return true;
	}

	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FChangeGroupingCommand, TSharedPtr<Insights::SMemAllocTableTreeView>, MemAllocTableTreeView, FAutomationTestBase*, Test);
bool FChangeGroupingCommand::Update()
{
	TArray<TSharedPtr<Insights::FTreeNodeGrouping>> CurrentGroupings;
	for (const auto& Grouping : MemAllocTableTreeView->GetAvailableGroupings())
	{
		if (Grouping->GetTitleName().ToString().Contains(TEXT("By Callstack")))
		{
			CurrentGroupings.Add(Grouping);
		}
	}

	Test->TestTrue(TEXT("CurrentGroupings should not be empty"), !CurrentGroupings.IsEmpty());
	MemAllocTableTreeView->SetCurrentGroupings(CurrentGroupings);

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FVerifyHierarchyCallStackCommand, TSharedPtr<Insights::SMemAllocTableTreeView>, MemAllocTableTreeView, FInsightsTestUtils, InsightsTestUtils, double, Timeout, FAutomationTestBase*, Test);
bool FVerifyHierarchyCallStackCommand::Update()
{
	if (!MemAllocTableTreeView->IsRunningAsyncUpdate())
	{
		for (const TSharedPtr<Insights::FTableTreeNode>& Node : MemAllocTableTreeView->GetTableRowNodes())
		{
			const Insights::FMemAllocNode& MemAllocNode = static_cast<const Insights::FMemAllocNode&>(*Node);
			const Insights::FMemoryAlloc Alloc = MemAllocNode.GetMemAllocChecked();
			if (!(!Alloc.GetAllocCallstack() || Alloc.GetAllocCallstack()->Num() == 0 || (Alloc.GetAllocCallstack()->Num() != 0 && Alloc.GetAllocCallstack()->Num() < 256)))
			{
				Test->AddError(TEXT("Resolved alloc callstack should be valid"));
			}
			if (!(!Alloc.GetFreeCallstack() || Alloc.GetFreeCallstack()->Num() == 0 || (Alloc.GetFreeCallstack()->Num() != 0 && Alloc.GetFreeCallstack()->Num() < 256)))
			{
				Test->AddError(TEXT("Resolved free callstack should be valid"));
			}
		}
		return true;
	}

	if (FPlatformTime::Seconds() - StartTime >= Timeout)
	{
		Test->AddError(TEXT("FVerifyHierarchyCallStackCommand timed out"));
		return true;
	}

	return false;
}

const TMap<TraceServices::IAllocationsProvider::EQueryRule, Insights::SMemAllocTableTreeView::FQueryParams> AllocsTimeMarkerStandaloneGameGetterMap
{
	{TraceServices::IAllocationsProvider::EQueryRule::aAf, {nullptr, {5.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::afA, {nullptr, {10.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::Aaf, {nullptr, {10.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aAfB, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBf, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aAfaBf, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AfB, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBCf, {nullptr, {50.0, 51.0, 52.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBfC, {nullptr, {50.0, 51.0, 52.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aABfC, {nullptr, {50.0, 51.0, 52.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBCfD, {nullptr, {50.0, 51.0, 52.0, 53.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aABf, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AafB, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaB, {nullptr, {50.0, 51.0, 0.0, 0.0}}},
};

const TMap<TraceServices::IAllocationsProvider::EQueryRule, Insights::SMemAllocTableTreeView::FQueryParams> AllocsTimeMarkerEditorPackageGetterMap
{
	{TraceServices::IAllocationsProvider::EQueryRule::aAf, {nullptr, {5.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::afA, {nullptr, {10.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::Aaf, {nullptr, {10.0, 0.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aAfB, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBf, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aAfaBf, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AfB, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBCf, {nullptr, {1.0, 2.0, 3.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBfC, {nullptr, {1.0, 2.0, 3.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aABfC, {nullptr, {1.0, 2.0, 3.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaBCfD, {nullptr, {1.0, 2.0, 3.0, 4.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AafB, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::AaB, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
	{TraceServices::IAllocationsProvider::EQueryRule::aABf, {nullptr, {2.0, 3.0, 0.0, 0.0}}},
};

bool MemoryInsightsAllocationsQueryTableTest(const FString& Parameters, const TMap <TraceServices::IAllocationsProvider::EQueryRule, Insights::SMemAllocTableTreeView::FQueryParams> AllocsTimeMarkerGetterMap, FAutomationTestBase* Test)
{
	double Timeout = 30.0;
	FInsightsTestUtils InsightsTestUtils(Test);
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = FMemoryProfilerManager::Get()->GetProfilerWindow();
	TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
	FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();

	TSharedPtr<Insights::FMemoryRuleSpec> MemoryRule = *Algo::FindByPredicate(SharedState.GetMemoryRules(),
		[&Parameters](const TSharedPtr<Insights::FMemoryRuleSpec>& Rule)
		{
			return Rule->GetShortName().ToString().Contains(Parameters);
		});

	if (!MemoryRule.IsValid())
	{
		Test->AddError(TEXT("MemoryRule should not be null"));
		return false;
	}

	TSharedPtr<Insights::SMemAllocTableTreeView> MemAllocTableTreeView = ProfilerWindow->ShowMemAllocTableTreeViewTab();

	Insights::SMemAllocTableTreeView::FQueryParams QueryParams = AllocsTimeMarkerGetterMap.FindChecked(MemoryRule->GetValue());
	if (MemoryRule->GetValue() == TraceServices::IAllocationsProvider::EQueryRule::Aaf)
	{
		QueryParams.TimeMarkers[0] = InsightsManager->GetSessionDuration() - 10.0;
	}
	QueryParams.Rule = MemoryRule;
	MemAllocTableTreeView->SetQueryParams(QueryParams);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForRunningQuieryFinishedCommand(MemAllocTableTreeView, 120.0f, Test));
	ADD_LATENT_AUTOMATION_COMMAND(FChangeGroupingCommand(MemAllocTableTreeView, Test));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForRunningQuieryFinishedCommand(MemAllocTableTreeView, 120.0f, Test));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyHierarchyCallStackCommand(MemAllocTableTreeView, InsightsTestUtils, 120.0f, Test));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMemoryInsightsAllocationsQueryTableEditorPackageTest, "System.Insights.Trace.Analysis.MemoryInsights.AllocationsQueryTable.Editor/Package", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FMemoryInsightsAllocationsQueryTableEditorPackageTest::RunTest(const FString& Parameters)
{
	bool bSuccess = MemoryInsightsAllocationsQueryTableTest(Parameters, AllocsTimeMarkerEditorPackageGetterMap, this);
	return bSuccess;
}

void FMemoryInsightsAllocationsQueryTableEditorPackageTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	if (!FMemoryProfilerManager::Get().IsValid())
	{
		return;
	}
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = FMemoryProfilerManager::Get()->GetProfilerWindow();
	if (!ProfilerWindow.IsValid())
	{
		return;
	}

	FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();

	for (const auto& MemoryRules : SharedState.GetMemoryRules())
	{
		const FString& MemoryRuleName = MemoryRules->GetShortName().ToString();

		OutBeautifiedNames.Add(FString::Printf(TEXT("%s"), *MemoryRuleName));
		OutTestCommands.Add(MemoryRuleName);
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMemoryInsightsAllocationsQueryTableStandaloneTest, "System.Insights.Trace.Analysis.MemoryInsights.AllocationsQueryTable.Standalone", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FMemoryInsightsAllocationsQueryTableStandaloneTest::RunTest(const FString& Parameters)
{
	bool bSuccess = MemoryInsightsAllocationsQueryTableTest(Parameters, AllocsTimeMarkerStandaloneGameGetterMap, this);
	return bSuccess;
}

void FMemoryInsightsAllocationsQueryTableStandaloneTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	if (!FMemoryProfilerManager::Get().IsValid())
	{
		return;
	}
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = FMemoryProfilerManager::Get()->GetProfilerWindow();
	if (!ProfilerWindow.IsValid())
	{
		return;
	}

	FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
	TSharedPtr<Insights::FMemoryRuleSpec> MemoryRule = SharedState.GetMemoryRules()[0];

	for (const auto& MemoryRules : SharedState.GetMemoryRules())
	{
		const FString& MemoryRuleName = MemoryRules->GetShortName().ToString();

		OutBeautifiedNames.Add(FString::Printf(TEXT("%s"), *MemoryRuleName));
		OutTestCommands.Add(MemoryRuleName);
	}
}

#endif //!WITH_EDITOR