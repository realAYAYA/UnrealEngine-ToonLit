// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_Timeline.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Timeline)

FRigUnit_Timeline_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Time = AccumulatedValue = 0.f;
		return;
	}

	Time = AccumulatedValue = AccumulatedValue + Context.DeltaTime * Speed;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Units/RigUnitTest.h"

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_Timeline)
{
	Context.DeltaTime = 1.f;
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Time, 1.f), TEXT("unexpected time"));
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Time, 1.f), TEXT("unexpected time"));
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Time, 2.f), TEXT("unexpected time"));
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Time, 3.f), TEXT("unexpected time"));
	Unit.Speed = 0.5f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Time, 3.5f), TEXT("unexpected time"));
	return true;
}

#endif

FRigUnit_TimeLoop_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Absolute = Relative = FlipFlop = AccumulatedAbsolute = AccumulatedRelative = 0.f;
		NumIterations = 0;
		Even = false;
		return;
	}

	const float DurationClamped = FMath::Max(Duration, 0.0001f);
	const float Increment = Context.DeltaTime * Speed;
	Absolute = AccumulatedAbsolute = AccumulatedAbsolute + Increment;

	AccumulatedRelative = AccumulatedRelative + Increment;
	while(AccumulatedRelative > DurationClamped)
	{
		AccumulatedRelative -= DurationClamped;
		NumIterations++;
	}

	Relative = AccumulatedRelative;

	Even = (NumIterations & 1) == 0;
	if(Even) // check is this is even or odd
	{
		FlipFlop = Relative;
	}
	else
	{
		FlipFlop = Duration - Relative;
	}

	if(Normalize)
	{
		Relative = Relative / DurationClamped;
		FlipFlop = FlipFlop / DurationClamped;
	}
}
