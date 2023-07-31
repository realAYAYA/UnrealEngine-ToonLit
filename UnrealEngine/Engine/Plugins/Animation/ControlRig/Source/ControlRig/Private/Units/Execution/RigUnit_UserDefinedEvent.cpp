// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_UserDefinedEvent.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_UserDefinedEvent)

FString FRigUnit_UserDefinedEvent::GetUnitLabel() const
{
	if(EventName.IsNone())
	{
		static const FString InvalidEvent = TEXT("Invalid Event");
		return InvalidEvent;
	}
	return *EventName.ToString();
}

FRigUnit_UserDefinedEvent_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.CopyFrom(RigVMExecuteContext);
	ExecuteContext.Hierarchy = Context.Hierarchy;

	if(EventName.IsNone())
	{
		ExecuteContext.EventName = EventName;
	}
	if(EventName.IsNone())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Unable to run event - EventName is None."));
	}
}

