// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "Debugger/StateTreeDebuggerTypes.h"

namespace UE::StateTreeDebugger
{

//----------------------------------------------------------------//
// FInstanceDescriptor
//----------------------------------------------------------------//
FInstanceDescriptor::FInstanceDescriptor(const UStateTree* InStateTree, const FStateTreeInstanceDebugId InId, const FString& InName, const TRange<double> InLifetime)
	: Lifetime(InLifetime)
	, StateTree(InStateTree)
	, Name(InName)
	, Id(InId)
{
}

bool FInstanceDescriptor::IsValid() const
{
	return StateTree.IsValid() && Name.Len() && Id.IsValid();
}


//----------------------------------------------------------------//
// FInstanceEventCollection
//----------------------------------------------------------------//
const FInstanceEventCollection FInstanceEventCollection::Invalid;


//----------------------------------------------------------------//
// FScrubState
//----------------------------------------------------------------//
bool FScrubState::SetScrubTime(const double NewScrubTime)
{
	if (NewScrubTime == ScrubTime)
	{
		return false;
	}

	ScrubTimeBoundState = EScrubTimeBoundState::Unset;
	TraceFrameIndex = INDEX_NONE;
	FrameSpanIndex = INDEX_NONE;
	ActiveStatesIndex = INDEX_NONE;

	if (EventCollectionIndex != INDEX_NONE)
	{
		const TArray<FFrameSpan>& Spans = EventCollections[EventCollectionIndex].FrameSpans;
		if (Spans.Num() > 0)
		{
			const double SpansLowerBound = Spans[0].GetWorldTimeStart();
			const double SpansUpperBound =  Spans.Last().GetWorldTimeEnd();
			
			if (NewScrubTime < SpansLowerBound)
			{
				ScrubTimeBoundState = EScrubTimeBoundState::BeforeLowerBound;
			}
			else if (NewScrubTime > SpansUpperBound)
			{
				ScrubTimeBoundState = EScrubTimeBoundState::AfterHigherBound;
				UpdateActiveStatesIndex(Spans.Num() - 1);
			}
			else
			{
				const uint32 NextFrameSpanIndex = Spans.IndexOfByPredicate([NewScrubTime](const FFrameSpan& Span)
					{
						return Span.GetWorldTimeStart() > NewScrubTime;
					});

				
				ensure(NextFrameSpanIndex == INDEX_NONE || NextFrameSpanIndex > 0);
				SetFrameSpanIndex(NextFrameSpanIndex != INDEX_NONE ? NextFrameSpanIndex-1 : Spans.Num() - 1);
			}
		}
	}

	// This will set back to the exact value provided since SetFrameSpanIndex will snap it to the start time of the matching frame.
	// It will be consistent with the case where EventCollectionIndex is not set.
	ScrubTime = NewScrubTime;

	return true;
}

void FScrubState::SetEventCollectionIndex(const int32 InEventCollectionIndex)
{
	EventCollectionIndex = InEventCollectionIndex;

	// Force refresh of internal indices with current time applied on the new event collection. 
	const double PrevScrubTime = ScrubTime;
	ScrubTime = 0;
	SetScrubTime(PrevScrubTime);
}

void FScrubState::SetFrameSpanIndex(const int32 NewFrameSpanIndex)
{
	FrameSpanIndex = NewFrameSpanIndex;
	checkf(EventCollections.IsValidIndex(EventCollectionIndex), TEXT("Internal method expecting validity checks before getting called."));
	const FInstanceEventCollection& EventCollection = EventCollections[EventCollectionIndex];

	checkf(EventCollections[EventCollectionIndex].FrameSpans.IsValidIndex(FrameSpanIndex), TEXT("Internal method expecting validity checks before getting called."));
	ScrubTime = EventCollection.FrameSpans[FrameSpanIndex].GetWorldTimeStart();
	TraceFrameIndex = EventCollection.FrameSpans[FrameSpanIndex].Frame.Index;
	ScrubTimeBoundState = EScrubTimeBoundState::InBounds;
	UpdateActiveStatesIndex(NewFrameSpanIndex);
}

void FScrubState::SetActiveStatesIndex(const int32 NewActiveStatesIndex)
{
	ActiveStatesIndex = NewActiveStatesIndex;

	checkf(EventCollections.IsValidIndex(EventCollectionIndex), TEXT("Internal method expecting validity checks before getting called."));
	const FInstanceEventCollection& EventCollection = EventCollections[EventCollectionIndex];

	checkf(EventCollection.ActiveStatesChanges.IsValidIndex(ActiveStatesIndex), TEXT("Internal method expecting validity checks before getting called."));
	FrameSpanIndex = EventCollection.ActiveStatesChanges[ActiveStatesIndex].SpanIndex;
	ScrubTime = EventCollection.FrameSpans[FrameSpanIndex].GetWorldTimeStart();
	TraceFrameIndex = EventCollection.FrameSpans[FrameSpanIndex].Frame.Index;
	ScrubTimeBoundState = EScrubTimeBoundState::InBounds;
}

bool FScrubState::HasPreviousFrame() const
{
	if (EventCollectionIndex != INDEX_NONE)
	{
		return IsInBounds() ? EventCollections[EventCollectionIndex].FrameSpans.IsValidIndex(FrameSpanIndex - 1) : ScrubTimeBoundState == EScrubTimeBoundState::AfterHigherBound;
	}
	return false;
}

double FScrubState::GotoPreviousFrame()
{
	SetFrameSpanIndex(IsInBounds() ? (FrameSpanIndex - 1) : EventCollections[EventCollectionIndex].FrameSpans.Num()-1);
	return ScrubTime;
}

bool FScrubState::HasNextFrame() const
{
	if (EventCollectionIndex != INDEX_NONE)
	{
		return IsInBounds()	? EventCollections[EventCollectionIndex].FrameSpans.IsValidIndex(FrameSpanIndex + 1) : ScrubTimeBoundState == EScrubTimeBoundState::BeforeLowerBound;
	}
	return false;
}

double FScrubState::GotoNextFrame()
{
	SetFrameSpanIndex(IsInBounds() ? (FrameSpanIndex + 1) : 0);
	return ScrubTime;
}

bool FScrubState::HasPreviousActiveStates() const
{
	if (EventCollectionIndex == INDEX_NONE || ActiveStatesIndex == INDEX_NONE)
	{
		return false;
	}

	const TArray<FInstanceEventCollection::FActiveStatesChangePair>& ActiveStatesChanges = EventCollections[EventCollectionIndex].ActiveStatesChanges;
	if (ScrubTimeBoundState == EScrubTimeBoundState::AfterHigherBound && ActiveStatesChanges.Num() > 0)
	{
		return true;
	}

	if (ActiveStatesChanges.IsValidIndex(ActiveStatesIndex) && ActiveStatesChanges[ActiveStatesIndex].SpanIndex < FrameSpanIndex)
	{
		return true;
	}

	return ActiveStatesChanges.IsValidIndex(ActiveStatesIndex - 1);
}

double FScrubState::GotoPreviousActiveStates()
{
	const TArray<FInstanceEventCollection::FActiveStatesChangePair>& ActiveStatesChanges = EventCollections[EventCollectionIndex].ActiveStatesChanges;
	if (ScrubTimeBoundState == EScrubTimeBoundState::AfterHigherBound)
	{
		SetActiveStatesIndex(ActiveStatesChanges.Num()-1);
	}
	else if (ActiveStatesChanges.IsValidIndex(ActiveStatesIndex) && ActiveStatesChanges[ActiveStatesIndex].SpanIndex < FrameSpanIndex)
	{
		SetActiveStatesIndex(ActiveStatesIndex);
	}
	else
	{
		SetActiveStatesIndex(ActiveStatesIndex - 1);
	}

	return ScrubTime;
}

bool FScrubState::HasNextActiveStates() const
{
	if (EventCollectionIndex == INDEX_NONE)
	{
		return false;
	}

	const TArray<FInstanceEventCollection::FActiveStatesChangePair>& ActiveStatesChanges = EventCollections[EventCollectionIndex].ActiveStatesChanges;
	if (ScrubTimeBoundState == EScrubTimeBoundState::BeforeLowerBound && ActiveStatesChanges.Num() > 0)
	{
		return true;
	}

	return ActiveStatesIndex != INDEX_NONE && EventCollections[EventCollectionIndex].ActiveStatesChanges.IsValidIndex(ActiveStatesIndex + 1);
}

double FScrubState::GotoNextActiveStates()
{
	if (ScrubTimeBoundState == EScrubTimeBoundState::BeforeLowerBound)
	{
		SetActiveStatesIndex(0);
	}
	else
	{
		SetActiveStatesIndex(ActiveStatesIndex + 1);
	}
	return ScrubTime;
}

const FInstanceEventCollection& FScrubState::GetEventCollection() const
{
	return EventCollectionIndex != INDEX_NONE ? EventCollections[EventCollectionIndex] : FInstanceEventCollection::Invalid;
}

void FScrubState::UpdateActiveStatesIndex(const int32 SpanIndex)
{
	check(EventCollectionIndex != INDEX_NONE);
	const FInstanceEventCollection& EventCollection = EventCollections[EventCollectionIndex];

	// Need to find the index of a frame span that contains an active states changed event; either the current one has it otherwise look backward to find the last one
	ActiveStatesIndex = EventCollection.ActiveStatesChanges.FindLastByPredicate(
		[SpanIndex](const FInstanceEventCollection::FActiveStatesChangePair& SpanAndEventIndices)
		{
			return SpanAndEventIndices.SpanIndex <= SpanIndex;
		});
}
} // UE::StateTreeDebugger

