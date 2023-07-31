// Copyright Epic Games, Inc. All Rights Reserved.

#include "Math/Simulation/CRSimPoint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CRSimPoint)

FCRSimPoint FCRSimPoint::IntegrateVerlet(const FVector& InForce, float InBlend, float InDeltaTime) const
{
	FCRSimPoint Point = *this;
	if(Point.Mass > SMALL_NUMBER)
	{
		Point.LinearVelocity = FMath::Lerp<FVector>(Point.LinearVelocity, InForce / Point.Mass, FMath::Clamp<float>(InBlend * InDeltaTime, 0.f, 1.f)) * FMath::Clamp<float>(1.f - LinearDamping, 0.f, 1.f);
		Point.Position = Point.Position + Point.LinearVelocity * InDeltaTime;
	}
	return Point;
}

FCRSimPoint FCRSimPoint::IntegrateSemiExplicitEuler(const FVector& InForce, float InDeltaTime) const
{
	FCRSimPoint Point = *this;
	if(Point.Mass > SMALL_NUMBER)
	{
		Point.LinearVelocity += InForce * InDeltaTime / Point.Mass;
		Point.LinearVelocity -= LinearVelocity * LinearDamping;
		Point.Position += Point.LinearVelocity * InDeltaTime;
	}
	return Point;
}

