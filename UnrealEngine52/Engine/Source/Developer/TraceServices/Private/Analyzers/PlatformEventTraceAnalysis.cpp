// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlatformEventTraceAnalysis.h"

#include "HAL/LowLevelMemTracker.h"
#include "Model/ContextSwitchesPrivate.h"
#include "Model/StackSamplesPrivate.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{

FPlatformEventTraceAnalyzer::FPlatformEventTraceAnalyzer(IAnalysisSession& InSession,
														 FContextSwitchesProvider& InContextSwitchesProvider,
														 FStackSamplesProvider& InStackSamplesProvider)
	: Session(InSession)
	, ContextSwitchesProvider(InContextSwitchesProvider)
	, StackSamplesProvider(InStackSamplesProvider)
{
}

void FPlatformEventTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_ContextSwitch, "PlatformEvent", "ContextSwitch");
	Builder.RouteEvent(RouteId_StackSample, "PlatformEvent", "StackSample");
	Builder.RouteEvent(RouteId_ThreadName, "PlatformEvent", "ThreadName");
}

bool FPlatformEventTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FPlatformEventTraceAnalyzer"));

	FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{

	case RouteId_ContextSwitch:
	{
		double Start = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("StartTime"));
		double End = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("EndTime"));
		uint32 ThreadId = EventData.GetValue<uint32>("ThreadId");
		uint32 CoreNumber = EventData.GetValue<uint8>("CoreNumber");
		ContextSwitchesProvider.Add(ThreadId, Start, End, CoreNumber);

		Session.UpdateDurationSeconds(End);

		break;
	}

	case RouteId_StackSample:
	{
		double Time = Context.EventTime.AsSeconds(EventData.GetValue<uint64>("Time"));
		uint32 ThreadId = EventData.GetValue<uint32>("ThreadId");
		const TArrayReader<uint64>& Addresses = EventData.GetArray<uint64>("Addresses");
		StackSamplesProvider.Add(ThreadId, Time, Addresses.Num(), Addresses.GetData());

		Session.UpdateDurationSeconds(Time);

		break;
	}

	case RouteId_ThreadName:
	{
		uint32 ThreadId = EventData.GetValue<uint32>("ThreadId");
		uint32 ProcessId = EventData.GetValue<uint32>("ProcessId");
		FStringView Name;
		if (EventData.GetString("Name", Name))
		{
			ContextSwitchesProvider.AddThreadName(ThreadId, ProcessId, Name);
		}
		break;
	}

	}

	return true;
}

void FPlatformEventTraceAnalyzer::OnThreadInfo(const FThreadInfo& ThreadInfo)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FPlatformEventTraceAnalyzer"));

	FAnalysisSessionEditScope _(Session);
	ContextSwitchesProvider.AddThreadInfo(ThreadInfo.GetId(), ThreadInfo.GetSystemId());
}

} // namespace TraceServices
