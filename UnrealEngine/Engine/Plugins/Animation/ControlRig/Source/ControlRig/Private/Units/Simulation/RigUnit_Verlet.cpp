// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_Verlet.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Verlet)

FRigUnit_VerletIntegrateVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		bInitialized = false;
		return;
	}

	if (!bInitialized)
	{
		Point.Mass = 1.f;
		Position = Point.Position = Target;
		Velocity = Acceleration = Point.LinearVelocity = FVector::ZeroVector;
		bInitialized = true;
		return;
	}

	Point.LinearDamping = Damp;
	if (Context.DeltaTime > SMALL_NUMBER)
	{
		float U = FMath::Clamp<float>(Blend * Context.DeltaTime, 0.f, 1.f);
		const FVector CombinedForce = (Target - Point.Position) * FMath::Max(Strength, 0.0001f) + Force * Context.DeltaTime * 60.f;
		const FVector PreviousVelocity = Point.LinearVelocity;
		Point = Point.IntegrateVerlet(CombinedForce, Blend, Context.DeltaTime);
		Acceleration = (Point.LinearVelocity - PreviousVelocity) / Context.DeltaTime;
		Position = Point.Position;
		Velocity = Point.LinearVelocity;
	}
}

