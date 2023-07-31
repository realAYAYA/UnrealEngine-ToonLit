// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Animation/RigUnit_TimeConversion.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_TimeConversion)

FRigUnit_FramesToSeconds_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    if(Context.FramesPerSecond > SMALL_NUMBER)
    {
		Seconds = Frames / Context.FramesPerSecond;
	}
	else
	{
		Seconds = 0.f;
	}
}

FRigUnit_SecondsToFrames_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    if(Context.FramesPerSecond > SMALL_NUMBER)
    {
		Frames = Seconds * Context.FramesPerSecond;
	}
	else
	{
		Frames = 0.f;
	}
}

