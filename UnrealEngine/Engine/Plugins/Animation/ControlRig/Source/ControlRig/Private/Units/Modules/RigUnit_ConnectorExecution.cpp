// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Modules/RigUnit_ConnectorExecution.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_ConnectorExecution)

FName FRigUnit_ConnectorExecution::EventName = TEXT("Connector");

FRigUnit_ConnectorExecution_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.SetEventName(FRigUnit_ConnectorExecution::EventName);
}

