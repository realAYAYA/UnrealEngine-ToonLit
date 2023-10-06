// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Math/RigVMFunction_MathVector.h"
#include "RigVMFunctions/Math/RigVMFunction_MathTransform.h"
#include "RigVMFunctions/RigVMDispatch_Core.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_MathVector)

FRigVMFunction_MathVectorMake_Execute()
{
	Result = FVector(X, Y, Z);
}

FRigVMFunction_MathVectorFromFloat_Execute()
{
	Result = FVector(Value, Value, Value);
}

FRigVMFunction_MathVectorFromDouble_Execute()
{
	Result = FVector((float)Value, (float)Value, (float)Value);
}

FRigVMFunction_MathVectorAdd_Execute()
{
	Result = A + B;
}

FRigVMFunction_MathVectorSub_Execute()
{
	Result = A - B;
}

FRigVMFunction_MathVectorMul_Execute()
{
	Result = A * B;
}

FRigVMFunction_MathVectorScale_Execute()
{
	Result = Value * Factor;
}

FRigVMFunction_MathVectorDiv_Execute()
{
	if(FMath::IsNearlyZero(B.X) || FMath::IsNearlyZero(B.Y) || FMath::IsNearlyZero(B.Z))
	{
		if (FMath::IsNearlyZero(B.X))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.X is nearly 0.f"));
		}
		if (FMath::IsNearlyZero(B.Y))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.Y is nearly 0.f"));
		}
		if (FMath::IsNearlyZero(B.Z))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.Z is nearly 0.f"));
		}
		Result = FVector::ZeroVector;
		return;
	}
	Result = A / B;
}

FRigVMFunction_MathVectorMod_Execute()
{
	if(FMath::IsNearlyZero(B.X) || FMath::IsNearlyZero(B.Y) || FMath::IsNearlyZero(B.Z) || B.X < 0.f || B.Y < 0.f || B.Z < 0.f)
	{
		if (FMath::IsNearlyZero(B.X) || B.X < 0.f)
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.X needs to be greater than 0"));
		}
		if (FMath::IsNearlyZero(B.Y) || B.Y < 0.f)
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.Y needs to be greater than 0"));
		}
		if (FMath::IsNearlyZero(B.Z) || B.Z < 0.f)
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B.Z needs to be greater than 0"));
		}
		Result = FVector::ZeroVector;
		return;
	}

	Result.X = FMath::Fmod(A.X, B.X);
	Result.Y = FMath::Fmod(A.Y, B.Y);
	Result.Z = FMath::Fmod(A.Z, B.Z);
}

FRigVMFunction_MathVectorMin_Execute()
{
	Result.X = FMath::Min<float>(A.X, B.X);
	Result.Y = FMath::Min<float>(A.Y, B.Y);
	Result.Z = FMath::Min<float>(A.Z, B.Z);
}

FRigVMFunction_MathVectorMax_Execute()
{
	Result.X = FMath::Max<float>(A.X, B.X);
	Result.Y = FMath::Max<float>(A.Y, B.Y);
	Result.Z = FMath::Max<float>(A.Z, B.Z);
}

FRigVMFunction_MathVectorNegate_Execute()
{
	Result = -Value;
}

FRigVMFunction_MathVectorAbs_Execute()
{
	Result.X = FMath::Abs(Value.X);
	Result.Y = FMath::Abs(Value.Y);
	Result.Z = FMath::Abs(Value.Z);
}

FRigVMFunction_MathVectorFloor_Execute()
{
	Result.X = FMath::FloorToFloat(Value.X);
	Result.Y = FMath::FloorToFloat(Value.Y);
	Result.Z = FMath::FloorToFloat(Value.Z);
}

FRigVMFunction_MathVectorCeil_Execute()
{
	Result.X = FMath::CeilToFloat(Value.X);
	Result.Y = FMath::CeilToFloat(Value.Y);
	Result.Z = FMath::CeilToFloat(Value.Z);
}

FRigVMFunction_MathVectorRound_Execute()
{
	Result.X = FMath::RoundToFloat(Value.X);
	Result.Y = FMath::RoundToFloat(Value.Y);
	Result.Z = FMath::RoundToFloat(Value.Z);
}

FRigVMFunction_MathVectorSign_Execute()
{
	Result = Value.GetSignVector();
}

FRigVMFunction_MathVectorClamp_Execute()
{
	Result.X = FMath::Clamp<float>(Value.X, Minimum.X, Maximum.X);
	Result.Y = FMath::Clamp<float>(Value.Y, Minimum.Y, Maximum.Y);
	Result.Z = FMath::Clamp<float>(Value.Z, Minimum.Z, Maximum.Z);
}

