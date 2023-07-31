// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/MemoryTag.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTracker.h"
#include "Insights/ViewModels/GraphSeries.h"
#include "Insights/ViewModels/GraphTrack.h"

class FMemorySharedState;
class FSlateFontMeasure;

struct FSlateBrush;

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EGraphTrackLabelUnit
{
	Auto,
	Byte,
	KiB, // 2^10 bytes (kibibyte)
	MiB, // 2^20 bytes (mebibyte)
	GiB, // 2^30 bytes (gibibyte)
	TiB, // 2^40 bytes (tebibyte)
	PiB, // 2^50 bytes (pebibyte)
	EiB, // 2^60 bytes (exbibyte)
	AutoCount,
	Count,
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryGraphSeries : public FGraphSeries
{
public:
	enum class ETimelineType
	{
		MemTag,
		MinTotalMem,
		MaxTotalMem,
		MinLiveAllocs,
		MaxLiveAllocs,
		AllocEvents,
		FreeEvents
	};

public:
	virtual FString FormatValue(double Value) const override;

	Insights::FMemoryTrackerId GetTrackerId() const { return TrackerId; }
	void SetTrackerId(Insights::FMemoryTrackerId InTrackerId) { TrackerId = InTrackerId; }

	Insights::FMemoryTagId GetTagId() const { return TagId; }
	void SetTagId(Insights::FMemoryTagId InTagId) { TagId = InTagId; }

	ETimelineType GetTimelineType() const { return TimelineType; }
	void SetTimelineType(ETimelineType InTimelineType) { TimelineType = InTimelineType; }

	double GetMinValue() const { return MinValue; }
	double GetMaxValue() const { return MaxValue; }
	void SetValueRange(double Min, double Max) { MinValue = Min; MaxValue = Max; }

private:
	Insights::FMemoryTrackerId TrackerId = Insights::FMemoryTracker::InvalidTrackerId; // LLM tracker id
	Insights::FMemoryTagId TagId = Insights::FMemoryTag::InvalidTagId; // LLM tag id
	double MinValue = 0.0;
	double MaxValue = 0.0;
	ETimelineType TimelineType = ETimelineType::MemTag;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EMemoryTrackHeightMode
{
	Small = 0,
	Medium,
	Large,

	Count
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryGraphTrack : public FGraphTrack
{
	INSIGHTS_DECLARE_RTTI(FMemoryGraphTrack, FGraphTrack)

public:
	FMemoryGraphTrack(FMemorySharedState& InSharedState);
	virtual ~FMemoryGraphTrack();

	void SetLabelUnit(EGraphTrackLabelUnit InLabelUnit, int32 InLabelDecimalDigitCount) { LabelUnit = InLabelUnit; LabelDecimalDigitCount = InLabelDecimalDigitCount; }

	bool IsAutoZoomEnabled() const { return bAutoZoom; }
	void EnableAutoZoom() { bAutoZoom = true; }
	void DisableAutoZoom() { bAutoZoom = false; }
	void SetAutoZoom(bool bOnOff) { bAutoZoom = bOnOff; }

	void SetDefaultValueRange(double InDefaultMinValue, double InDefaultMaxValue) { DefaultMinValue = InDefaultMinValue; DefaultMaxValue = InDefaultMaxValue; }
	void ResetDefaultValueRange();

	bool IsStacked() const { return bIsStacked; }
	void SetStacked(bool bOnOff) { bIsStacked = bOnOff; }
	TSharedPtr<FMemoryGraphSeries> GetMainSeries() const { return MainSeries; }
	void SetMainSeries(TSharedPtr<FMemoryGraphSeries> InMainSeries) { MainSeries = InMainSeries; }

	virtual void Update(const ITimingTrackUpdateContext& Context) override;
	virtual void InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const override;

	TSharedPtr<FMemoryGraphSeries> GetMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	TSharedPtr<FMemoryGraphSeries> AddMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	int32 RemoveMemTagSeries(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId);
	int32 RemoveAllMemTagSeries();

	TSharedPtr<FMemoryGraphSeries> GetTimelineSeries(FMemoryGraphSeries::ETimelineType InTimelineType);
	TSharedPtr<FMemoryGraphSeries> AddTimelineSeries(FMemoryGraphSeries::ETimelineType InTimelineType);

	void SetAvailableTrackHeight(EMemoryTrackHeightMode InMode, float InTrackHeight);
	void SetCurrentTrackHeight(EMemoryTrackHeightMode InMode);

	static void GetUnit(const EGraphTrackLabelUnit InLabelUnit, const double InPrecision, double& OutUnitValue, const TCHAR*& OutUnitText);
	static FString FormatValue(const double InValue, const double InUnitValue, const TCHAR* InUnitText, const int32 InDecimalDigitCount);

protected:
	void PreUpdateMemTagSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport);
	void UpdateMemTagSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport);

	void PreUpdateAllocationsTimelineSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport);
	void UpdateAllocationsTimelineSeries(FMemoryGraphSeries& Series, const FTimingTrackViewport& Viewport);

	virtual void DrawVerticalAxisGrid(const ITimingTrackDrawContext& Context) const override;

	struct TDrawHorizontalAxisLabelParams
	{
		TDrawHorizontalAxisLabelParams(FDrawContext& InDrawContext, const FSlateBrush* InBrush, const TSharedRef<FSlateFontMeasure>& InFontMeasureService)
		: DrawContext(InDrawContext), Brush(InBrush), FontMeasureService(InFontMeasureService)
		{
		}

		FDrawContext& DrawContext;
		const FSlateBrush* Brush;
		const TSharedRef<FSlateFontMeasure>& FontMeasureService;
		FLinearColor TextBgColor;
		FLinearColor TextColor;
		float X;
		float Y;
		double Value;
		double Precision;
		bool bShowTextDetail;
		FString Prefix;
	};
	void DrawHorizontalAxisLabel(const TDrawHorizontalAxisLabelParams& Params) const;

protected:
	FMemorySharedState& SharedState;

	EGraphTrackLabelUnit LabelUnit;

	/**
	 * Number of decimal digits for labels.
	 * Specifies the number of decimal digits to use when formating labels of the vertical axis grid.
	 * If negative, the formatting will use maximum the number of decimal digits specified (trims trailing 0s),
	 * otherwise, it will use exactly the number of decimal digits specified.
	 */
	int32 LabelDecimalDigitCount;

	double DefaultMinValue;
	double DefaultMaxValue;
	double AllSeriesMinValue;
	double AllSeriesMaxValue;

	bool bAutoZoom; // all series will share same scale

	float AvailableTrackHeights[static_cast<uint32>(EMemoryTrackHeightMode::Count)];

	bool bIsStacked;
	TSharedPtr<FMemoryGraphSeries> MainSeries;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
