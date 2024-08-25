// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "Debugger/StateTreeDebuggerTrack.h"
#include "Debugger/StateTreeDebugger.h"
#include "SStateTreeDebuggerEventTimelineView.h"

//----------------------------------------------------------------------//
// FStateTreeDebuggerInstanceTrack
//----------------------------------------------------------------------//
FStateTreeDebuggerInstanceTrack::FStateTreeDebuggerInstanceTrack(
	const TSharedPtr<FStateTreeDebugger>& InDebugger,
	const FStateTreeInstanceDebugId InInstanceId,
	const FText& InName,
	const TRange<double>& InViewRange
	)
	: FStateTreeDebuggerBaseTrack(FSlateIcon("StateTreeEditorStyle", "StateTreeEditor.Debugger.InstanceTrack", "StateTreeEditor.Debugger.InstanceTrack"), InName)
	, StateTreeDebugger(InDebugger)
	, InstanceId(InInstanceId)
	, ViewRange(InViewRange)
{
	EventData = MakeShared<SStateTreeDebuggerEventTimelineView::FTimelineEventData>();
}

void FStateTreeDebuggerInstanceTrack::OnSelected()
{
	if (FStateTreeDebugger* Debugger = StateTreeDebugger.Get())
	{
		Debugger->SelectInstance(InstanceId);
	}
}

bool FStateTreeDebuggerInstanceTrack::UpdateInternal()
{
	const int32 PrevNumPoints = EventData->Points.Num();
	const int32 PrevNumWindows = EventData->Windows.Num();

	EventData->Points.SetNum(0, EAllowShrinking::No);
	EventData->Windows.SetNum(0);
	
	const FStateTreeDebugger* Debugger = StateTreeDebugger.Get();
	check(Debugger);
	const UStateTree* StateTree = Debugger->GetAsset();
	const UE::StateTreeDebugger::FInstanceEventCollection& EventCollection = Debugger->GetEventCollection(InstanceId);
	const double RecordingDuration = Debugger->GetRecordingDuration();
	
	if (StateTree != nullptr && EventCollection.IsValid())
	{
		auto MakeRandomColor = [bActive = !bIsStale](const uint32 InSeed)->FLinearColor
		{
			const FRandomStream Stream(InSeed);
			const uint8 Hue = static_cast<uint8>(Stream.FRand() * 255.0f);
			const uint8 SatVal = bActive ? 196 : 128;
			return FLinearColor::MakeFromHSV8(Hue, SatVal, SatVal);
		};
		
		const TConstArrayView<UE::StateTreeDebugger::FFrameSpan> Spans = EventCollection.FrameSpans;
		const TConstArrayView<FStateTreeTraceEventVariantType> Events = EventCollection.Events;
		const uint32 NumStateChanges = EventCollection.ActiveStatesChanges.Num();
		
		for (uint32 StateChangeIndex = 0; StateChangeIndex < NumStateChanges; ++StateChangeIndex)
		{
			const uint32 SpanIndex = EventCollection.ActiveStatesChanges[StateChangeIndex].SpanIndex;
			const uint32 EventIndex = EventCollection.ActiveStatesChanges[StateChangeIndex].EventIndex;
			const FStateTreeTraceActiveStatesEvent& Event = Events[EventIndex].Get<FStateTreeTraceActiveStatesEvent>();
				
			FString StatePath = Event.GetValueString(*StateTree);
			UE::StateTreeDebugger::FFrameSpan Span = EventCollection.FrameSpans[SpanIndex];
			SStateTreeDebuggerEventTimelineView::FTimelineEventData::EventWindow& Window = EventData->Windows.AddDefaulted_GetRef();
			Window.Color = MakeRandomColor(GetTypeHash(StatePath));
			Window.Description = FText::FromString(StatePath);
			Window.TimeStart = Span.GetWorldTimeStart();

			// When there is another state change after the current one in the list we use its start time to close the window.
			if (StateChangeIndex < NumStateChanges-1)
			{
				const uint32 NextSpanIndex = EventCollection.ActiveStatesChanges[StateChangeIndex+1].SpanIndex;
				Window.TimeEnd = EventCollection.FrameSpans[NextSpanIndex].GetWorldTimeStart();
			}
			else
			{
				Window.TimeEnd = Debugger->IsActiveInstance(RecordingDuration, InstanceId) ? RecordingDuration : EventCollection.FrameSpans.Last().GetWorldTimeEnd();
			}
		}

		for (int32 SpanIndex = 0; SpanIndex < Spans.Num(); SpanIndex++)
		{
			const UE::StateTreeDebugger::FFrameSpan& Span = Spans[SpanIndex];

			const int32 StartIndex = Span.EventIdx;
			const int32 MaxIndex = (SpanIndex + 1 < Spans.Num()) ? Spans[SpanIndex+1].EventIdx : Events.Num();
			for (int EventIndex = StartIndex; EventIndex < MaxIndex; ++EventIndex)
			{
				if (Events[EventIndex].IsType<FStateTreeTraceLogEvent>())
				{
					SStateTreeDebuggerEventTimelineView::FTimelineEventData::EventPoint Point;
					Point.Time = Span.GetWorldTimeStart();
					Point.Color = FColorList::Salmon;
					EventData->Points.Add(Point);
				}
			}
		}
	}

	const bool bChanged = (PrevNumPoints != EventData->Points.Num() || PrevNumWindows != EventData->Windows.Num());
	return bChanged;
}

TSharedPtr<SWidget> FStateTreeDebuggerInstanceTrack::GetTimelineViewInternal()
{
	return SNew(SStateTreeDebuggerEventTimelineView)
		.ViewRange_Lambda([this](){ return ViewRange; })
		.EventData_Lambda([this](){ return EventData; });
}


//----------------------------------------------------------------------//
// FStateTreeDebuggerOwnerTrack
//----------------------------------------------------------------------//
FStateTreeDebuggerOwnerTrack::FStateTreeDebuggerOwnerTrack(const FText& InInstanceName)
	: FStateTreeDebuggerBaseTrack(FSlateIcon("StateTreeEditorStyle", "StateTreeEditor.Debugger.OwnerTrack", "StateTreeEditor.Debugger.OwnerTrack"), InInstanceName)
{
}

bool FStateTreeDebuggerOwnerTrack::UpdateInternal()
{
	bool bChanged = false;
	for (const TSharedPtr<FStateTreeDebuggerInstanceTrack>& Track : SubTracks)
	{
		bChanged = Track->Update() || bChanged;
	}

	return bChanged;
}

void FStateTreeDebuggerOwnerTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for (TSharedPtr<FStateTreeDebuggerInstanceTrack>& Track : SubTracks)
	{
		IteratorFunction(Track);
	}
}

void FStateTreeDebuggerOwnerTrack::MarkAsStale()
{
	for (TSharedPtr<FStateTreeDebuggerInstanceTrack>& Track : SubTracks)
	{
		if (FStateTreeDebuggerBaseTrack* InstanceTrack = Track.Get())
		{
			InstanceTrack->MarkAsStale();
		}
	}
}

bool FStateTreeDebuggerOwnerTrack::IsStale() const
{
	// Considered stale only if all sub tracks are stale
	if (SubTracks.IsEmpty())
	{
		return false;
	}

	for (const TSharedPtr<FStateTreeDebuggerInstanceTrack>& Track : SubTracks)
	{
		const FStateTreeDebuggerBaseTrack* InstanceTrack = Track.Get();
		if (InstanceTrack && InstanceTrack->IsStale() == false)
		{
			return false;
		}
	}

	return true;
}

#endif // WITH_STATETREE_DEBUGGER
