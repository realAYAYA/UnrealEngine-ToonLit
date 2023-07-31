// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLoggerAnalyzer.h"

#include "HAL/LowLevelMemTracker.h"
#include "Serialization/MemoryReader.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "VisualLoggerProvider.h"

FVisualLoggerAnalyzer::FVisualLoggerAnalyzer(TraceServices::IAnalysisSession& InSession, FVisualLoggerProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FVisualLoggerAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_VisualLogEntry, "VisualLogger", "VisualLogEntry");
}

bool FVisualLoggerAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FVisualLoggerAnalyzer"));

	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
		case RouteId_VisualLogEntry:
		{
			uint64 OwnerId = EventData.GetValue<uint64>("OwnerId");
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			TArrayView<const uint8> SerializedData = EventData.GetArrayView<uint8>("LogEntry");
			FMemoryReaderView Archive(SerializedData);
			FVisualLogEntry Entry;
			Archive << Entry;
			Provider.AppendVisualLogEntry(OwnerId, Context.EventTime.AsSeconds(Cycle), Entry);
			break;
		}
	}

	return true;
}
