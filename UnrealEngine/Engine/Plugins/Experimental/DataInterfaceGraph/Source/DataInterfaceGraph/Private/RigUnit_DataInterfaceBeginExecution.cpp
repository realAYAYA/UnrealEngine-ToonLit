// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_DataInterfaceBeginExecution.h"
#include "Units/RigUnitContext.h"

FName FRigUnit_DataInterfaceBeginExecution::EventName = TEXT("Update");

FRigUnit_DataInterfaceBeginExecution_Execute()
{
	const FDataInterfaceExecuteContext& DataInterfaceExecuteContext = static_cast<const FDataInterfaceExecuteContext&>(RigVMExecuteContext);
	ExecuteContext.CopyFrom(DataInterfaceExecuteContext);
}
