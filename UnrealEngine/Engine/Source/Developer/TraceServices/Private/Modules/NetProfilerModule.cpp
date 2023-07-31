// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetProfilerModule.h"
#include "Analyzers/NetTraceAnalyzer.h"
#include "AnalysisServicePrivate.h"
#include "Model/NetProfilerProvider.h"

namespace TraceServices
{

FName FNetProfilerModule::ModuleName("TraceModule_NetProfiler");

void FNetProfilerModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("NetProfiler");
}

void FNetProfilerModule::OnAnalysisBegin(IAnalysisSession& Session)
{
	TSharedPtr<FNetProfilerProvider> NetProfilerProvider = MakeShared<FNetProfilerProvider>(Session);
	Session.AddProvider(GetNetProfilerProviderName(), NetProfilerProvider);

	Session.AddAnalyzer(new FNetTraceAnalyzer(Session, *NetProfilerProvider));
}

void FNetProfilerModule::GetLoggers(TArray<const TCHAR*>& OutLoggers)
{
}

} // namespace TraceServices
