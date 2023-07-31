// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_Random.h"
#include "Units/RigUnitContext.h"
#include "GenericPlatform/GenericPlatformMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Random)

float FRigUnit_Random_Helper(int32& Seed)
{
	Seed = (Seed * 196314165) + 907633515;
	union { float f; int32 i; } Result;
	union { float f; int32 i; } Temp;
	const float SRandTemp = 1.0f;
	Temp.f = SRandTemp;
	Result.i = (Temp.i & 0xff800000) | (Seed & 0x007fffff);
	return FPlatformMath::Fractional(Result.f);
}

FRigUnit_RandomFloat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		LastSeed = BaseSeed = Seed;
		TimeLeft = 0.f;
		return;
	}

	if(Seed != BaseSeed)
	{
		LastSeed = BaseSeed = Seed;
	}

	TimeLeft = TimeLeft - Context.DeltaTime;
	if (TimeLeft > 0.f)
	{
		Result = LastResult;
		return;
	}

	Result = FRigUnit_Random_Helper(LastSeed);
	Result = FMath::Lerp<float>(Minimum, Maximum, Result);
	TimeLeft = Duration < -SMALL_NUMBER ? FLT_MAX : Duration;
	LastResult = Result;
}

FRigUnit_RandomVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		LastSeed = BaseSeed = Seed;
		TimeLeft = 0.f;
		return;
	}

	if(Seed != BaseSeed)
	{
		LastSeed = BaseSeed = Seed;
	}

	TimeLeft = TimeLeft - Context.DeltaTime;
	if (TimeLeft > 0.f)
	{
		Result = LastResult;
		return;
	}

	Result.X = FMath::Lerp<float>(Minimum, Maximum, FRigUnit_Random_Helper(LastSeed));
	Result.Y = FMath::Lerp<float>(Minimum, Maximum, FRigUnit_Random_Helper(LastSeed));
	Result.Z = FMath::Lerp<float>(Minimum, Maximum, FRigUnit_Random_Helper(LastSeed));
	TimeLeft = Duration < -SMALL_NUMBER ? FLT_MAX : Duration;
	LastResult = Result;
}