FStateTreeDebuggerBreakpoint::FStateTreeDebuggerBreakpoint()
	: BreakpointType(EStateTreeBreakpointType::Unset)
	, EventType(EStateTreeTraceEventType::Unset)
{
}

FStateTreeDebuggerBreakpoint::FStateTreeDebuggerBreakpoint(const FStateTreeStateHandle StateHandle, const EStateTreeBreakpointType BreakpointType)
	: ElementIdentifier(TInPlaceType<FStateTreeStateHandle>(), StateHandle)
	, BreakpointType(BreakpointType)
{
	EventType = GetMatchingEventType(BreakpointType);
}

FStateTreeDebuggerBreakpoint::FStateTreeDebuggerBreakpoint(const FStateTreeTaskIndex Index, const EStateTreeBreakpointType BreakpointType)
	: ElementIdentifier(TInPlaceType<FStateTreeTaskIndex>(), Index)
	, BreakpointType(BreakpointType)
{
	EventType = GetMatchingEventType(BreakpointType);
}

FStateTreeDebuggerBreakpoint::FStateTreeDebuggerBreakpoint(const FStateTreeTransitionIndex Index, const EStateTreeBreakpointType BreakpointType)
	: ElementIdentifier(TInPlaceType<FStateTreeTransitionIndex>(), Index)
	, BreakpointType(BreakpointType)
{
	EventType = GetMatchingEventType(BreakpointType);
}

