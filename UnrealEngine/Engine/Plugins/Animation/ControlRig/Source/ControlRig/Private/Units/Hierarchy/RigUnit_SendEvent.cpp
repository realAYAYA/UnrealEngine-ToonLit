// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_SendEvent.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SendEvent)

FRigUnit_SendEvent_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    if(!bEnable)
    {
    	return;
    }

	if (bOnlyDuringInteraction && !Context.IsInteracting())
	{
		return;
	}

	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				break;
			}
			case EControlRigState::Update:
			{
				FRigEventContext EventContext;
				EventContext.Key = Item;
				EventContext.Event = Event;
				EventContext.SourceEventName = ExecuteContext.GetEventName();
				EventContext.LocalTime = Context.AbsoluteTime + OffsetInSeconds;
				Hierarchy->SendEvent(EventContext, false /* async */); //needs to be false for sequencer keying to work
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

