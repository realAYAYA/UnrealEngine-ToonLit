// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_PrepareForExecution)

FName FRigUnit_PrepareForExecution::EventName = TEXT("Construction");

FRigUnit_PrepareForExecution_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.SetEventName(FRigUnit_PrepareForExecution::EventName);
}

