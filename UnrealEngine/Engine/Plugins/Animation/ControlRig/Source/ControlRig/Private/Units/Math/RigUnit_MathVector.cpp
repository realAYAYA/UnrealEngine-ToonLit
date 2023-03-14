// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathVector.h"
#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/RigUnitContext.h"
#include "Math/ControlRigMathLibrary.h"
#include "Units/Core/RigUnit_CoreDispatch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathVector)

FRigUnit_MathVectorFromFloat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FVector(Value, Value, Value);
}

FRigUnit_MathVectorAdd_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A + B;
}

FRigUnit_MathVectorSub_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A - B;
}

FRigUnit_MathVectorMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathVectorScale_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value * Factor;
}

FRigUnit_MathVectorDiv_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B.X) || FMath::IsNearlyZero(B.Y) || FMath::IsNearlyZero(B.Z))
	{
		if (FMath::IsNearlyZero(B.X))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.X is nearly 0.f"));
		}
		if (FMath::IsNearlyZero(B.Y))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.Y is nearly 0.f"));
		}
		if (FMath::IsNearlyZero(B.Z))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.Z is nearly 0.f"));
		}
		Result = FVector::ZeroVector;
		return;
	}
	Result = A / B;
}

FRigUnit_MathVectorMod_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B.X) || FMath::IsNearlyZero(B.Y) || FMath::IsNearlyZero(B.Z) || B.X < 0.f || B.Y < 0.f || B.Z < 0.f)
	{
		if (FMath::IsNearlyZero(B.X) || B.X < 0.f)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.X needs to be greater than 0"));
		}
		if (FMath::IsNearlyZero(B.Y) || B.Y < 0.f)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.Y needs to be greater than 0"));
		}
		if (FMath::IsNearlyZero(B.Z) || B.Z < 0.f)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B.Z needs to be greater than 0"));
		}
		Result = FVector::ZeroVector;
		return;
	}

	Result.X = FMath::Fmod(A.X, B.X);
	Result.Y = FMath::Fmod(A.Y, B.Y);
	Result.Z = FMath::Fmod(A.Z, B.Z);
}

FRigUnit_MathVectorMin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::Min<float>(A.X, B.X);
	Result.Y = FMath::Min<float>(A.Y, B.Y);
	Result.Z = FMath::Min<float>(A.Z, B.Z);
}

FRigUnit_MathVectorMax_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::Max<float>(A.X, B.X);
	Result.Y = FMath::Max<float>(A.Y, B.Y);
	Result.Z = FMath::Max<float>(A.Z, B.Z);
}

FRigUnit_MathVectorNegate_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = -Value;
}

FRigUnit_MathVectorAbs_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::Abs(Value.X);
	Result.Y = FMath::Abs(Value.Y);
	Result.Z = FMath::Abs(Value.Z);
}

FRigUnit_MathVectorFloor_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::FloorToFloat(Value.X);
	Result.Y = FMath::FloorToFloat(Value.Y);
	Result.Z = FMath::FloorToFloat(Value.Z);
}

FRigUnit_MathVectorCeil_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::CeilToFloat(Value.X);
	Result.Y = FMath::CeilToFloat(Value.Y);
	Result.Z = FMath::CeilToFloat(Value.Z);
}

FRigUnit_MathVectorRound_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::RoundToFloat(Value.X);
	Result.Y = FMath::RoundToFloat(Value.Y);
	Result.Z = FMath::RoundToFloat(Value.Z);
}

FRigUnit_MathVectorSign_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.GetSignVector();
}

FRigUnit_MathVectorClamp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.X = FMath::Clamp<float>(Value.X, Minimum.X, Maximum.X);
	Result.Y = FMath::Clamp<float>(Value.Y, Minimum.Y, Maximum.Y);
	Result.Z = FMath::Clamp<float>(Value.Z, Minimum.Z, Maximum.Z);
}

FRigUnit_MathVectorLerp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Lerp<FVector>(A, B, T);
}

FRigUnit_MathVectorRemap_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FVector Ratio(0.f, 0.f, 0.f);
	if (FMath::IsNearlyEqual(SourceMinimum.X, SourceMaximum.X) || FMath::IsNearlyEqual(SourceMinimum.Y, SourceMaximum.Y) || FMath::IsNearlyEqual(SourceMinimum.Z, SourceMaximum.Z))
	{
		if (FMath::IsNearlyEqual(SourceMinimum.X, SourceMaximum.X))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("The source minimum and maximum X are the same."));
		}
		if (FMath::IsNearlyEqual(SourceMinimum.Y, SourceMaximum.Y))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("The source minimum and maximum Y are the same."));
		}
		if (FMath::IsNearlyEqual(SourceMinimum.Z, SourceMaximum.Z))
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("The source minimum and maximum Z are the same."));
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

FRigUnit_MathVectorEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigUnit_MathVectorEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreEquals::StaticStruct());
}

FRigUnit_MathVectorNotEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigUnit_MathVectorNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreNotEquals::StaticStruct());
}

