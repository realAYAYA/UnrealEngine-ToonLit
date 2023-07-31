// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTrack.h"
#include "Insights/ViewModels/TimingEventsTrack.h"

class FGameplaySharedData;
struct FObjectPropertiesMessage;
class FTimingEventSearchParameters;
struct FWorldInfo;
namespace TraceServices { struct FFrame; }

class FObjectPropertiesTrack : public FGameplayTimingEventsTrack
{
	INSIGHTS_DECLARE_RTTI(FObjectPropertiesTrack, FGameplayTimingEventsTrack)

public:
	FObjectPropertiesTrack(const FGameplaySharedData& InSharedData, uint64 InObjectID, const TCHAR* InName);

	virtual void BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) override;
	virtual void Draw(const ITimingTrackDrawContext& Context) const override;
	virtual void InitTooltip(FTooltipDrawState& Tooltip, const ITimingEvent& HoveredTimingEvent) const override;
	virtual const TSharedPtr<const ITimingEvent> SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const override;
	virtual void GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const override;

private:
	// Helper function used to find an object event
	void FindObjectEvent(const FTimingEventSearchParameters& InParameters, TFunctionRef<void(double, double, uint32, const FObjectPropertiesMessage&)> InFoundPredicate) const;

	// Helper function to build the track's name
	FText MakeTrackName(const FGameplaySharedData& InSharedData, uint64 InObjectID, const TCHAR* InName) const;

private:
	const FGameplaySharedData& SharedData;
};
