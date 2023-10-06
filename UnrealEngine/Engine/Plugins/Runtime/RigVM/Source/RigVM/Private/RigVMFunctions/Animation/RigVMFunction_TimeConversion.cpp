// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Animation/RigVMFunction_TimeConversion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_TimeConversion)

FRigVMFunction_FramesToSeconds_Execute()
{
    if(ExecuteContext.GetFramesPerSecond() > SMALL_NUMBER)
    {
		Seconds = Frames / ExecuteContext.GetFramesPerSecond();
	}
	else
	{
		Seconds = 0.f;
	}
}

FRigVMFunction_SecondsToFrames_Execute()
{
    if(ExecuteContext.GetFramesPerSecond() > SMALL_NUMBER)
    {
		Frames = Seconds * ExecuteContext.GetFramesPerSecond();
	}
	else
	{
		Frames = 0.f;
	}
}

