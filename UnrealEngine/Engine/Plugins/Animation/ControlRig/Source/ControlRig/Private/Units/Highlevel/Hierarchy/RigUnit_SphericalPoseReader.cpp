// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Highlevel/Hierarchy/RigUnit_SphericalPoseReader.h"
#include "Kismet/KismetMathLibrary.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SphericalPoseReader)

FRigUnit_SphericalPoseReader_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy == nullptr)
	{
		return;
	}

	if (!DriverCache.UpdateCache(DriverItem, Hierarchy))
	{
		return;
	}

	// remap/clamp inputs
	RemapAndConvertInputs(
		InnerRegion,
		OuterRegion,
		ActiveRegionSize,
		ActiveRegionScaleFactors,
		FalloffSize,
		FalloffRegionScaleFactors,
		FlipWidthScaling,
		FlipHeightScaling);

	// get parent global space
	FTransform GlobalDriverParentTransform;
	OptionalParentCache.UpdateCache(OptionalParentItem, Hierarchy);
	if (OptionalParentCache.IsValid())
	{
		GlobalDriverParentTransform = Hierarchy->GetGlobalTransformByIndex(OptionalParentCache, false);
	}else
	{
		// if user does not specify a custom parent space, then simply use the driver's parent
		GlobalDriverParentTransform = Hierarchy->GetParentTransformByIndex(DriverCache, false);
	}

	// check if we need to update the LocalDriverTransformInit
	if (!bCachedInitTransforms || !CachedRotationOffset.Equals(RotationOffset))
	{
		bCachedInitTransforms = true;
		CachedRotationOffset = RotationOffset;

		FTransform GlobalDriverParentTransformInitInverse;
		if (OptionalParentCache.IsValid())
		{
			GlobalDriverParentTransformInitInverse = Hierarchy->GetGlobalTransformByIndex(OptionalParentCache, true).Inverse();
		}else
		{
			// if user does not specify a custom parent space, then simply use the driver's parent
			GlobalDriverParentTransformInitInverse = Hierarchy->GetParentTransformByIndex(DriverCache, true).Inverse();
		}

		// get local rotation space of driver
		const FTransform GlobalDriverTransformInit = Hierarchy->GetGlobalTransformByIndex(DriverCache, true);
		LocalDriverTransformInit = GlobalDriverTransformInit * GlobalDriverParentTransformInitInverse;
		// apply static offset in local space
		FQuat RotationOffsetQuat = FQuat::MakeFromEuler(RotationOffset);
		LocalDriverTransformInit.SetRotation(LocalDriverTransformInit.GetRotation() * RotationOffsetQuat);
	}

	// calculate world transform of sphere
	const FTransform GlobalDriverTransform = Hierarchy->GetGlobalTransformByIndex(DriverCache);
	FTransform WorldOffset = LocalDriverTransformInit * GlobalDriverParentTransform;
	WorldOffset.SetLocation(GlobalDriverTransform.GetLocation());

	// get driver axis in local space of sphere
	const FVector CurrentGlobalDriverAxis = GlobalDriverTransform.GetRotation().RotateVector(DriverAxis);
	DriverNormal = WorldOffset.InverseTransformVectorNoScale(CurrentGlobalDriverAxis).GetSafeNormal();

	//
	// evaluate the interpolation of the output param
	//

	// get angle between the driver direction and the local Z-axis of the sphere
	float DriverDotZAxis = FVector::DotProduct(DriverNormal, FVector::ZAxisVector);
	if (FMath::IsNearlyEqual(DriverDotZAxis, -1.0f, KINDA_SMALL_NUMBER))
	{
		// avoid singularity at negative pole (back of sphere)
		OutputParam = 0.0f;
		Debug.DrawDebug(WorldOffset, ExecuteContext.GetDrawInterface(), InnerRegion, OuterRegion, DriverNormal, Driver2D, OutputParam);
		return;
	}

	// remap angle from 0-PI, to 0-1
	const float AngleFromForward = FMath::Acos(DriverDotZAxis);
	const float Mag = RemapRange(AngleFromForward, 0.0f, PI, 0.0f, 1.0f);
	if (FMath::IsNearlyZero(Mag))
	{
		// avoid singularity at positive pole (guaranteed to be inside inner ellipse)
		// causes NaNs in DistanceToEllipse
		OutputParam = 1.0f;
		Debug.DrawDebug(WorldOffset, ExecuteContext.GetDrawInterface(), InnerRegion, OuterRegion, DriverNormal, Driver2D, OutputParam);
		return;
	}

	// convert to 2d polar coordinates, with magnitude normalized by range 0-PI
	Driver2D = DriverNormal;
	if (Mag > SMALL_NUMBER)
	{
		Driver2D.Z = 0.0f;
		Driver2D.Normalize();
		Driver2D *= Mag;
	}
	Driver2D.Z = -1.0f;
	
	const float PointX = Driver2D.X;
	const float PointY = Driver2D.Y;

	float EllipseWidth;
	float EllipseHeight;

	// query inner ellipse
	InnerRegion.GetEllipseWidthAndHeight(PointX, PointY, EllipseWidth, EllipseHeight);
	FEllipseQuery InnerEllipseResults;
	DistanceToEllipse(PointX, PointY, EllipseWidth, EllipseHeight, InnerEllipseResults);

	// query outer ellipse
	OuterRegion.GetEllipseWidthAndHeight(PointX, PointY, EllipseWidth, EllipseHeight);
	FEllipseQuery OuterEllipseResults;
	DistanceToEllipse(PointX, PointY, EllipseWidth, EllipseHeight, OuterEllipseResults);

	// calc output param
	OutputParam = CalcOutputParam(InnerEllipseResults, OuterEllipseResults);

	// do all debug drawing
	Debug.DrawDebug(WorldOffset, ExecuteContext.GetDrawInterface(), InnerRegion, OuterRegion, DriverNormal, Driver2D, OutputParam);
}