FRigVMFunction_MathVectorLerp_Execute()
{
	Result = FMath::Lerp<FVector>(A, B, T);
}

FRigVMFunction_MathVectorRemap_Execute()
{
	FVector Ratio(0.f, 0.f, 0.f);
	if (FMath::IsNearlyEqual(SourceMinimum.X, SourceMaximum.X) || FMath::IsNearlyEqual(SourceMinimum.Y, SourceMaximum.Y) || FMath::IsNearlyEqual(SourceMinimum.Z, SourceMaximum.Z))
	{
		if (FMath::IsNearlyEqual(SourceMinimum.X, SourceMaximum.X))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("The source minimum and maximum X are the same."));
		}
		if (FMath::IsNearlyEqual(SourceMinimum.Y, SourceMaximum.Y))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("The source minimum and maximum Y are the same."));
		}
		if (FMath::IsNearlyEqual(SourceMinimum.Z, SourceMaximum.Z))
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("The source minimum and maximum Z are the same."));
		}
	}
	else
	{
		Ratio = (Value - SourceMinimum) / (SourceMaximum - SourceMinimum);
	}
	if (bClamp)
	{
		Ratio.X = FMath::Clamp<float>(Ratio.X, 0.f, 1.f);
		Ratio.Y = FMath::Clamp<float>(Ratio.Y, 0.f, 1.f);
		Ratio.Z = FMath::Clamp<float>(Ratio.Z, 0.f, 1.f);
	}
	Result = FMath::Lerp<FVector>(TargetMinimum, TargetMaximum, Ratio);
}

FRigVMFunction_MathVectorEquals_Execute()
{
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigVMFunction_MathVectorEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigVMDispatch_CoreEquals::StaticStruct());
}

FRigVMFunction_MathVectorNotEquals_Execute()
{
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigVMFunction_MathVectorNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigVMDispatch_CoreNotEquals::StaticStruct());
}

