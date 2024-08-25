// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSplineStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSplineStruct)

namespace PCGSplineStruct
{
	static int32 UpperBound(const TArray<FInterpCurvePoint<FVector>>& SplinePoints, float Value)
	{
		int32 Count = SplinePoints.Num();
		int32 First = 0;

		while (Count > 0)
		{
			const int32 Middle = Count / 2;
			if (Value >= SplinePoints[First + Middle].InVal)
			{
				First += Middle + 1;
				Count -= Middle + 1;
			}
			else
			{
				Count = Middle;
			}
		}

		return First;
	}

	// Note: copied verbatim from USplineComponent::CalcBounds
	static FBoxSphereBounds CalcBounds(const FSplineCurves& SplineCurves, bool bClosedLoop, const FTransform& LocalToWorld)
	{
//#if SPLINE_FAST_BOUNDS_CALCULATION
//		FBox BoundingBox(0);
//		for (const auto& InterpPoint : SplineCurves.Position.Points)
//		{
//			BoundingBox += InterpPoint.OutVal;
//		}
//
//		return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
//#else
		const int32 NumPoints = SplineCurves.Position.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

		FVector Min(WORLD_MAX);
		FVector Max(-WORLD_MAX);
		if (NumSegments > 0)
		{
			for (int32 Index = 0; Index < NumSegments; Index++)
			{
				const bool bLoopSegment = (Index == NumPoints - 1);
				const int32 NextIndex = bLoopSegment ? 0 : Index + 1;
				const FInterpCurvePoint<FVector>& ThisInterpPoint = SplineCurves.Position.Points[Index];
				FInterpCurvePoint<FVector> NextInterpPoint = SplineCurves.Position.Points[NextIndex];
				if (bLoopSegment)
				{
					NextInterpPoint.InVal = ThisInterpPoint.InVal + SplineCurves.Position.LoopKeyOffset;
				}

				CurveVectorFindIntervalBounds(ThisInterpPoint, NextInterpPoint, Min, Max);
			}
		}
		else if (NumPoints == 1)
		{
			Min = Max = SplineCurves.Position.Points[0].OutVal;
		}
		else
		{
			Min = FVector::ZeroVector;
			Max = FVector::ZeroVector;
		}

		return FBoxSphereBounds(FBox(Min, Max).TransformBy(LocalToWorld));
//#endif
	}
}

void FPCGSplineStruct::Initialize(const USplineComponent* InSplineComponent)
{
	check(InSplineComponent);
	SplineCurves = InSplineComponent->SplineCurves;
	Transform = InSplineComponent->GetComponentTransform();
	DefaultUpVector = InSplineComponent->DefaultUpVector;
	ReparamStepsPerSegment = InSplineComponent->ReparamStepsPerSegment;
	bClosedLoop = InSplineComponent->IsClosedLoop();

	Bounds = InSplineComponent->Bounds;
	LocalBounds = InSplineComponent->CalcLocalBounds();
}

void FPCGSplineStruct::Initialize(const TArray<FSplinePoint>& InSplinePoints, bool bIsClosedLoop, const FTransform& InTransform)
{
	Transform = InTransform;
	DefaultUpVector = FVector::ZAxisVector;
	ReparamStepsPerSegment = 10; // default value in USplineComponent

	bClosedLoop = bIsClosedLoop;
	AddPoints(InSplinePoints, true);

	Bounds = PCGSplineStruct::CalcBounds(SplineCurves, bClosedLoop, InTransform);
	LocalBounds = PCGSplineStruct::CalcBounds(SplineCurves, bClosedLoop, FTransform::Identity);
}

void FPCGSplineStruct::ApplyTo(USplineComponent* InSplineComponent)
{
	check(InSplineComponent);

	InSplineComponent->ClearSplinePoints(false);
	InSplineComponent->SetComponentToWorld(Transform);
	InSplineComponent->DefaultUpVector = DefaultUpVector;
	InSplineComponent->ReparamStepsPerSegment = ReparamStepsPerSegment;

	InSplineComponent->SplineCurves = SplineCurves;
	InSplineComponent->bStationaryEndpoints = false;
	// TODO: metadata? might not be needed
	InSplineComponent->SetClosedLoop(bClosedLoop);
	InSplineComponent->UpdateSpline();
	InSplineComponent->UpdateBounds();
}

void FPCGSplineStruct::AddPoint(const FSplinePoint& InSplinePoint, bool bUpdateSpline)
{
	const int32 Index = PCGSplineStruct::UpperBound(SplineCurves.Position.Points, InSplinePoint.InputKey);

	SplineCurves.Position.Points.Insert(FInterpCurvePoint<FVector>(
		InSplinePoint.InputKey,
		InSplinePoint.Position,
		InSplinePoint.ArriveTangent,
		InSplinePoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(InSplinePoint.Type)
		),
		Index);

	SplineCurves.Rotation.Points.Insert(FInterpCurvePoint<FQuat>(
		InSplinePoint.InputKey,
		InSplinePoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
		),
		Index);

	SplineCurves.Scale.Points.Insert(FInterpCurvePoint<FVector>(
		InSplinePoint.InputKey,
		InSplinePoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
		),
		Index);

	// TODO spline metadata?
	//USplineMetadata* Metadata = GetSplinePointsMetadata();
	//if (Metadata)
	//{
	//	Metadata->AddPoint(InSplinePoint.InputKey);
	//}

	if (bUpdateSpline)
	{
		UpdateSpline();
	}
}