void FRigUnit_SphericalPoseReader::RemapAndConvertInputs(
	FSphericalRegion& InnerRegion,
	FSphericalRegion& OuterRegion,
	const float ActiveRegionSize,
	const FRegionScaleFactors& ActiveRegionScaleFactors,
	const float FalloffSize,
	const FRegionScaleFactors& FalloffRegionScaleFactors,
	const bool FlipWidth,
	const bool FlipHeight)
{
	// remap normalized inputs to angles
	float RegionAngle = ActiveRegionSize * 180.0f;
	RegionAngle = FMath::Min(FMath::Max(0.5f, RegionAngle), 178.0f);

	InnerRegion.RegionAngleRadians = FMath::DegreesToRadians(RegionAngle);
	InnerRegion.ScaleFactors = ActiveRegionScaleFactors;

	// clamp outer falloff angle to always be greater than inner
	float FalloffAngle = FalloffSize * 180.0f;
	FalloffAngle = FMath::Min(FMath::Max(RegionAngle+1.0f, FalloffAngle), 179.0f);
	OuterRegion.RegionAngleRadians = FMath::DegreesToRadians(FalloffAngle);

	// clamp falloff to always be outside inner angle
	const float InvOuterAngleRadians = 1.0f / OuterRegion.RegionAngleRadians;
	//
	const float PosWidthMin = (InnerRegion.RegionAngleRadians * ActiveRegionScaleFactors.PositiveWidth) * InvOuterAngleRadians;
	OuterRegion.ScaleFactors.PositiveWidth = FMath::Lerp(PosWidthMin, 1.0f, FalloffRegionScaleFactors.PositiveWidth);
	//
	const float NegWidthMin = (InnerRegion.RegionAngleRadians * ActiveRegionScaleFactors.NegativeWidth) * InvOuterAngleRadians;
	OuterRegion.ScaleFactors.NegativeWidth = FMath::Lerp(NegWidthMin, 1.0f, FalloffRegionScaleFactors.NegativeWidth);
	//
	const float PosHeightMin = (InnerRegion.RegionAngleRadians * ActiveRegionScaleFactors.PositiveHeight) * InvOuterAngleRadians;
	OuterRegion.ScaleFactors.PositiveHeight = FMath::Lerp(PosHeightMin, 1.0f, FalloffRegionScaleFactors.PositiveHeight);
	//
	const float NegHeightMin = (InnerRegion.RegionAngleRadians * ActiveRegionScaleFactors.NegativeHeight) * InvOuterAngleRadians;
	OuterRegion.ScaleFactors.NegativeHeight = FMath::Lerp(NegHeightMin, 1.0f, FalloffRegionScaleFactors.NegativeHeight);

	// optionally flip the scaling factors to better support mirrored setups
	if (FlipWidth)
	{
		Swap(InnerRegion.ScaleFactors.PositiveWidth, InnerRegion.ScaleFactors.NegativeWidth);
		Swap(OuterRegion.ScaleFactors.PositiveWidth, OuterRegion.ScaleFactors.NegativeWidth);
	}
	if (FlipHeight)
	{
		Swap(InnerRegion.ScaleFactors.PositiveHeight, InnerRegion.ScaleFactors.NegativeHeight);
		Swap(OuterRegion.ScaleFactors.PositiveHeight, OuterRegion.ScaleFactors.NegativeHeight);
	}
}

