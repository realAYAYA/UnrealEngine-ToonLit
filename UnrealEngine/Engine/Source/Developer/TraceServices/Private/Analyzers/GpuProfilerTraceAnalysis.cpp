// Copyright Epic Games, Inc. All Rights Reserved.

#include "GpuProfilerTraceAnalysis.h"

#include "AnalysisServicePrivate.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"

namespace TraceServices
{

FGpuProfilerAnalyzer::FGpuProfilerAnalyzer(FAnalysisSession& InSession, FTimingProfilerProvider& InTimingProfilerProvider)
	: Session(InSession)
	, TimingProfilerProvider(InTimingProfilerProvider)
	, Timeline(TimingProfilerProvider.EditGpuTimeline())
	, Timeline2(TimingProfilerProvider.EditGpu2Timeline())
{

}

void FGpuProfilerAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_EventSpec, "GpuProfiler", "EventSpec");
	Builder.RouteEvent(RouteId_Frame, "GpuProfiler", "Frame");
	Builder.RouteEvent(RouteId_Frame2, "GpuProfiler", "Frame2");
}

bool FGpuProfilerAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FGpuProfilerAnalyzer"));

	FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;

	switch (RouteId)
	{
	case RouteId_EventSpec:
	{
		uint32 EventType = EventData.GetValue<uint32>("EventType");
		const auto& Name = EventData.GetArray<UTF16CHAR>("Name");

		auto NameTChar = StringCast<TCHAR>(Name.GetData(), Name.Num());
		uint32* TimerIndexPtr = EventTypeMap.Find(EventType);
		if (!TimerIndexPtr)
		{
			EventTypeMap.Add(EventType, TimingProfilerProvider.AddGpuTimer(FStringView(NameTChar.Get(), NameTChar.Length())));
		}
		else
		{
			TimingProfilerProvider.SetTimerName(*TimerIndexPtr, FStringView(NameTChar.Get(), NameTChar.Length()));
		}
		break;
	}
	case RouteId_Frame:
	case RouteId_Frame2:
	{
		TraceServices::FTimingProfilerProvider::TimelineInternal& ThisTimeline = (RouteId == RouteId_Frame) ? Timeline : Timeline2;
		double& ThisMinTime = (RouteId == RouteId_Frame) ? MinTime : MinTime2;
		const auto& Data = EventData.GetArray<uint8>("Data");
		const uint8* BufferPtr = Data.GetData();
		const uint8* BufferEnd = BufferPtr + Data.Num();

		uint32 CurrentDepth = 0;

		uint64 CalibrationBias = EventData.GetValue<uint64>("CalibrationBias");
		uint64 LastTimestamp = EventData.GetValue<uint64>("TimestampBase");
		double LastTime = 0.0;
		while (BufferPtr < BufferEnd)
		{
			uint64 DecodedTimestamp = FTraceAnalyzerUtils::Decode7bit(BufferPtr);
			uint64 ActualTimestamp = (DecodedTimestamp >> 1) + LastTimestamp;
			LastTimestamp = ActualTimestamp;
			LastTime = double(ActualTimestamp + CalibrationBias) / 1000000.0;
			LastTime += Context.EventTime.AsSeconds(0);

			// The monolithic timeline assumes that timestamps are ever increasing, but
			// with gpu/cpu calibration and drift there can be a tiny bit of overlap between
			// frames. So we just clamp.
			if (ThisMinTime > LastTime)
			{
				LastTime = ThisMinTime;
			}
			ThisMinTime = LastTime;

			if (DecodedTimestamp & 1ull)
			{
				uint32 EventType = *reinterpret_cast<const uint32*>(BufferPtr);
				BufferPtr += sizeof(uint32);
				if (EventTypeMap.Contains(EventType))
				{
					FTimingProfilerEvent Event;
					Event.TimerIndex = EventTypeMap[EventType];
					ThisTimeline.AppendBeginEvent(LastTime, Event);
				}
				else
				{
					FTimingProfilerEvent Event;
					Event.TimerIndex = TimingProfilerProvider.AddGpuTimer(TEXTVIEW("<unknown>"));
					EventTypeMap.Add(EventType, Event.TimerIndex);
					ThisTimeline.AppendBeginEvent(LastTime, Event);
				}
				++CurrentDepth;
			}
			else
			{
				if (CurrentDepth > 0)
				{
					--CurrentDepth;
				}
				ThisTimeline.AppendEndEvent(LastTime);
			}
		}
		Session.UpdateDurationSeconds(LastTime);
		check(BufferPtr == BufferEnd);
		check(CurrentDepth == 0);
		break;
	}
		
	}

	return true;
}

} // namespace TraceServices