void FPCGSplineStruct::AddPoints(const TArray<FSplinePoint>& InSplinePoints, bool bUpdateSpline)
{
	SplineCurves.Position.Points.Reserve(SplineCurves.Position.Points.Num() + InSplinePoints.Num());
	SplineCurves.Rotation.Points.Reserve(SplineCurves.Rotation.Points.Num() + InSplinePoints.Num());
	SplineCurves.Scale.Points.Reserve(SplineCurves.Scale.Points.Num() + InSplinePoints.Num());

	for (const auto& SplinePoint : InSplinePoints)
	{
		AddPoint(SplinePoint, false);
	}

	if (bUpdateSpline)
	{
		UpdateSpline();
	}
}

void FPCGSplineStruct::UpdateSpline()
{
	const bool bLoopPositionOverride = false;
	const bool bStationaryEndpoints = false;
	const float LoopPosition = 0.0f;

	SplineCurves.UpdateSpline(bClosedLoop, bStationaryEndpoints, ReparamStepsPerSegment, bLoopPositionOverride, LoopPosition, Transform.GetScale3D());
}

int FPCGSplineStruct::GetNumberOfSplineSegments() const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	return (bClosedLoop ? NumPoints : NumPoints - 1);
}

FVector::FReal FPCGSplineStruct::GetSplineLength() const
{
	return SplineCurves.GetSplineLength();
}

FBox FPCGSplineStruct::GetBounds() const
{
	// See USplineComponent::CalcBounds
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	FVector Min(WORLD_MAX);
	FVector Max(-WORLD_MAX);
	if (NumSegments > 0)
	{
		for (int32 Index = 0; Index < NumSegments; Index++)
		{
			const bool bLoopSegment = (Index == NumPoints - 1);
			const int32 NextIndex = bLoopSegment ? 0 : Index + 1;
			const FInterpCurvePoint<FVector>& ThisInterpPoint = SplineCurves.Position.Points[Index];
			FInterpCurvePoint<FVector> NextInterpPoint = SplineCurves.Position.Points[NextIndex];
			if (bLoopSegment)
			{
				NextInterpPoint.InVal = ThisInterpPoint.InVal /* + SplineCurves.Position.LoopKeyOffset*/;
			}

			CurveVectorFindIntervalBounds(ThisInterpPoint, NextInterpPoint, Min, Max);
		}
	}
	else if (NumPoints == 1)
	{
		Min = Max = SplineCurves.Position.Points[0].OutVal;
	}
	else
	{
		Min = FVector::ZeroVector;
		Max = FVector::ZeroVector;
	}

	return FBox(Min, Max);
}

const FInterpCurveVector& FPCGSplineStruct::GetSplinePointsScale() const
{
	return SplineCurves.Scale;
}

const FInterpCurveVector& FPCGSplineStruct::GetSplinePointsPosition() const
{
	return SplineCurves.Position;
}

FVector::FReal FPCGSplineStruct::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	// Ensure that if the reparam table is not prepared yet we don't attempt to access it. This can happen
	// early in the construction of the spline component object.
	if ((PointIndex >= 0) && (PointIndex < NumSegments + 1) && ((PointIndex * ReparamStepsPerSegment) < SplineCurves.ReparamTable.Points.Num()))
	{
		return SplineCurves.ReparamTable.Points[PointIndex * ReparamStepsPerSegment].InVal;
	}

	return 0.0f;
}

FVector FPCGSplineStruct::GetLocationAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineCurves.ReparamTable.Eval(Distance, 0.0f);
	return GetLocationAtSplineInputKey(Param, CoordinateSpace);
}

FTransform FPCGSplineStruct::GetTransformAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const float Param = SplineCurves.ReparamTable.Eval(Distance, 0.0f);
	return GetTransformAtSplineInputKey(Param, CoordinateSpace, bUseScale);
}

FVector FPCGSplineStruct::GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FQuat Quat = GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local);
	FVector RightVector = Quat.RotateVector(FVector::RightVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		RightVector = Transform.TransformVectorNoScale(RightVector);
	}

	return RightVector;
}

FTransform FPCGSplineStruct::GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FVector Scale = bUseScale ? GetScaleAtSplineInputKey(InKey) : FVector(1.0f);

	FTransform KeyTransform(Rotation, Location, Scale);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		KeyTransform = KeyTransform * Transform;
	}

	return KeyTransform;
}

float FPCGSplineStruct::FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = Transform.InverseTransformPosition(WorldLocation);
	float Dummy;
	return SplineCurves.Position.InaccurateFindNearest(LocalLocation, Dummy);
}

FVector FPCGSplineStruct::GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Location = SplineCurves.Position.Eval(InKey, FVector::ZeroVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Location = Transform.TransformPosition(Location);
	}

	return Location;
}

FQuat FPCGSplineStruct::GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = Transform.GetRotation() * Rot;
	}


	return Rot;
}

FVector FPCGSplineStruct::GetScaleAtSplineInputKey(float InKey) const
{
	return SplineCurves.Scale.Eval(InKey, FVector(1.0f));
}