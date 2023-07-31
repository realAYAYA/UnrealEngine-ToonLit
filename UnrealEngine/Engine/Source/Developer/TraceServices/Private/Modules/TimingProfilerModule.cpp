// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimingProfilerModule.h"
#include "Analyzers/CpuProfilerTraceAnalysis.h"
#include "Analyzers/GpuProfilerTraceAnalysis.h"
#include "AnalysisServicePrivate.h"
#include "Model/ThreadsPrivate.h"
#include "Model/TimingProfilerPrivate.h"

namespace TraceServices
{

static const FName TimingProfilerModuleName("TraceModule_TimingProfiler");
static const FName TimingProfilerProviderName("TimingProfilerProvider");

void FTimingProfilerModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = TimingProfilerModuleName;
	OutModuleInfo.DisplayName = TEXT("Timing");
}
	
void FTimingProfilerModule::OnAnalysisBegin(IAnalysisSession& InSession)
{
	FAnalysisSession& Session = static_cast<FAnalysisSession&>(InSession);
	
	IEditableThreadProvider* EditableThreadProvider = Session.EditProvider<IEditableThreadProvider>(FThreadProvider::ProviderName);
	check(EditableThreadProvider != nullptr);

	// see comment in FAnalysisService::StartAnalysis for details
	TSharedPtr<FTimingProfilerProvider> TimingProfilerProvider = MakeShared<FTimingProfilerProvider>(Session);
	Session.AddProvider(TimingProfilerProviderName, TSharedPtr<ITimingProfilerProvider>(TimingProfilerProvider), TSharedPtr<IEditableTimingProfilerProvider>(TimingProfilerProvider));
	Session.AddAnalyzer(new FCpuProfilerAnalyzer(Session, *TimingProfilerProvider, *EditableThreadProvider));
	Session.AddAnalyzer(new FGpuProfilerAnalyzer(Session, *TimingProfilerProvider));
}

void FTimingProfilerModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("CpuProfiler"));
	OutLoggers.Add(TEXT("GpuProfiler"));
}

const ITimingProfilerProvider* ReadTimingProfilerProvider(const IAnalysisSession& Session)
{
	return Session.ReadProvider<ITimingProfilerProvider>(TimingProfilerProviderName);
}

IEditableTimingProfilerProvider* EditTimingProfilerProvider(IAnalysisSession& Session)
{
	return Session.EditProvider<IEditableTimingProfilerProvider>(TimingProfilerProviderName);
}

} // namespace TraceServices
