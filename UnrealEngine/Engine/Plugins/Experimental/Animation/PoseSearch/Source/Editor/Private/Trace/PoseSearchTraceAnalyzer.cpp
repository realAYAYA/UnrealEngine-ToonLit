// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchTraceAnalyzer.h"

#include "HAL/LowLevelMemTracker.h"
#include "PoseSearchTraceProvider.h"
#include "Runtime/Private/Trace/PoseSearchTraceLogger.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace UE::PoseSearch
{

FTraceAnalyzer::FTraceAnalyzer(TraceServices::IAnalysisSession& InSession, FTraceProvider& InTraceProvider)
: Session(InSession), TraceProvider(InTraceProvider)
{
}

void FTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	FInterfaceBuilder& Builder = Context.InterfaceBuilder;

	ANSICHAR LoggerName[NAME_SIZE];
	ANSICHAR MotionMatchingStateName[NAME_SIZE];

	FTraceLogger::Name.GetPlainANSIString(LoggerName);
	FTraceMotionMatchingState::Name.GetPlainANSIString(MotionMatchingStateName);

	Builder.RouteEvent(RouteId_MotionMatchingState, LoggerName, MotionMatchingStateName);
}

bool FTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/PoseSearch::FTraceAnalyzer"));

	TraceServices::FAnalysisSessionEditScope Scope(Session);

	if (RouteId == RouteId_MotionMatchingState)
	{
		FTraceMotionMatchingStateMessage Message;
		FMemoryReaderView Archive(Context.EventData.GetArrayView<uint8>("Data"));
		Archive << Message;
		TraceProvider.AppendMotionMatchingState(Message, Context.EventTime.AsSeconds(Message.Cycle));
	}
	else
	{
		// Should not happen
		checkNoEntry();
	}

	return true;
}
} // namespace UE::PoseSearch