bool FStateTreeDebuggerBreakpoint::IsMatchingEvent(const FStateTreeTraceEventVariantType& Event) const
{
	EStateTreeTraceEventType ReceivedEventType = EStateTreeTraceEventType::Unset;
	Visit([&ReceivedEventType](auto& TypedEvent) { ReceivedEventType = TypedEvent.EventType; }, Event);

	bool bIsMatching = false;
	if (EventType == ReceivedEventType)
	{
		if (const FStateTreeStateHandle* StateHandle = ElementIdentifier.TryGet<FStateTreeStateHandle>())
		{
			const FStateTreeTraceStateEvent* StateEvent = Event.TryGet<FStateTreeTraceStateEvent>();
			bIsMatching = StateEvent != nullptr && StateEvent->GetStateHandle() == *StateHandle;
		}
		else if (const FStateTreeTaskIndex* TaskIndex = ElementIdentifier.TryGet<FStateTreeTaskIndex>())
		{
			const FStateTreeTraceTaskEvent* TaskEvent = Event.TryGet<FStateTreeTraceTaskEvent>();
			bIsMatching = TaskEvent != nullptr && TaskEvent->Index == TaskIndex->Index;
		}
		else if (const FStateTreeTransitionIndex* TransitionIndex = ElementIdentifier.TryGet<FStateTreeTransitionIndex>())
		{
			const FStateTreeTraceTransitionEvent* TransitionEvent = Event.TryGet<FStateTreeTraceTransitionEvent>();
			bIsMatching = TransitionEvent != nullptr && TransitionEvent->TransitionSource.TransitionIndex == TransitionIndex->Index;
		}
	}
	return bIsMatching;
}

EStateTreeTraceEventType FStateTreeDebuggerBreakpoint::GetMatchingEventType(const EStateTreeBreakpointType BreakpointType)
{
	switch (BreakpointType)
	{
	case EStateTreeBreakpointType::OnEnter:
		return EStateTreeTraceEventType::OnEntered;
	case EStateTreeBreakpointType::OnExit:
		return EStateTreeTraceEventType::OnExited;
	case EStateTreeBreakpointType::OnTransition:
		return EStateTreeTraceEventType::OnTransition;
	default:
		return EStateTreeTraceEventType::Unset;
	}
}

#endif // WITH_STATETREE_DEBUGGER

