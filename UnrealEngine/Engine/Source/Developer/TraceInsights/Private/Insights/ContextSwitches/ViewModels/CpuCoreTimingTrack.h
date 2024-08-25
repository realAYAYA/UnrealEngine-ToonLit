// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ViewModels/TimingEventsTrack.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FThreadTrackEvent;
struct FTimingEventsTrackDrawState;

namespace TraceServices
{
	struct FCpuCoreEvent;
};

namespace Insights
{

class FContextSwitchesSharedState;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FCpuCoreTimingTrack : public FTimingEventsTrack
{
	INSIGHTS_DECLARE_RTTI(FCpuCoreTimingTrack, FTimingEventsTrack)

public:
	explicit FCpuCoreTimingTrack(FContextSwitchesSharedState& InSharedState, const FString& InName, uint32 InCoreNumber);

	virtual ~FCpuCoreTimingTrack() {}

	uint32 GetCoreNumber() const { return CoreNumber; }

	virtual void Reset() override;

	virtual void BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) override;
	virtual void BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) override;

	virtual void Draw(const ITimingTrackDrawContext& Context) const override;

	virtual void PostDraw(const ITimingTrackDrawContext& Context) const override;

	virtual void InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const override;

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder) override;

	virtual const TSharedPtr<const ITimingEvent> SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const override;

protected:
	virtual const TSharedPtr<const ITimingEvent> GetEvent(double InTime, double SecondsPerPixel, int32 Depth) const override;

	virtual bool HasCustomFilter() const override;

	void AddCoreTimingEvent(ITimingEventsTrackDrawStateBuilder& Builder, const TraceServices::FCpuCoreEvent& CpuCoreEvent);

	FString GetThreadName(uint32 InSystemThreadId) const;

	virtual int32 GetMaxDepth() const override { return NonTargetProcessEventsMaxDepth; }

private:
	FContextSwitchesSharedState& SharedState;

	uint32 CoreNumber;

	TSharedRef<FTimingEventsTrackDrawState> NonTargetProcessEventsDrawState;

	int32 NonTargetProcessEventsMaxDepth = -1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
