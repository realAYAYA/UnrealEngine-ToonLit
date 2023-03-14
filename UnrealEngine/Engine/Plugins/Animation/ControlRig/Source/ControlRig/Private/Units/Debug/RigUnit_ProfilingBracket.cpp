// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_ProfilingBracket.h"
#include "Units/RigUnitContext.h"
#include "KismetAnimationLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_ProfilingBracket)

FRigUnit_StartProfilingTimer_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Update)
	{
		UKismetAnimationLibrary::K2_StartProfilingTimer();
	}
}

FRigUnit_EndProfilingTimer_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		AccumulatedTime = 0.f;
		MeasurementsLeft = FMath::Max(NumberOfMeasurements, 1);
		return;
	}

	if (Context.State == EControlRigState::Update)
	{
		float Delta = UKismetAnimationLibrary::K2_EndProfilingTimer(false);
		if(MeasurementsLeft > 0)
		{
			AccumulatedTime += Delta / float(FMath::Max(NumberOfMeasurements, 1));
			MeasurementsLeft--;

			if (MeasurementsLeft == 0)
			{
				if (Prefix.IsEmpty())
				{
					UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(TEXT("%0.3f ms (%d runs)."), AccumulatedTime, FMath::Max(NumberOfMeasurements, 1));
				}
				else
				{
					UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(TEXT("[%s] %0.3f ms (%d runs)."), *Prefix, AccumulatedTime, FMath::Max(NumberOfMeasurements, 1));
				}
			}
		}
	}
}
