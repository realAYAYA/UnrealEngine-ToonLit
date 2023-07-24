// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Execution/RigVMFunction_ForLoop.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_ForLoop)

FRigVMFunction_ForLoopCount_Execute()
{
	if(BlockToRun.IsNone())
	{
		Index = 0;
		BlockToRun = ExecuteContextName;
	}
	else if(BlockToRun == ExecuteContextName)
	{
		Index++;
	}

	if(Index == Count)
	{
		BlockToRun = ControlFlowCompletedName;
	}

	Ratio = GetRatioFromIndex(Index, Count);
}