FRigVMFunction_MathVectorIsNearlyZero_Execute()
{
	if(Tolerance < 0.f)
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = Value.IsNearlyZero(FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigVMFunction_MathVectorIsNearlyEqual_Execute()
{
	if(Tolerance < 0.f)
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = (A - B).IsNearlyZero(FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigVMFunction_MathVectorSelectBool_Execute()
{
	Result = Condition ? IfTrue : IfFalse;
}

FRigVMStructUpgradeInfo FRigVMFunction_MathVectorSelectBool::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigVMFunction_MathVectorDeg_Execute()
{
	Result = FMath::RadiansToDegrees(Value);
}

FRigVMFunction_MathVectorRad_Execute()
{
	Result = FMath::DegreesToRadians(Value);
}

FRigVMFunction_MathVectorLengthSquared_Execute()
{
	Result = Value.SizeSquared();
}

FRigVMFunction_MathVectorLength_Execute()
{
	Result = Value.Size();
}

FRigVMFunction_MathVectorDistance_Execute()
{
	Result = FVector::Distance(A, B);
}

FRigVMFunction_MathVectorCross_Execute()
{
	Result = A ^ B;
}

FRigVMFunction_MathVectorDot_Execute()
{
	Result = A | B;
}

FRigVMFunction_MathVectorUnit_Execute()
{
	if (Value.IsNearlyZero())
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.GetUnsafeNormal();
}

FRigVMFunction_MathVectorSetLength_Execute()
{
	if (Value.IsNearlyZero())
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.GetUnsafeNormal() * Length;
}

FRigVMFunction_MathVectorClampLength_Execute()
{
	if (Value.IsNearlyZero())
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	float Length = Value.Size();
	Result = Value * FMath::Clamp<float>(Length, MinimumLength, MaximumLength) / Length;
}

FRigVMFunction_MathVectorMirror_Execute()
{
	if (Normal.IsNearlyZero())
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Normal is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.MirrorByVector(Normal.GetSafeNormal());
}

FRigVMFunction_MathVectorAngle_Execute()
{
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = 0.f;
		return;
	}
	Result = FRigVMMathLibrary::FindQuatBetweenVectors(A, B).GetAngle();
}

FRigVMFunction_MathVectorParallel_Execute()
{
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = false;
		return;
	}
	Result = FVector::Parallel(A, B);
}

FRigVMFunction_MathVectorOrthogonal_Execute()
{
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = false;
		return;
	}
	Result = FVector::Orthogonal(A, B);
}

FRigVMFunction_MathVectorBezierFourPoint_Execute()
{
	FRigVMMathLibrary::FourPointBezier(Bezier, T, Result, Tangent);
}

FRigVMStructUpgradeInfo FRigVMFunction_MathVectorBezierFourPoint::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigVMFunction_MathVectorMakeBezierFourPoint_Execute()
{
}

FRigVMStructUpgradeInfo FRigVMFunction_MathVectorMakeBezierFourPoint::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigVMFunction_MathVectorClampSpatially_Execute()
{
	Result = FRigVMMathLibrary::ClampSpatially(Value, Axis, Type, Minimum, Maximum, Space);

	if (ExecuteContext.GetDrawInterface() != nullptr && bDrawDebug)
	{
		switch (Type)
		{
			case ERigVMClampSpatialMode::Plane:
			{
				TArray<FVector> Points;
				Points.SetNumUninitialized(2);
					
				switch (Axis)
				{
					case EAxis::X:
					{
						Points[0] = FVector(Minimum, 0.f, 0.f);
						Points[1] = FVector(Maximum, 0.f, 0.f);
						break;
					}
					case EAxis::Y:
					{
						Points[0] = FVector(0.f, Minimum, 0.f);
						Points[1] = FVector(0.f, Maximum, 0.f);
						break;
					}
					default:
					{
						Points[0] = FVector(0.f, 0.f, Minimum);
						Points[1] = FVector(0.f, 0.f, Maximum);
						break;
					}
				}

				ExecuteContext.GetDrawInterface()->DrawLine(Space, Points[0], Points[1], DebugColor, DebugThickness);
				ExecuteContext.GetDrawInterface()->DrawPoints(Space, Points, DebugThickness * 8.f, DebugColor);

				break;
			}
			case ERigVMClampSpatialMode::Cylinder:
			{
				FTransform CircleTransform = FTransform::Identity;
				switch (Axis)
				{
					case EAxis::X:
					{
						CircleTransform.SetRotation(FQuat(FVector(0.f, 1.f, 0.f), PI * 0.5f));
						break;
					}
					case EAxis::Y:
					{
						CircleTransform.SetRotation(FQuat(FVector(1.f, 0.f, 0.f), PI * 0.5f));
						break;
					}
					default:
					{
						break;
					}
				}
				if (Minimum > SMALL_NUMBER)
				{
					ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				break;
			}
			default:
			case ERigVMClampSpatialMode::Sphere:
			{
				FTransform CircleTransform = FTransform::Identity;
				if (Minimum > SMALL_NUMBER)
				{
					ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				CircleTransform.SetRotation(FQuat(FVector(0.f, 1.f, 0.f), PI * 0.5f));
				if (Minimum > SMALL_NUMBER)
				{
					ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				CircleTransform.SetRotation(FQuat(FVector(1.f, 0.f, 0.f), PI * 0.5f));
				if (Minimum > SMALL_NUMBER)
				{
					ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				ExecuteContext.GetDrawInterface()->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				break;
			}

		}
	}
}

FRigVMFunction_MathIntersectPlane_Execute()
{
	FPlane Plane(PlanePoint, PlaneNormal);

	Result = FMath::RayPlaneIntersection(Start, Direction, Plane);
	Distance = (Start - Result).Size();
}

FRigVMFunction_MathDistanceToPlane_Execute()
{
	FVector UnitVectorNormal = PlaneNormal.GetSafeNormal();

	if (!UnitVectorNormal.IsZero())
	{ 
		FPlane Plane(PlanePoint, UnitVectorNormal);

		ClosestPointOnPlane = FVector::PointPlaneProject(Point, Plane);
		SignedDistance = FVector::PointPlaneDist(Point, PlanePoint, UnitVectorNormal);
	}
	else
	{
		ClosestPointOnPlane = FVector::ZeroVector;
		SignedDistance = 0;
	}
}

FRigVMFunction_MathVectorMakeRelative_Execute()
{
	Local = Global - Parent;
}

FRigVMFunction_MathVectorMakeAbsolute_Execute()
{
	Global = Parent + Local;
}

FRigVMFunction_MathVectorMirrorTransform_Execute()
{
	FTransform Transform = FTransform::Identity;
	Transform.SetTranslation(Value);
	FRigVMFunction_MathTransformMirrorTransform::StaticExecute(ExecuteContext, Transform, MirrorAxis, AxisToFlip, CentralTransform, Transform);
	Result = Transform.GetTranslation();
}

FRigVMFunction_MathVectorArraySum_Execute()
{
	Sum = FVector::ZeroVector;
	for (const FVector& Value : Array)
	{
		Sum += Value;
	}
}

FRigVMFunction_MathVectorArrayAverage_Execute()
{
	Average = FVector::ZeroVector;
	if (!Array.IsEmpty())
	{
		for (const FVector& Value : Array)
		{
			Average += Value;
		}
		Average = Average / Array.Num();
	}
	else
	{
		UE_RIGVMSTRUCT_REPORT_WARNING(TEXT("Array is empty"));
	}
}

