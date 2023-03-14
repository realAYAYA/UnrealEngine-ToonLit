// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ViewModels/TimingEvent.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API FThreadTrackEvent : public FTimingEvent
{
	INSIGHTS_DECLARE_RTTI(FThreadTrackEvent, FTimingEvent)

public:
	FThreadTrackEvent(const TSharedRef<const FBaseTimingTrack> InTrack, double InStartTime, double InEndTime, uint32 InDepth);
	virtual ~FThreadTrackEvent() {}

	uint32 GetTimerIndex() const;
	void SetTimerIndex(uint32 InTimerIndex);

	uint32 GetTimerId() const;
	void SetTimerId(uint32 InTimerId);

private:
	uint32 TimerIndex = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
