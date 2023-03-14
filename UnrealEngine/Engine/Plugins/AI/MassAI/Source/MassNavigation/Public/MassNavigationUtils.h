// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"

namespace UE::MassNavigation
{
	// Calculates yaw angle from direction vector.
	inline float GetYawFromQuat(const FQuat Rotation)
	{
		const float YawY = 2.f * (Rotation.W * Rotation.Z + Rotation.X * Rotation.Y);
		const float YawX = (1.f - 2.f * (FMath::Square(Rotation.Y) + FMath::Square(Rotation.Z)));
		return FMath::Atan2(YawY, YawX);
	}

	inline float GetYawFromDirection(const FVector Direction)
	{
		return FMath::Atan2(Direction.Y, Direction.X);
	}

	// Wraps and angle to range -PI..PI. Angle in radians.
	inline float WrapAngle(const float Angle)
	{
		float WrappedAngle = FMath::Fmod(Angle, PI*2.0f);
		WrappedAngle = (WrappedAngle > PI) ? WrappedAngle - PI*2.0f : WrappedAngle;
		WrappedAngle = (WrappedAngle < -PI) ? WrappedAngle + PI*2.0f : WrappedAngle;
		return WrappedAngle;
	}

	// Linearly interpolates between two angles (in Radians).
	inline float LerpAngle(const float AngleA, const float AngleB, const float T)
	{
		const float DeltaAngle = WrapAngle(AngleB - AngleA);
		return AngleA + DeltaAngle * T;
	}

	// Exponential smooth from current angle to target angle. Angles in radians.
	inline float ExponentialSmoothingAngle(const float Angle, const float TargetAngle, const float DeltaTime, const float SmoothingTime)
	{
		// Note: based on FMath::ExponentialSmoothingApprox().
		if (SmoothingTime < KINDA_SMALL_NUMBER)
		{
			return TargetAngle;
		}
		const float A = DeltaTime / SmoothingTime;
		const float Exp = FMath::InvExpApprox(A);
		return TargetAngle + WrapAngle(Angle - TargetAngle) * Exp;
	}

	// Clamps vectors magnitude to Mag.
	inline FVector ClampVector(const FVector Vec, const float Mag)
	{
		const float Len = Vec.SizeSquared();
		if (Len > FMath::Square(Mag)) {
			return Vec * Mag / FMath::Sqrt(Len);
		}
		return Vec;
	}

	// Projects a point to segment and returns the time interpolation value.
	inline float ProjectPtSeg(const FVector2D Point, const FVector2D Start, const FVector2D End)
	{
		const FVector2D Seg = End - Start;
		const FVector2D Dir = Point - Start;
		const float d = Seg.SizeSquared();
		const float t = FVector2D::DotProduct(Seg, Dir);
		if (t < 0.f) return 0;
		if (t > d) return 1;
		return d > 0.f ? (t / d) : 0.f;
	}

	// Returns the SmoothStep curve for X in range [0..1]. 
	inline float Smooth(const float X)
	{
		return X * X * (3 - 2 * X);
	}

	// Returns left direction from forward and up directions.
	inline FVector GetLeftDirection(const FVector Forward, const FVector Up)
	{
		return FVector::CrossProduct(Forward, Up);
	}

	// Computes miter normal in XY plane from two neighbour edge normals.
	inline FVector ComputeMiterNormal(const FVector NormalA, const FVector NormalB)
	{
		FVector Mid = 0.5f * (NormalA + NormalB);
		const float MidSquared = FVector::DotProduct(Mid, Mid);
		if (MidSquared > KINDA_SMALL_NUMBER)
		{
			const float Scale = FMath::Min(1.f / MidSquared, 20.f);
			Mid *= Scale;
		}
		return Mid;
	}
	
} // UE::MassMovement