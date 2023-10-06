// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ViewModels/TimingEvent.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FContextSwitchTimingEvent : public FTimingEvent
{
	INSIGHTS_DECLARE_RTTI(FContextSwitchTimingEvent, FTimingEvent)

public:
	FContextSwitchTimingEvent(const TSharedRef<const FBaseTimingTrack> InTrack, double InStartTime, double InEndTime, uint32 InDepth);
	virtual ~FContextSwitchTimingEvent() {}

	void SetCoreNumber(uint32 InCoreNumber) { SetType((uint64)InCoreNumber); }
	uint32 GetCoreNumber() const { return (uint32)GetType(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FCpuCoreTimingEvent : public FTimingEvent
{
	INSIGHTS_DECLARE_RTTI(FCpuCoreTimingEvent, FTimingEvent)

public:
	FCpuCoreTimingEvent(const TSharedRef<const FBaseTimingTrack> InTrack, double InStartTime, double InEndTime, uint32 InDepth);
	virtual ~FCpuCoreTimingEvent() {}

	void SetSystemThreadId(uint32 InSystemThreadId) { SetType((uint64)InSystemThreadId); }
	uint32 GetSystemThreadId() const { return (uint32)GetType(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
