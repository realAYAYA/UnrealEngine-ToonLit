// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryGraphTrack.h"

#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

// TraceServices
#include "Common/ProviderLock.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Model/Memory.h"

// Insights
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/ViewModels/AxisViewportDouble.h"
#include "Insights/ViewModels/GraphTrackBuilder.h"
#include "Insights/ViewModels/ITimingViewDrawHelper.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TooltipDrawState.h"

#include <limits>

#define LOCTEXT_NAMESPACE "MemoryGraphTrack"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMemoryGraphSeries
////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr int32 DefaultDecimalDigitCount = 2;

FString FMemoryGraphSeries::FormatValue(double Value) const
{
	const int64 ValueInt64 = static_cast<int64>(Value);
	if (ValueInt64 == 0)
	{
		return TEXT("0");
	}

	if (TimelineType <= ETimelineType::MaxTotalMem)
	{
		double UnitValue;
		const TCHAR* UnitText;
		FMemoryGraphTrack::GetUnit(EGraphTrackLabelUnit::Auto, FMath::Abs(Value), UnitValue, UnitText);

		if (ValueInt64 < 0)
		{
			FString Auto = FMemoryGraphTrack::FormatValue(-Value, UnitValue, UnitText, DefaultDecimalDigitCount);
			return FString::Printf(TEXT("-%s (%s bytes)"), *Auto, *FText::AsNumber(ValueInt64).ToString());
		}
		else
		{
			FString Auto = FMemoryGraphTrack::FormatValue(Value, UnitValue, UnitText, DefaultDecimalDigitCount);
			return FString::Printf(TEXT("%s (%s bytes)"), *Auto, *FText::AsNumber(ValueInt64).ToString());
		}
	}
	else
	{
		return FText::AsNumber(ValueInt64).ToString();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMemoryGraphTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FMemoryGraphTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryGraphTrack::FMemoryGraphTrack(FMemorySharedState& InSharedState)
	: FGraphTrack()
	, SharedState(InSharedState)
	, LabelUnit(EGraphTrackLabelUnit::Auto)
	, LabelDecimalDigitCount(DefaultDecimalDigitCount)
	, DefaultMinValue(+std::numeric_limits<double>::infinity())
	, DefaultMaxValue(-std::numeric_limits<double>::infinity())
	, AllSeriesMinValue(0.0)
	, AllSeriesMaxValue(0.0)
	, bAutoZoom(false)
	, bIsStacked(false)
{
	EnabledOptions = //EGraphOptions::ShowDebugInfo |
					 //EGraphOptions::ShowPoints |
					 EGraphOptions::ShowPointsWithBorder |
					 EGraphOptions::ShowLines |
					 EGraphOptions::ShowPolygon |
					 EGraphOptions::UseEventDuration |
					 //EGraphOptions::ShowBars |
					 //EGraphOptions::ShowBaseline |
					 EGraphOptions::ShowVerticalAxisGrid |
					 EGraphOptions::ShowHeader |
					 EGraphOptions::None;

	for (uint32 Mode = 0; Mode < static_cast<uint32>(EMemoryTrackHeightMode::Count); ++Mode)
	{
		SetAvailableTrackHeight(static_cast<EMemoryTrackHeightMode>(Mode), 100.0f * static_cast<float>(Mode + 1));
	}
	SetHeight(200.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::ResetDefaultValueRange()
{
	DefaultMinValue = +std::numeric_limits<double>::infinity();
	DefaultMaxValue = -std::numeric_limits<double>::infinity();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryGraphTrack::~FMemoryGraphTrack()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::Update(const ITimingTrackUpdateContext& Context)
{
	FGraphTrack::Update(Context);

	const bool bIsEntireGraphTrackDirty = IsDirty() || Context.GetViewport().IsHorizontalViewportDirty();
	bool bNeedsUpdate = bIsEntireGraphTrackDirty;

	if (!bNeedsUpdate)
	{
		for (TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			if (Series->IsVisible() && Series->IsDirty())
			{
				// At least one series is dirty.
				bNeedsUpdate = true;
				break;
			}
		}
	}

	if (bNeedsUpdate)
	{
		ClearDirtyFlag();

		NumAddedEvents = 0;

		const FTimingTrackViewport& Viewport = Context.GetViewport();

		AllSeriesMinValue = DefaultMinValue;
		AllSeriesMaxValue = DefaultMaxValue;

		for (TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			if (Series->IsVisible())
			{
				//TODO: if (Series->Is<FMemoryGraphSeries>())
				TSharedPtr<FMemoryGraphSeries> MemorySeries = StaticCastSharedPtr<FMemoryGraphSeries>(Series);

				if (bIsEntireGraphTrackDirty || Series->IsDirty())
				{
					if (MemorySeries->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag)
					{
						PreUpdateMemTagSeries(*MemorySeries, Viewport);
					}
					else
					{
						PreUpdateAllocationsTimelineSeries(*MemorySeries, Viewport);
					}
				}

				const double CurrentSeriesMinValue = MemorySeries->GetMinValue();
				const double CurrentSeriesMaxValue = MemorySeries->GetMaxValue();
				if (CurrentSeriesMinValue < AllSeriesMinValue)
				{
					AllSeriesMinValue = CurrentSeriesMinValue;
				}
				if (CurrentSeriesMaxValue > AllSeriesMaxValue)
				{
					AllSeriesMaxValue = CurrentSeriesMaxValue;
				}
			}
		}

		if (bAutoZoom)
		{
			const float TopY = 4.0f;
			const float BottomY = GetHeight() - 4.0f;
			if (TopY < BottomY)
			{
				for (TSharedPtr<FGraphSeries>& Series : AllSeries)
				{
					if (Series->IsVisible())
					{
						if (Series->UpdateAutoZoomEx(TopY, BottomY, AllSeriesMinValue, AllSeriesMaxValue, true))
						{
							Series->SetDirtyFlag();
						}
					}
				}
			}
		}

		for (TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			if (Series->IsVisible() && (bIsEntireGraphTrackDirty || Series->IsDirty()))
			{
				// Clear the flag before updating, because the update itself may further need to set the series as dirty.
				Series->ClearDirtyFlag();

				//TODO: if (Series->Is<FMemoryGraphSeries>())
				TSharedPtr<FMemoryGraphSeries> MemorySeries = StaticCastSharedPtr<FMemoryGraphSeries>(Series);

				if (MemorySeries->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag)
				{
					UpdateMemTagSeries(*MemorySeries, Viewport);
				}
				else
				{
					UpdateAllocationsTimelineSeries(*MemorySeries, Viewport);
				}

				if (Series->IsAutoZoomDirty())
				{
					Series->SetDirtyFlag();
				}
			}
		}

		UpdateStats();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// LLM Tag Series
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphSeries> FMemoryGraphTrack::GetMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	TSharedPtr<FGraphSeries>* Ptr = AllSeries.FindByPredicate([InMemTrackerId, InMemTagId](const TSharedPtr<FGraphSeries>& Series)
	{
		//TODO: if (Series->Is<FMemoryGraphSeries>())
		const TSharedPtr<FMemoryGraphSeries> MemorySeries = StaticCastSharedPtr<FMemoryGraphSeries>(Series);
		return MemorySeries->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag &&
			   MemorySeries->GetTrackerId() == InMemTrackerId &&
			   MemorySeries->GetTagId() == InMemTagId;
	});
	return (Ptr != nullptr) ? StaticCastSharedPtr<FMemoryGraphSeries>(*Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphSeries> FMemoryGraphTrack::AddMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	TSharedPtr<FMemoryGraphSeries> Series = GetMemTagSeries(InMemTrackerId, InMemTagId);

	if (!Series.IsValid())
	{
		Series = MakeShared<FMemoryGraphSeries>();

		Series->SetName(TEXT("LLM Tag"));
		Series->SetDescription(TEXT("Low Level Memory Tag"));

		Series->SetTrackerId(InMemTrackerId);
		Series->SetTagId(InMemTagId);
		Series->SetValueRange(0.0f, 0.0f);

		Series->SetBaselineY(GetHeight() - 1.0f);
		Series->SetScaleY(1.0);

		AllSeries.Add(Series);
		SetDirtyFlag();
	}

	return Series;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemoryGraphTrack::RemoveMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	SetDirtyFlag();
	return AllSeries.RemoveAll([InMemTrackerId, InMemTagId](const TSharedPtr<FGraphSeries>& GraphSeries)
	{
		//TODO: if (GraphSeries->Is<FMemoryGraphSeries>())
		const TSharedPtr<FMemoryGraphSeries> Series = StaticCastSharedPtr<FMemoryGraphSeries>(GraphSeries);
		return Series->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag &&
			   Series->GetTrackerId() == InMemTrackerId &&
			   Series->GetTagId() == InMemTagId;
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemoryGraphTrack::RemoveAllMemTagSeries()
{
	SetDirtyFlag();
	return AllSeries.RemoveAll([](const TSharedPtr<FGraphSeries>& GraphSeries)
	{
		//TODO: if (GraphSeries->Is<FMemoryGraphSeries>())
		const TSharedPtr<FMemoryGraphSeries> Series = StaticCastSharedPtr<FMemoryGraphSeries>(GraphSeries);
		return Series->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag;
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::PreUpdateMemTagSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::IMemoryProvider* MemoryProvider = TraceServices::ReadMemoryProvider(*Session.Get());
		if (MemoryProvider)
		{
			const uint64 TotalSampleCount = MemoryProvider->GetTagSampleCount(Series.GetTrackerId(), Series.GetTagId());
			if (TotalSampleCount > 0)
			{
				double MinValue = +std::numeric_limits<double>::infinity();
				double MaxValue = -std::numeric_limits<double>::infinity();

				// Compute Min/Max values.
				MemoryProvider->EnumerateTagSamples(Series.GetTrackerId(), Series.GetTagId(), Viewport.GetStartTime(), Viewport.GetEndTime(), true,
					[&MinValue, &MaxValue](double Time, double Duration, const TraceServices::FMemoryTagSample& Sample)
				{
					const double Value = static_cast<double>(Sample.Value);
					if (Value < MinValue)
					{
						MinValue = Value;
					}
					if (Value > MaxValue)
					{
						MaxValue = Value;
					}
				});

				Series.SetValueRange(MinValue, MaxValue);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::UpdateMemTagSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::IMemoryProvider* MemoryProvider = TraceServices::ReadMemoryProvider(*Session.Get());
		if (MemoryProvider)
		{
			const uint64 TotalSampleCount = MemoryProvider->GetTagSampleCount(Series.GetTrackerId(), Series.GetTagId());

			if (TotalSampleCount > 0)
			{
				const float TopY = 4.0f;
				const float BottomY = GetHeight() - 4.0f;

				if (Series.IsAutoZoomEnabled() && TopY < BottomY)
				{
					Series.UpdateAutoZoom(TopY, BottomY, Series.GetMinValue(), Series.GetMaxValue());
				}

				MemoryProvider->EnumerateTagSamples(Series.GetTrackerId(), Series.GetTagId(), Viewport.GetStartTime(), Viewport.GetEndTime(), true,
					[this, &Builder](double Time, double Duration, const TraceServices::FMemoryTagSample& Sample)
				{
					Builder.AddEvent(Time, Duration, static_cast<double>(Sample.Value));
				});
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphSeries> FMemoryGraphTrack::GetTimelineSeries(FMemoryGraphSeries::ETimelineType InTimelineType)
{
	return nullptr; //TODO
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphSeries> FMemoryGraphTrack::AddTimelineSeries(FMemoryGraphSeries::ETimelineType InTimelineType)
{
	TSharedPtr<FMemoryGraphSeries> Series = GetTimelineSeries(InTimelineType);

	if (!Series.IsValid())
	{
		Series = MakeShared<FMemoryGraphSeries>();

		switch (InTimelineType)
		{
			case FMemoryGraphSeries::ETimelineType::MinTotalMem:
			{
				Series->SetName(TEXT("Total Allocated Memory (Min)"));
				Series->SetDescription(TEXT("Minimum value per sample for the Total Allocated Memory"));
				const FLinearColor Color = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}

			case FMemoryGraphSeries::ETimelineType::MaxTotalMem:
			{
				Series->SetName(TEXT("Total Allocated Memory (Max)"));
				Series->SetDescription(TEXT("Maximum value per sample for the Total Allocated Memory"));
				const FLinearColor Color = FLinearColor(1.0f, 0.25f, 1.0f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}

			case FMemoryGraphSeries::ETimelineType::MinLiveAllocs:
			{
				Series->SetName(TEXT("Live Allocation Count (Min)"));
				Series->SetDescription(TEXT("Minimum value per sample for the Live Allocation Count"));
				const FLinearColor Color = FLinearColor(1.0f, 1.0f, 0.25f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}

			case FMemoryGraphSeries::ETimelineType::MaxLiveAllocs:
			{
				Series->SetName(TEXT("Live Allocation Count (Max)"));
				Series->SetDescription(TEXT("Maximum value per sample for the Live Allocation Count"));
				const FLinearColor Color = FLinearColor(1.0f, 0.25f, 1.0f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}

			case FMemoryGraphSeries::ETimelineType::AllocEvents:
			{
				Series->SetName(TEXT("Alloc Event Count"));
				Series->SetDescription(TEXT("Number of alloc events per sample"));
				const FLinearColor Color = FLinearColor(0.0f, 1.0f, 0.5f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}

			case FMemoryGraphSeries::ETimelineType::FreeEvents:
			{
				Series->SetName(TEXT("Free Event Count"));
				Series->SetDescription(TEXT("Number of free events per sample"));
				const FLinearColor Color = FLinearColor(1.0f, 0.5f, 0.25f, 1.0f);
				const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
				Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
				break;
			}
		}

		Series->SetTimelineType(InTimelineType);
		Series->SetValueRange(0.0f, 0.0f);

		Series->SetBaselineY(GetHeight() - 1.0f);
		Series->SetScaleY(1.0);

		AllSeries.Add(Series);
		SetDirtyFlag();
	}

	return Series;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::PreUpdateAllocationsTimelineSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
		if (AllocationsProvider)
		{
			TraceServices::FProviderReadScopeLock ProviderReadScope(*AllocationsProvider);

			int32 StartIndex = -1;
			int32 EndIndex = -1;
			AllocationsProvider->GetTimelineIndexRange(Viewport.GetStartTime(), Viewport.GetEndTime(), StartIndex, EndIndex);
			if (EndIndex >= 0)
			{
				--StartIndex; // include one more point on the left side
				++EndIndex; // include one more point on the right side
			}

			uint64 MinValue = std::numeric_limits<uint64>::max();
			uint64 MaxValue = 0;

			// Compute Min/Max values.
			auto Callback64 = [&MinValue, &MaxValue](double Time, double Duration, uint64 Value)
			{
				if (Value < MinValue)
				{
					MinValue = Value;
				}
				if (Value > MaxValue)
				{
					MaxValue = Value;
				}
			};
			auto Callback32 = [&MinValue, &MaxValue](double Time, double Duration, uint32 Value)
			{
				if (Value < MinValue)
				{
					MinValue = Value;
				}
				if (Value > MaxValue)
				{
					MaxValue = Value;
				}
			};

			switch (Series.GetTimelineType())
			{
			case FMemoryGraphSeries::ETimelineType::MinTotalMem:
				AllocationsProvider->EnumerateMinTotalAllocatedMemoryTimeline(StartIndex, EndIndex, Callback64);
				break;
			case FMemoryGraphSeries::ETimelineType::MaxTotalMem:
				AllocationsProvider->EnumerateMaxTotalAllocatedMemoryTimeline(StartIndex, EndIndex, Callback64);
				break;
			case FMemoryGraphSeries::ETimelineType::MinLiveAllocs:
				AllocationsProvider->EnumerateMinLiveAllocationsTimeline(StartIndex, EndIndex, Callback32);
				break;
			case FMemoryGraphSeries::ETimelineType::MaxLiveAllocs:
				AllocationsProvider->EnumerateMaxLiveAllocationsTimeline(StartIndex, EndIndex, Callback32);
				break;
			case FMemoryGraphSeries::ETimelineType::AllocEvents:
				AllocationsProvider->EnumerateAllocEventsTimeline(StartIndex, EndIndex, Callback32);
				break;
			case FMemoryGraphSeries::ETimelineType::FreeEvents:
				AllocationsProvider->EnumerateFreeEventsTimeline(StartIndex, EndIndex, Callback32);
				break;
			}

			if (Series.GetTimelineType() == FMemoryGraphSeries::ETimelineType::FreeEvents)
			{
				// Shows FreeEvents as negative values in order to be displayed on same graph as AllocEvents.
				Series.SetValueRange(-static_cast<double>(MaxValue), -static_cast<double>(MinValue));
			}
			else
			{
				Series.SetValueRange(static_cast<double>(MinValue), static_cast<double>(MaxValue));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::UpdateAllocationsTimelineSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
		if (AllocationsProvider)
		{
			struct FTimelineEvent
			{
				FTimelineEvent(double InTime, double InDuration, double InValue) :
					Time(InTime),
					Duration(InDuration),
					Value(InValue)
				{

				}
				double Time;
				double Duration;
				double Value;
			};

			int32 StartIndex = -1;
			int32 EndIndex = -1;

			TArray<FTimelineEvent> TimelineEvents;
			{
				TraceServices::FProviderReadScopeLock ProviderReadScope(*AllocationsProvider);
				AllocationsProvider->GetTimelineIndexRange(Viewport.GetStartTime(), Viewport.GetEndTime(), StartIndex, EndIndex);

				if (EndIndex >= 0)
				{
					--StartIndex; // include one more point on the left side
					++EndIndex; // include one more point on the right side
				}

				TimelineEvents.Reserve((EndIndex - StartIndex) + 1);

				const float TopY = 4.0f;
				const float BottomY = GetHeight() - 4.0f;

				if (Series.IsAutoZoomEnabled() && TopY < BottomY)
				{
					Series.UpdateAutoZoom(TopY, BottomY, Series.GetMinValue(), Series.GetMaxValue());
				}

				auto Callback64 = [&TimelineEvents](double Time, double Duration, uint64 Value)
				{
					TimelineEvents.Emplace(Time, Duration, static_cast<double>(Value));
				};
				auto Callback32 = [&TimelineEvents](double Time, double Duration, uint32 Value)
				{
					TimelineEvents.Emplace(Time, Duration, static_cast<double>(Value));
				};
				auto Callback32Negative = [&TimelineEvents](double Time, double Duration, uint32 Value)
				{
					// Shows FreeEvents as negative values in order to be displayed on same graph as AllocEvents.
					TimelineEvents.Emplace(Time, Duration, -static_cast<double>(Value));
				};

				switch (Series.GetTimelineType())
				{
				case FMemoryGraphSeries::ETimelineType::MinTotalMem:
					AllocationsProvider->EnumerateMinTotalAllocatedMemoryTimeline(StartIndex, EndIndex, Callback64);
					break;
				case FMemoryGraphSeries::ETimelineType::MaxTotalMem:
					AllocationsProvider->EnumerateMaxTotalAllocatedMemoryTimeline(StartIndex, EndIndex, Callback64);
					break;
				case FMemoryGraphSeries::ETimelineType::MinLiveAllocs:
					AllocationsProvider->EnumerateMinLiveAllocationsTimeline(StartIndex, EndIndex, Callback32);
					break;
				case FMemoryGraphSeries::ETimelineType::MaxLiveAllocs:
					AllocationsProvider->EnumerateMaxLiveAllocationsTimeline(StartIndex, EndIndex, Callback32);
					break;
				case FMemoryGraphSeries::ETimelineType::AllocEvents:
					AllocationsProvider->EnumerateAllocEventsTimeline(StartIndex, EndIndex, Callback32);
					break;
				case FMemoryGraphSeries::ETimelineType::FreeEvents:
					AllocationsProvider->EnumerateFreeEventsTimeline(StartIndex, EndIndex, Callback32Negative);
					break;
				}
			}

			for (const FTimelineEvent& TimelineEvent : TimelineEvents)
			{
				Builder.AddEvent(TimelineEvent.Time, TimelineEvent.Duration, TimelineEvent.Value);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::DrawVerticalAxisGrid(const ITimingTrackDrawContext& Context) const
{
	TSharedPtr<FMemoryGraphSeries> Series = MainSeries;

	if (!Series.IsValid())
	{
		// Use the first visible series.
		for (const TSharedPtr<FGraphSeries>& GraphSeries : AllSeries)
		{
			if (GraphSeries->IsVisible())
			{
				//TODO: if (GraphSeries->Is<FMemoryGraphSeries>())
				Series = StaticCastSharedPtr<FMemoryGraphSeries>(GraphSeries);
				break;
			}
		}
	}

	if (!Series.IsValid())
	{
		return;
	}

	FAxisViewportDouble ViewportY;
	ViewportY.SetSize(GetHeight());
	ViewportY.SetScaleLimits(std::numeric_limits<double>::min(), std::numeric_limits<double>::max());
	ViewportY.SetScale(Series->GetScaleY());
	ViewportY.ScrollAtPos(static_cast<float>(Series->GetBaselineY()) - GetHeight());

	const float ViewWidth = Context.GetViewport().GetWidth();
	const float RoundedViewHeight = FMath::RoundToFloat(GetHeight());

	const float X0 = ViewWidth - 12.0f; // let some space for the vertical scrollbar
	const float Y0 = GetPosY();

	constexpr float MinDY = 32.0f; // min vertical distance between horizontal grid lines
	constexpr float TextH = 14.0f; // label height

	const float MinLabelY = Y0 + 1.0f;
	const float MaxLabelY = Y0 + RoundedViewHeight - TextH;

	float MinValueY = Y0 - MinDY; // a value below the track
	float MaxValueY = Y0 + RoundedViewHeight + MinDY; // a value above the track
	float ActualMinValueY = MinValueY;
	float ActualMaxValueY = MaxValueY;

	const bool bHasMinMax = (AllSeriesMinValue <= AllSeriesMaxValue);
	if (bHasMinMax)
	{
		const float MinValueOffset = ViewportY.GetOffsetForValue(AllSeriesMinValue);
		const float MinValueRoundedOffset = FMath::RoundToFloat(MinValueOffset);
		ActualMinValueY = Y0 + RoundedViewHeight - MinValueRoundedOffset;
		MinValueY = FMath::Min(MaxLabelY, FMath::Max(MinLabelY, ActualMinValueY - TextH / 2));

		const float MaxValueOffset = ViewportY.GetOffsetForValue(AllSeriesMaxValue);
		const float MaxValueRoundedOffset = FMath::RoundToFloat(MaxValueOffset);
		ActualMaxValueY = Y0 + RoundedViewHeight - MaxValueRoundedOffset;
		MaxValueY = FMath::Min(MaxLabelY, FMath::Max(MinLabelY, ActualMaxValueY - TextH / 2));
	}

	FDrawContext& DrawContext = Context.GetDrawContext();
	const FSlateBrush* Brush = Context.GetHelper().GetWhiteBrush();
	//const FSlateFontInfo& Font = Context.GetHelper().GetEventFont();
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	const double TopValue = ViewportY.GetValueAtOffset(RoundedViewHeight);
	const double GridValue = ViewportY.GetValueAtOffset(MinDY);
	const double BottomValue = ViewportY.GetValueAtOffset(0.0f);
	const double Delta = GridValue - BottomValue;

	double Precision = FMath::Abs(TopValue - BottomValue);
	for (int32 Digit = FMath::Abs(LabelDecimalDigitCount); Digit > 0; --Digit)
	{
		Precision *= 10.0;
	}

	if (Delta > 0.0)
	{
		double Grid;

		if (Series->GetTimelineType() <= FMemoryGraphSeries::ETimelineType::MaxTotalMem)
		{
			const uint64 DeltaBytes = FMath::Max(1ULL, static_cast<uint64>(Delta));
			Grid = static_cast<double>(FMath::RoundUpToPowerOfTwo64(DeltaBytes));
		}
		else
		{
			const uint64 DeltaCount = FMath::Max(1ULL, static_cast<uint64>(Delta));

			// Compute rounding based on magnitude of visible range of values (Delta).
			uint64 Delta10 = DeltaCount;
			uint64 Power10 = 1;
			while (Delta10 > 0)
			{
				Delta10 /= 10;
				Power10 *= 10;
			}
			if (Power10 >= 100)
			{
				Power10 /= 100;
			}
			else
			{
				Power10 = 1;
			}

			// Compute Grid as the next value divisible with a multiple of 10.
			Grid = static_cast<double>(((DeltaCount + Power10 - 1) / Power10) * Power10);
		}

		const double StartValue = FMath::GridSnap(BottomValue, Grid);

		TDrawHorizontalAxisLabelParams Params(DrawContext, Brush, FontMeasureService);
		Params.TextBgColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
		Params.TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		Params.X = X0;
		Params.Precision = Precision;

		const FLinearColor GridColor(0.0f, 0.0f, 0.0f, 0.1f);

		for (double Value = StartValue; Value < TopValue; Value += Grid)
		{
			const float Y = Y0 + RoundedViewHeight - FMath::RoundToFloat(ViewportY.GetOffsetForValue(Value));

			const float LabelY = FMath::Min(MaxLabelY, FMath::Max(MinLabelY, Y - TextH / 2));

			// Do not overlap with the min/max values.
			if (bHasMinMax && (FMath::Abs(LabelY - MinValueY) < TextH || FMath::Abs(LabelY - MaxValueY) < TextH))
			{
				continue;
			}

			// Draw horizontal grid line.
			DrawContext.DrawBox(0, Y, ViewWidth, 1, Brush, GridColor);

			// Draw label.
			Params.Y = LabelY;
			Params.Value = Value;
			DrawHorizontalAxisLabel(Params);
		}
	}

	if (bHasMinMax && GetHeight() >= TextH)
	{
		TDrawHorizontalAxisLabelParams Params(DrawContext, Brush, FontMeasureService);

		if (MainSeries.IsValid() || AllSeries.Num() == 1)
		{
			Params.TextBgColor = (Series->GetColor() * 0.05f).CopyWithNewOpacity(1.0f);
			Params.TextColor= Series->GetBorderColor().CopyWithNewOpacity(1.0f);
		}
		else
		{
			Params.TextBgColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);
			Params.TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		}

		Params.X = X0;
		Params.Precision = -Precision; // format with detailed text

		int32 MinMaxAxis = 0;

		// Draw horizontal axis at the max value.
		if (MaxValueY >= Y0 && MaxValueY <= Y0 + RoundedViewHeight)
		{
			Params.Y = MaxValueY;
			Params.Value = AllSeriesMaxValue;
			DrawHorizontalAxisLabel(Params);
			++MinMaxAxis;
		}

		// Draw horizontal axis at the min value.
		if (MinValueY >= Y0 && MinValueY <= Y0 + RoundedViewHeight && FMath::Abs(MaxValueY - MinValueY) > TextH)
		{
			Params.Y = MinValueY;
			Params.Value = AllSeriesMinValue;
			DrawHorizontalAxisLabel(Params);
			++MinMaxAxis;
		}

		if (MinMaxAxis == 2)
		{
			const float MX = static_cast<float>(Context.GetMousePosition().X);
			const float MY = static_cast<float>(Context.GetMousePosition().Y);

			//constexpr float MX1 = 80.0f; // start fading out
			constexpr float MX2 = 120.0f; // completly faded out

			if (MX > ViewWidth - MX2 && MY >= MaxValueY && MY < MinValueY + TextH)
			{
				const float LineX = MX - 16.0f;
				DrawContext.DrawBox(DrawContext.LayerId + 1, LineX, ActualMaxValueY, X0 - LineX, 1.0f, Params.Brush, Params.TextBgColor);
				DrawContext.DrawBox(DrawContext.LayerId + 1, LineX, ActualMaxValueY, 1.0f, ActualMinValueY - ActualMaxValueY, Params.Brush, Params.TextBgColor);
				DrawContext.DrawBox(DrawContext.LayerId + 1, LineX, ActualMinValueY, X0 - LineX, 1.0f, Params.Brush, Params.TextBgColor);

				DrawContext.LayerId += 3; // ensure to draw on top of other labels

				Params.X = MX;
				Params.Y = MY - TextH / 2;
				Params.Value = AllSeriesMaxValue - AllSeriesMinValue;
				Params.Precision = -Precision * 0.1; // format with detailed text and increased precision
				Params.Prefix = TEXT("\u0394=");
				DrawHorizontalAxisLabel(Params);
			}
		}
	}

	DrawContext.LayerId += 3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::DrawHorizontalAxisLabel(const TDrawHorizontalAxisLabelParams& Params) const
{
	FString LabelText = Params.Prefix;

	if (FMath::IsNearlyZero(Params.Value, 0.5))
	{
		LabelText += TEXT("0");
	}
	else
	{
		double UnitValue;
		const TCHAR* UnitText;
		GetUnit(LabelUnit, FMath::Abs(Params.Precision), UnitValue, UnitText);

		LabelText += FormatValue(Params.Value, UnitValue, UnitText, LabelDecimalDigitCount);

		if (Params.Precision < 0 && LabelUnit == EGraphTrackLabelUnit::Auto)
		{
			double ValueUnitValue;
			const TCHAR* ValueUnitText;
			GetUnit(LabelUnit, Params.Value, ValueUnitValue, ValueUnitText);
			if (ValueUnitValue > UnitValue)
			{
				FString LabelTextDetail = FormatValue(Params.Value, ValueUnitValue, ValueUnitText, LabelDecimalDigitCount);
				LabelText += TEXT(" (");
				LabelText += LabelTextDetail;
				LabelText += TEXT(")");
			}
		}
	}

	const float FontScale = Params.DrawContext.Geometry.Scale;
	const FVector2D TextSize = Params.FontMeasureService->Measure(LabelText, Font, FontScale) / FontScale;
	const float TextW = static_cast<float>(TextSize.X);
	constexpr float TextH = 14.0f;

	// Draw background for value text.
	Params.DrawContext.DrawBox(Params.DrawContext.LayerId + 1, Params.X - TextW - 4.0f, Params.Y, TextW + 5.0f, TextH, Params.Brush, Params.TextBgColor);

	// Draw value text.
	Params.DrawContext.DrawText(Params.DrawContext.LayerId + 2, Params.X - TextW - 2.0f, Params.Y + 1.0f, LabelText, Font, Params.TextColor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::GetUnit(const EGraphTrackLabelUnit InLabelUnit, const double InPrecision, double& OutUnitValue, const TCHAR*& OutUnitText)
{
	constexpr double KiB = (double)(1LL << 10); // 2^10 bytes
	constexpr double MiB = (double)(1LL << 20); // 2^20 bytes
	constexpr double GiB = (double)(1LL << 30); // 2^30 bytes
	constexpr double TiB = (double)(1LL << 40); // 2^40 bytes
	constexpr double PiB = (double)(1LL << 50); // 2^50 bytes
	constexpr double EiB = (double)(1LL << 60); // 2^60 bytes

	constexpr double K10 = 1000.0;    // 10^3
	constexpr double M10 = K10 * K10; // 10^6
	constexpr double G10 = M10 * K10; // 10^9
	constexpr double T10 = G10 * K10; // 10^12
	constexpr double P10 = T10 * K10; // 10^15
	constexpr double E10 = P10 * K10; // 10^18

	switch (InLabelUnit)
	{
		case EGraphTrackLabelUnit::Auto:
		{
			if (InPrecision >= EiB)
			{
				OutUnitValue = EiB;
				OutUnitText = TEXT("EiB");
			}
			else if (InPrecision >= PiB)
			{
				OutUnitValue = PiB;
				OutUnitText = TEXT("PiB");
			}
			else if (InPrecision >= TiB)
			{
				OutUnitValue = TiB;
				OutUnitText = TEXT("TiB");
			}
			else if (InPrecision >= GiB)
			{
				OutUnitValue = GiB;
				OutUnitText = TEXT("GiB");
			}
			else if (InPrecision >= MiB)
			{
				OutUnitValue = MiB;
				OutUnitText = TEXT("MiB");
			}
			else if (InPrecision >= KiB)
			{
				OutUnitValue = KiB;
				OutUnitText = TEXT("KiB");
			}
			else
			{
				OutUnitValue = 1.0;
				OutUnitText = TEXT("B");
			}
		}
		break;

		case EGraphTrackLabelUnit::KiB:
			OutUnitValue = KiB;
			OutUnitText = TEXT("KiB");
			break;

		case EGraphTrackLabelUnit::MiB:
			OutUnitValue = MiB;
			OutUnitText = TEXT("MiB");
			break;

		case EGraphTrackLabelUnit::GiB:
			OutUnitValue = GiB;
			OutUnitText = TEXT("GiB");
			break;

		case EGraphTrackLabelUnit::TiB:
			OutUnitValue = TiB;
			OutUnitText = TEXT("TiB");
			break;

		case EGraphTrackLabelUnit::PiB:
			OutUnitValue = PiB;
			OutUnitText = TEXT("PiB");
			break;

		case EGraphTrackLabelUnit::EiB:
			OutUnitValue = EiB;
			OutUnitText = TEXT("EiB");
			break;

		case EGraphTrackLabelUnit::Byte:
			OutUnitValue = 1.0;
			OutUnitText = TEXT("B");
			break;

		case EGraphTrackLabelUnit::AutoCount:
		{
			if (InPrecision >= E10)
			{
				OutUnitValue = E10;
				OutUnitText = TEXT("E");
			}
			else if (InPrecision >= P10)
			{
				OutUnitValue = P10;
				OutUnitText = TEXT("P");
			}
			else if (InPrecision >= T10)
			{
				OutUnitValue = T10;
				OutUnitText = TEXT("T");
			}
			else if (InPrecision >= G10)
			{
				OutUnitValue = G10;
				OutUnitText = TEXT("G");
			}
			else if (InPrecision >= M10)
			{
				OutUnitValue = M10;
				OutUnitText = TEXT("M");
			}
			else if (InPrecision >= K10)
			{
				OutUnitValue = K10;
				OutUnitText = TEXT("K");
			}
			else
			{
				OutUnitValue = 1.0;
				OutUnitText = TEXT("");
			}
		}
		break;

		case EGraphTrackLabelUnit::Count:
		default:
			OutUnitValue = 1.0;
			OutUnitText = TEXT("");
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FMemoryGraphTrack::FormatValue(const double InValue, const double InUnitValue, const TCHAR* InUnitText, const int32 InDecimalDigitCount)
{
	if (InUnitText[0] == TEXT('\0') && InDecimalDigitCount == 0)
	{
		return FText::AsNumber(static_cast<int64>(InValue)).ToString();
	}

	FString OutText;

	TCHAR FormatString[32];
	FCString::Snprintf(FormatString, sizeof(FormatString), TEXT("%%.%df"), FMath::Abs(InDecimalDigitCount));
	OutText = FString::Printf(FormatString, InValue / InUnitValue);

	if (InDecimalDigitCount < 0)
	{
		// Remove ending 0s.
		while (OutText.Len() > 0 && OutText[OutText.Len() - 1] == TEXT('0'))
		{
			OutText.RemoveAt(OutText.Len() - 1, 1);
		}
		// Remove ending dot.
		if (OutText.Len() > 0 && OutText[OutText.Len() - 1] == TEXT('.'))
		{
			OutText.RemoveAt(OutText.Len() - 1, 1);
		}
	}

	if (InUnitText[0] != TEXT('\0'))
	{
		OutText += TEXT(' ');
		OutText += InUnitText;
	}

	return OutText;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::SetAvailableTrackHeight(EMemoryTrackHeightMode InMode, float InTrackHeight)
{
	check(static_cast<uint32>(InMode) < static_cast<uint32>(EMemoryTrackHeightMode::Count));
	AvailableTrackHeights[static_cast<uint32>(InMode)] = InTrackHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::SetCurrentTrackHeight(EMemoryTrackHeightMode InMode)
{
	check(static_cast<uint32>(InMode) < static_cast<uint32>(EMemoryTrackHeightMode::Count));
	SetHeight(AvailableTrackHeights[static_cast<uint32>(InMode)]);

	const float TrackHeight = GetHeight();
	for (TSharedPtr<FGraphSeries> Series : AllSeries)
	{
		Series->SetBaselineY(TrackHeight - 1.0f);
		Series->SetDirtyFlag();
	}

	SetDirtyFlag();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemoryGraphTrack::InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const
{
	if (InTooltipEvent.CheckTrack(this) && InTooltipEvent.Is<FGraphTrackEvent>())
	{
		const FGraphTrackEvent& TooltipEvent = InTooltipEvent.As<FGraphTrackEvent>();
		const TSharedRef<const FMemoryGraphSeries> Series = StaticCastSharedRef<const FMemoryGraphSeries, const FGraphSeries>(TooltipEvent.GetSeries());

		InOutTooltip.ResetContent();
		InOutTooltip.AddTitle(Series->GetName().ToString(), Series->GetColor());

		if (Series->GetTimelineType() == FMemoryGraphSeries::ETimelineType::MemTag)
		{
			FString SubTitle = FString::Printf(TEXT("(tag id 0x%llX, tracker id %i)"), (uint64)Series->GetTagId(), (int32)Series->GetTrackerId());
			InOutTooltip.AddTitle(SubTitle, Series->GetColor());
		}

		const double Precision = FMath::Max(1.0 / TimeScaleX, TimeUtils::Nanosecond);
		InOutTooltip.AddNameValueTextLine(TEXT("Time:"), TimeUtils::FormatTime(TooltipEvent.GetStartTime(), Precision));
		if (Series->HasEventDuration())
		{
			InOutTooltip.AddNameValueTextLine(TEXT("Duration:"), TimeUtils::FormatTimeAuto(TooltipEvent.GetDuration()));
		}
		InOutTooltip.AddNameValueTextLine(TEXT("Value:"), Series->FormatValue(TooltipEvent.GetValue()));
		InOutTooltip.UpdateLayout();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
