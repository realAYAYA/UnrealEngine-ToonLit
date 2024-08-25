// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceModule.h"

#include "Trace/ChaosVDTraceAnalyzer.h"
#include "Trace/ChaosVDTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"

FName FChaosVDTraceModule::ModuleName("ChaosVDTrace");

void FChaosVDTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("ChaosVisualDebugger");
}

void FChaosVDTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	const TSharedPtr<FChaosVDTraceProvider> Provider = MakeShared<FChaosVDTraceProvider>(InSession);

	InSession.AddProvider(FChaosVDTraceProvider::ProviderName, Provider);
	InSession.AddAnalyzer(new FChaosVDTraceAnalyzer(InSession, Provider));
}

void FChaosVDTraceModule::GetLoggers(TArray<const TCHAR*>& OutLoggers)
{
	OutLoggers.Add(TEXT("ChaosVD"));
}

void FChaosVDTraceModule::GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory)
{
	IModule::GenerateReports(Session, CmdLine, OutputDirectory);
}
