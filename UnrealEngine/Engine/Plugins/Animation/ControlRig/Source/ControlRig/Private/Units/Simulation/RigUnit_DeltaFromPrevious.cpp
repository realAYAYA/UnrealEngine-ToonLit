// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_DeltaFromPrevious.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DeltaFromPrevious)

FRigUnit_DeltaFromPreviousFloat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Delta = 0.f;
		PreviousValue = Cache = Value;
		return;
	}

	PreviousValue = Cache;
	Delta = Cache - Value;

	if (FMath::Abs(Context.DeltaTime) > SMALL_NUMBER)
	{
		Cache = Value;
	}
}

FRigUnit_DeltaFromPreviousVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Delta = FVector::ZeroVector;
		PreviousValue = Cache = Value;
		return;
	}

	PreviousValue = Cache;
	Delta = Cache - Value;

	if (FMath::Abs(Context.DeltaTime) > SMALL_NUMBER)
	{
		Cache = Value;
	}
}

FRigUnit_DeltaFromPreviousQuat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Delta = FQuat::Identity;
		PreviousValue = Cache = Value;
		return;
	}

	PreviousValue = Cache;
	Delta = Cache.Inverse() * Value;

	if (FMath::Abs(Context.DeltaTime) > SMALL_NUMBER)
	{
		Cache = Value;
	}
}

FRigUnit_DeltaFromPreviousTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Delta = FTransform::Identity;
		PreviousValue = Cache = Value;
		return;
	}

	PreviousValue = Cache;
	Delta = Value.GetRelativeTransform(Cache);

	if (FMath::Abs(Context.DeltaTime) > SMALL_NUMBER)
	{
		Cache = Value;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Units/RigUnitTest.h"

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_DeltaFromPreviousFloat)
{
	Context.DeltaTime = 0.1f;
	Unit.Value = 1.f;
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Delta, 0.f), TEXT("unexpected average result"));
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.PreviousValue, 1.f), TEXT("unexpected average result"));
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Delta, 0.f), TEXT("unexpected average result"));
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.PreviousValue, 1.f), TEXT("unexpected average result"));
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Delta, 0.f), TEXT("unexpected average result"));
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.PreviousValue, 1.f), TEXT("unexpected average result"));
	Unit.Value = 2.f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Delta, -1.f), TEXT("unexpected average result"));
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.PreviousValue, 1.f), TEXT("unexpected average result"));
	Unit.Value = 5.f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Delta, -3.f), TEXT("unexpected average result"));
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.PreviousValue, 2.f), TEXT("unexpected average result"));
	return true;
}

#endif
