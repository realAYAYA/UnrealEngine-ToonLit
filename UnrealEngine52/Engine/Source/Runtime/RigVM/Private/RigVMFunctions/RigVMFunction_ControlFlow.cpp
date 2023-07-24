// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMFunction_ControlFlow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_ControlFlow)

FRigVMFunction_ControlFlowBranch_Execute()
{
	if(BlockToRun.IsNone())
	{
		if(Condition)
		{
			static const FName TrueName = GET_MEMBER_NAME_CHECKED(FRigVMFunction_ControlFlowBranch, True); 
			BlockToRun = TrueName;
		}
		else
		{
			static const FName FalseName = GET_MEMBER_NAME_CHECKED(FRigVMFunction_ControlFlowBranch, False); 
			BlockToRun = FalseName;
		}
	}
	else
	{
		BlockToRun = FRigVMStruct::ControlFlowCompletedName;
	}
}
