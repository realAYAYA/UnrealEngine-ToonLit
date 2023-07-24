// Copyright Epic Games, Inc. All Rights Reserved.

#include "VLogTraceModule.h"
#include "VisualLoggerProvider.h"
#include "VisualLoggerAnalyzer.h"

FName FVLogTraceModule::ModuleName("GameplayTrace");

void FVLogTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("VisualLogger");
}

void FVLogTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	TSharedPtr<FVisualLoggerProvider> VisualLoggerProvider = MakeShared<FVisualLoggerProvider>(InSession);
	InSession.AddProvider(FVisualLoggerProvider::ProviderName, VisualLoggerProvider);

	InSession.AddAnalyzer(new FVisualLoggerAnalyzer(InSession, *VisualLoggerProvider));
}

void FVLogTraceModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("VisualLogger"));
}

void FVLogTraceModule::GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory)
{

}

