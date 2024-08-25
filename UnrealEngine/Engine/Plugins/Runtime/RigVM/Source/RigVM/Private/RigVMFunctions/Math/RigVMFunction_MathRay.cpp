// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Math/RigVMFunction_MathRay.h"
#include "RigVMFunctions/Math/RigVMFunction_MathTransform.h"
#include "RigVMFunctions/RigVMDispatch_Core.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_MathRay)

FRigVMFunction_MathRayIntersectRay_Execute()
{
	const FVector DirectionA = A.Direction * 10000.0;
	const FVector DirectionB = B.Direction * 10000.0;
	
	FVector ClosestA, ClosestB;
	FMath::SegmentDistToSegmentSafe(
		A.Origin - DirectionA,
		A.Origin + DirectionA,
		B.Origin - DirectionB,
		B.Origin + DirectionB,
		ClosestA, ClosestB);

	Result = (ClosestA + ClosestB) * 0.5;
	Distance = 0;
	if(!ClosestA.Equals(ClosestB))
	{
		Distance = (ClosestA - ClosestB).Size();
	}
	RatioA = A.GetParameter(ClosestA);	
	RatioB = B.GetParameter(ClosestB);	
}

FRigVMFunction_MathRayIntersectPlane_Execute()
{
	const FVector NormalizedPlaneNormal = PlaneNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (FMath::IsNearlyZero(FVector::DotProduct(NormalizedPlaneNormal, Ray.Direction)))
	{
		Result = FVector::ZeroVector;
		Distance = Ratio = 0.f;
		return;
	}
	
	Ratio = FMath::RayPlaneIntersectionParam(Ray.Origin, Ray.Direction, FPlane(PlanePoint, NormalizedPlaneNormal));
	Result = Ray.PointAt(Ratio);
	Distance = 0;
	if(!Ray.Origin.Equals(Result))
	{
		Distance = (Ray.Origin - Result).Size();
	}
}

FRigVMFunction_MathRayGetAt_Execute()
{
	Result = Ray.PointAt(Ratio);
}

FRigVMFunction_MathRayTransform_Execute()
{
	Result.Origin = Transform.TransformPosition(Ray.Origin);
	Result.Direction = Transform.TransformVector(Ray.Direction);
}
