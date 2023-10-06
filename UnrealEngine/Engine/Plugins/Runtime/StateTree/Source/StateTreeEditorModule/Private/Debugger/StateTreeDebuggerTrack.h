// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "RewindDebuggerTrack.h"
#include "SStateTreeDebuggerEventTimelineView.h"
#include "StateTreeExecutionTypes.h"

struct FStateTreeDebugger;

/** Base struct for Debugger tracks to append some functionalities not available in RewindDebuggerTrack */
struct FStateTreeDebuggerBaseTrack : RewindDebugger::FRewindDebuggerTrack
{
	explicit FStateTreeDebuggerBaseTrack(const FSlateIcon& Icon, const FText& TrackName)
		: Icon(Icon)
		, TrackName(TrackName)
	{
	}

	virtual bool IsStale() const { return false; }
	virtual void MarkAsStale() {}
	virtual void OnSelected() {}

protected:
	FSlateIcon Icon;
	FText TrackName;
};

/**
 * Struct use to store timeline events for a single StateTree instance
 * This is currently not tied to RewindDebugger but we use this base
 * type to facilitate a future integration.
 */
struct FStateTreeDebuggerInstanceTrack : FStateTreeDebuggerBaseTrack
{
	explicit FStateTreeDebuggerInstanceTrack(
		const TSharedPtr<FStateTreeDebugger>& InDebugger,
		const FStateTreeInstanceDebugId InInstanceId,
		const FText& InName,
		const TRange<double>& InViewRange);

protected:
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FName GetNameInternal() const override { return FName(TrackName.ToString()); }
	virtual FText GetDisplayNameInternal() const override { return TrackName; }

	virtual void OnSelected() override;
	virtual void MarkAsStale() override { bIsStale = true; }
	virtual bool IsStale() const override { return bIsStale; }

private:
	TSharedPtr<SStateTreeDebuggerEventTimelineView::FTimelineEventData> EventData;
	
	TSharedPtr<FStateTreeDebugger> StateTreeDebugger;
	FStateTreeInstanceDebugId InstanceId;
	const TRange<double>& ViewRange;
	bool bIsStale = false;
};


/** Parent track of all the statetree instance tracks sharing the same execution context owner */
struct FStateTreeDebuggerOwnerTrack : FStateTreeDebuggerBaseTrack
{
	explicit FStateTreeDebuggerOwnerTrack(const FText& InInstanceName);

	void AddSubTrack(const TSharedPtr<FStateTreeDebuggerInstanceTrack>& SubTrack) { SubTracks.Emplace(SubTrack); }
	int32 NumSubTracks() const { return SubTracks.Num(); }

protected:
	virtual bool UpdateInternal() override;	
	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FName GetNameInternal() const override { return FName(TrackName.ToString()); }
	virtual FText GetDisplayNameInternal() const override { return TrackName; }
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) override;

	virtual void MarkAsStale() override;
	virtual bool IsStale() const override;
private:
	TArray<TSharedPtr<FStateTreeDebuggerInstanceTrack>> SubTracks;
};

#endif // WITH_STATETREE_DEBUGGER