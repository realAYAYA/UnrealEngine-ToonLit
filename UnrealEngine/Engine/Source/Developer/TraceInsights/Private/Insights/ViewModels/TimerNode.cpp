// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimerNode.h"

#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/TimingProfiler.h"

#include "Insights/InsightsManager.h"
#include "Insights/ViewModels/TimingEvent.h"

#define LOCTEXT_NAMESPACE "FTimerNode"

INSIGHTS_IMPLEMENT_RTTI(FTimerNode)

const FName FTimerNode::GpuGroup(TEXT("GPU"));
const FName FTimerNode::CpuGroup(TEXT("CPU"));

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNode::FTimerNode(uint32 InTimerId, const TCHAR* InName, ETimerNodeType InType, bool bInIsGroup)
	: FBaseTreeNode(FName(InName), bInIsGroup)
	, TimerId(InTimerId)
	, MetaGroupName(InType == ETimerNodeType::CpuScope ? CpuGroup : InType == ETimerNodeType::GpuScope ? GpuGroup : NAME_None)
	, Type(InType)
	, NumGraphs(0)
	, bIsHotPath(false)
{
	uint32 Color32 = FTimingEvent::ComputeEventColor(InName);
	Color.R = ((Color32 >> 16) & 0xFF) / 255.0f;
	Color.G = ((Color32 >>  8) & 0xFF) / 255.0f;
	Color.B = ((Color32      ) & 0xFF) / 255.0f;
	Color.A = 1.0;

	ResetAggregatedStats();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Initialization constructor for the group node. */
FTimerNode::FTimerNode(const FName InGroupName)
	: FBaseTreeNode(InGroupName, true)
	, TimerId(InvalidTimerId)
	, Type(ETimerNodeType::Group)
	, Color(0.0, 0.0, 0.0, 1.0)
	, NumGraphs(0)
	, bIsHotPath(false)
{
	ResetAggregatedStats();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNode::~FTimerNode()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNode::ResetAggregatedStats()
{
	AggregatedStats = TraceServices::FTimingProfilerAggregatedStats();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNode::SetAggregatedStats(const TraceServices::FTimingProfilerAggregatedStats& InAggregatedStats)
{
	AggregatedStats = InAggregatedStats;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTimerNode::GetSourceFileAndLine(FString& OutFile, uint32& OutLine) const
{
	bool bIsSourceFileValid = false;

	if (GetTimerId() != FTimerNode::InvalidTimerId)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
		{
			const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			const TraceServices::ITimingProfilerTimerReader* TimerReader = nullptr;
			TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });
			if (TimerReader)
			{
				const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(GetTimerId());
				if (Timer && Timer->File)
				{
					OutFile = FString(Timer->File);
					OutLine = Timer->Line;
					bIsSourceFileValid = true;
				}
			}
		}
	}
	if (!bIsSourceFileValid)
	{
		OutFile.Reset();
		OutLine = 0;
	}
	return bIsSourceFileValid;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