FRigUnit_MathVectorIsNearlyZero_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = Value.IsNearlyZero(FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathVectorIsNearlyEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = (A - B).IsNearlyZero(FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathVectorSelectBool_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Condition ? IfTrue : IfFalse;
}

FRigVMStructUpgradeInfo FRigUnit_MathVectorSelectBool::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathVectorDeg_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::RadiansToDegrees(Value);
}

FRigUnit_MathVectorRad_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::DegreesToRadians(Value);
}

FRigUnit_MathVectorLengthSquared_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.SizeSquared();
}

FRigUnit_MathVectorLength_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.Size();
}

FRigUnit_MathVectorDistance_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FVector::Distance(A, B);
}

FRigUnit_MathVectorCross_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A ^ B;
}

FRigUnit_MathVectorDot_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A | B;
}

FRigUnit_MathVectorUnit_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Value.IsNearlyZero())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.GetUnsafeNormal();
}

FRigUnit_MathVectorSetLength_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Value.IsNearlyZero())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.GetUnsafeNormal() * Length;
}

FRigUnit_MathVectorClampLength_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Value.IsNearlyZero())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	float Length = Value.Size();
	Result = Value * FMath::Clamp<float>(Length, MinimumLength, MaximumLength) / Length;
}

FRigUnit_MathVectorMirror_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Normal.IsNearlyZero())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Normal is nearly zero"));
		Result = FVector::ZeroVector;
		return;
	}
	Result = Value.MirrorByVector(Normal.GetSafeNormal());
}

FRigUnit_MathVectorAngle_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = 0.f;
		return;
	}
	Result = FControlRigMathLibrary::FindQuatBetweenVectors(A, B).GetAngle();
}

FRigUnit_MathVectorParallel_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = false;
		return;
	}
	Result = FVector::Parallel(A, B);
}

FRigUnit_MathVectorOrthogonal_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		if (A.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("A is nearly zero"));
		}
		if (B.IsNearlyZero())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B is nearly zero"));
		}
		Result = false;
		return;
	}
	Result = FVector::Orthogonal(A, B);
}

FRigUnit_MathVectorBezierFourPoint_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FControlRigMathLibrary::FourPointBezier(Bezier, T, Result, Tangent);
}

FRigVMStructUpgradeInfo FRigUnit_MathVectorBezierFourPoint::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathVectorMakeBezierFourPoint_Execute()
{
}

FRigVMStructUpgradeInfo FRigUnit_MathVectorMakeBezierFourPoint::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathVectorClampSpatially_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FControlRigMathLibrary::ClampSpatially(Value, Axis, Type, Minimum, Maximum, Space);

	if (Context.DrawInterface != nullptr && bDrawDebug)
	{
		switch (Type)
		{
			case EControlRigClampSpatialMode::Plane:
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

				Context.DrawInterface->DrawLine(Space, Points[0], Points[1], DebugColor, DebugThickness);
				Context.DrawInterface->DrawPoints(Space, Points, DebugThickness * 8.f, DebugColor);

				break;
			}
			case EControlRigClampSpatialMode::Cylinder:
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
					Context.DrawInterface->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				Context.DrawInterface->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				break;
			}
			default:
			case EControlRigClampSpatialMode::Sphere:
			{
				FTransform CircleTransform = FTransform::Identity;
				if (Minimum > SMALL_NUMBER)
				{
					Context.DrawInterface->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				Context.DrawInterface->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				CircleTransform.SetRotation(FQuat(FVector(0.f, 1.f, 0.f), PI * 0.5f));
				if (Minimum > SMALL_NUMBER)
				{
					Context.DrawInterface->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				Context.DrawInterface->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				CircleTransform.SetRotation(FQuat(FVector(1.f, 0.f, 0.f), PI * 0.5f));
				if (Minimum > SMALL_NUMBER)
				{
					Context.DrawInterface->DrawArc(Space, CircleTransform, Minimum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				}
				Context.DrawInterface->DrawArc(Space, CircleTransform, Maximum, 0.f, PI * 2.f, DebugColor, DebugThickness, 16);
				break;
			}

		}
	}
}

FRigUnit_MathIntersectPlane_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	FPlane Plane(PlanePoint, PlaneNormal);

	Result = FMath::RayPlaneIntersection(Start, Direction, Plane);
	Distance = (Start - Result).Size();
}

FRigUnit_MathDistanceToPlane_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

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

FRigUnit_MathVectorMakeRelative_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Local = Global - Parent;
}

FRigUnit_MathVectorMakeAbsolute_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Global = Parent + Local;
}

FRigUnit_MathVectorMirrorTransform_Execute()
{
	FTransform Transform = FTransform::Identity;
	Transform.SetTranslation(Value);
	FRigUnit_MathTransformMirrorTransform::StaticExecute(RigVMExecuteContext, Transform, MirrorAxis, AxisToFlip, CentralTransform, Transform, Context);
	Result = Transform.GetTranslation();
}
