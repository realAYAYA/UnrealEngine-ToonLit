// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatsModule.h"
#include "Analyzers/StatsTraceAnalysis.h"
#include "TraceServices/Model/Counters.h"

namespace TraceServices
{

static const FName StatsModuleName("TraceModule_Stats");

void FStatsModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = StatsModuleName;
	OutModuleInfo.DisplayName = TEXT("Stats");
}
	
void FStatsModule::OnAnalysisBegin(IAnalysisSession& Session)
{
	IEditableCounterProvider& EditableCounterProvider = EditCounterProvider(Session);
	Session.AddAnalyzer(new FStatsAnalyzer(Session, EditableCounterProvider));
}

void FStatsModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	//OutLoggers.Add(TEXT("Stats"));
}

} // namespace TraceServices
