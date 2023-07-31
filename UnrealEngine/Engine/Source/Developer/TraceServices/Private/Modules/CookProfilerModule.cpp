// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookProfilerModule.h"
#include "Analyzers/CookAnalysis.h"
#include "AnalysisServicePrivate.h"
#include "Model/CookProfilerProviderPrivate.h"

namespace TraceServices
{

static const FName CookProfilerModuleName("TraceModule_CookProfiler");
static const FName CookProfilerProviderName("CookProfilerProvider");

void FCookProfilerModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = CookProfilerProviderName;
	OutModuleInfo.DisplayName = TEXT("Cook Profiling");
}

void FCookProfilerModule::OnAnalysisBegin(IAnalysisSession& Session)
{
	TSharedPtr<FCookProfilerProvider> CookProfilerProvider = MakeShared<FCookProfilerProvider>(Session);
	Session.AddProvider(CookProfilerProviderName, CookProfilerProvider);
	Session.AddAnalyzer(new FCookAnalyzer(Session, *CookProfilerProvider));
}

void FCookProfilerModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("CookProfiler"));
}

const ICookProfilerProvider* ReadCookProfilerProvider(const IAnalysisSession& Session)
{
	return Session.ReadProvider<ICookProfilerProvider>(CookProfilerProviderName);
}

} // namespace TraceServices
