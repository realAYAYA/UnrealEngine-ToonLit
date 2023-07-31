// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_BeginExecution.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_BeginExecution)

FName FRigUnit_BeginExecution::EventName = TEXT("Forwards Solve");

FRigUnit_BeginExecution_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	ExecuteContext.CopyFrom(RigVMExecuteContext);
	ExecuteContext.Hierarchy = Context.Hierarchy;
	ExecuteContext.EventName = FRigUnit_BeginExecution::EventName;
}