float FRigUnit_SphericalPoseReader::CalcOutputParam(
	const FEllipseQuery& InnerEllipseResults,
	const FEllipseQuery& OuterEllipseResults)
{
    if (InnerEllipseResults.IsInside)
    {
        return 1.0f; // inside inner ellipse
    }

    if (!OuterEllipseResults.IsInside)
    {
        return 0.0f; // outside outer ellipse
    }

    // we're between outer and inner ellipse, calculate falloff
    const float DistanceToOuter = FMath::Sqrt(OuterEllipseResults.DistSq);
    const float DistanceToInner = FMath::Sqrt(InnerEllipseResults.DistSq);
    const float TotalDistance = DistanceToInner + DistanceToOuter;
    if (TotalDistance < 0.0001f)
    {
        return 0.0f; // don't lerp when outer is VERY close to inner (avoid div by zero)
    }

    return 1.0f - (DistanceToInner / TotalDistance);
}

void FRigUnit_SphericalPoseReader::DistanceToEllipse(
	const float InX,
	const float InY,
	const float SizeX,
	const float SizeY,
	FEllipseQuery& OutEllipseQuery)
{
	// degenerate ellipse
	if (SizeX <= KINDA_SMALL_NUMBER || SizeY <= KINDA_SMALL_NUMBER)
	{
		OutEllipseQuery.ClosestX = 0.0f;
		OutEllipseQuery.ClosestY = 0.0f;
		OutEllipseQuery.DistSq = FMath::Pow(InX, 2.f) + FMath::Pow(InY, 2.f);
		OutEllipseQuery.IsInside = OutEllipseQuery.DistSq <= KINDA_SMALL_NUMBER;
		return;
	}
	
    const float PX = FMath::Abs(InX);
    const float PY = FMath::Abs(InY);

	const float SizeXSq = SizeX * SizeX;
	const float SizeYSq = SizeY * SizeY;

	const float InvSizeX = 1.0f / SizeX;
	const float InvSizeY = 1.0f / SizeY;
	
    float TX = 0.70710678118f;
    float TY = 0.70710678118f;

    const int Iterations = 2; // this could be higher for accuracy, but 2 seems good enough for this use case
    for (int i=0; i<Iterations; ++i)
    {
        const float ScaledX = SizeX * TX;
        const float ScaledY = SizeY * TY;

        const float EX = (SizeXSq - SizeYSq) * (TX * TX * TX) * InvSizeX;
        const float EY = (SizeYSq - SizeXSq) * (TY * TY * TY) * InvSizeY;

        const float RX = ScaledX - EX;
        const float RY = ScaledY - EY;

        const float QX = PX - EX;
        const float QY = PY - EY;

        const float R = FMath::Sqrt(RX * RX + RY * RY);
        const float Q = FMath::Sqrt(QY * QY + QX * QX);

        TX = FMath::Min(1.f, FMath::Max(0.f, (QX * R / Q + EX) * InvSizeX));
        TY = FMath::Min(1.f, FMath::Max(0.f, (QY * R / Q + EY) * InvSizeY));

        const float InvT = 1.0f / FMath::Sqrt(TX * TX + TY * TY);

        TX *= InvT;
        TY *= InvT;
    }

    OutEllipseQuery.ClosestX = SizeX * (InX < 0 ? -TX : TX);
    OutEllipseQuery.ClosestY = SizeY * (InY < 0 ? -TY : TY);
    const float ToClosestX = OutEllipseQuery.ClosestX - InX;
    const float ToClosestY = OutEllipseQuery.ClosestY - InY;
    OutEllipseQuery.DistSq = FMath::Pow(ToClosestX, 2.f) + FMath::Pow(ToClosestY, 2.f);
    const float CenterToClosestDistSq = FMath::Pow(OutEllipseQuery.ClosestX, 2.f) + FMath::Pow(OutEllipseQuery.ClosestY, 2.f);
    const float CenterToInputDistSq = FMath::Pow(InX, 2.f) + FMath::Pow(InY, 2.f);
    OutEllipseQuery.IsInside = CenterToClosestDistSq > CenterToInputDistSq;
}

float FRigUnit_SphericalPoseReader::RemapRange(
	const float T,
	const float AStart,
	const float AEnd,
	const float BStart,
	const float BEnd)
{
	check(FMath::Abs(AEnd - AStart) > 0.0f);
	return BStart + (T - AStart) * (BEnd - BStart) / (AEnd - AStart);
}

