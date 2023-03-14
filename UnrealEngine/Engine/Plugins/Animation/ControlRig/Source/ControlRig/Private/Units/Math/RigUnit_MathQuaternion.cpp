// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathQuaternion.h"
#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/RigUnitContext.h"
#include "AnimationCoreLibrary.h"
#include "Math/ControlRigMathLibrary.h"
#include "Units/Core/RigUnit_CoreDispatch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathQuaternion)

FRigUnit_MathQuaternionFromAxisAndAngle_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Axis.IsNearlyZero())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Axis is nearly zero"));
		Result = FQuat::Identity;
		return;
	}
	Result = FQuat(Axis.GetUnsafeNormal(), Angle);
}

FRigUnit_MathQuaternionFromEuler_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = AnimationCore::QuatFromEuler(Euler, RotationOrder);
}

FRigUnit_MathQuaternionFromRotator_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FQuat(Rotator);
}

FRigVMStructUpgradeInfo FRigUnit_MathQuaternionFromRotator::GetUpgradeInfo() const
{
	FRigUnit_MathQuaternionFromRotatorV2 NewNode;
	NewNode.Value = Rotator;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Rotator"), TEXT("Value"));
	return Info;
}

FRigUnit_MathQuaternionFromRotatorV2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FQuat(Value);
}

FRigUnit_MathQuaternionFromTwoVectors_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		Result = FQuat::Identity;
		return;
	}
	Result = FControlRigMathLibrary::FindQuatBetweenVectors(A, B);
}

FRigUnit_MathQuaternionToAxisAndAngle_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Value.GetNormalized().ToAxisAndAngle(Axis, Angle);
	if (Axis.IsNearlyZero())
	{
		Axis = FVector(1.f, 0.f, 0.f);
		Angle = 0.f;
	}


	float AngleSign = Angle < 0.f ? -1.f : 1.f;
	Angle = FMath::Abs(Angle);

	if (Angle > TWO_PI)
	{
		Angle = FMath::Fmod(Angle, TWO_PI);
	}
	if (Angle > PI)
	{
		Angle = TWO_PI - Angle;
		AngleSign = -AngleSign;
	}

	Angle = Angle * AngleSign;
}

FRigUnit_MathQuaternionScale_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FVector Axis = FVector::ZeroVector;
	float Angle = 0.f;
	Value.ToAxisAndAngle(Axis, Angle);
	Value = FQuat(Axis, Angle * Scale);
}

FRigVMStructUpgradeInfo FRigUnit_MathQuaternionScale::GetUpgradeInfo() const
{
	FRigUnit_MathQuaternionScaleV2 NewNode;
	NewNode.Value = Value;
	NewNode.Factor = Scale;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Scale"), TEXT("Factor"));
	Info.AddRemappedPin(TEXT("Value"), TEXT("Result"), false, true);
	return Info;
}

FRigUnit_MathQuaternionScaleV2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FVector Axis = FVector::ZeroVector;
	float Angle = 0.f;
	Value.ToAxisAndAngle(Axis, Angle);
	Result = FQuat(Axis, Angle * Factor);
}

FRigUnit_MathQuaternionToEuler_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = AnimationCore::EulerFromQuat(Value, RotationOrder);
}

FRigUnit_MathQuaternionToRotator_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.Rotator();
}

FRigUnit_MathQuaternionMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathQuaternionInverse_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.Inverse();
}

FRigUnit_MathQuaternionSlerp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FQuat::Slerp(A, B, T);
}

FRigUnit_MathQuaternionEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigUnit_MathQuaternionEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreEquals::StaticStruct());
}

FRigUnit_MathQuaternionNotEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigUnit_MathQuaternionNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreNotEquals::StaticStruct());
}

FRigUnit_MathQuaternionSelectBool_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Condition ? IfTrue : IfFalse;
}

FRigVMStructUpgradeInfo FRigUnit_MathQuaternionSelectBool::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathQuaternionDot_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A | B;
}

FRigUnit_MathQuaternionUnit_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.GetNormalized();
}

FRigUnit_MathQuaternionRotateVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Transform.RotateVector(Vector);
}

FRigUnit_MathQuaternionGetAxis_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	switch (Axis)
	{
		default:
		case EAxis::X:
		{
			Result = Quaternion.GetAxisX();
			break;
		}
		case EAxis::Y:
		{
			Result = Quaternion.GetAxisY();
			break;
		}
		case EAxis::Z:
		{
			Result = Quaternion.GetAxisZ();
			break;
		}
	}
}


FRigUnit_MathQuaternionSwingTwist_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (TwistAxis.IsNearlyZero())
	{
		Swing = Twist = FQuat::Identity;
		return;
	}

	FVector NormalizedAxis = TwistAxis.GetSafeNormal();
	Input.ToSwingTwist(NormalizedAxis, Swing, Twist);
}

FRigUnit_MathQuaternionRotationOrder_Execute()
{
}

FRigUnit_MathQuaternionMakeRelative_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Local = Parent.Inverse() * Global;
	Local.Normalize();
}

FRigUnit_MathQuaternionMakeAbsolute_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Global = Parent * Local;
	Global.Normalize();
}

FRigUnit_MathQuaternionMirrorTransform_Execute()
{
	FTransform Transform = FTransform::Identity;
	Transform.SetRotation(Value);
	FRigUnit_MathTransformMirrorTransform::StaticExecute(RigVMExecuteContext, Transform, MirrorAxis, AxisToFlip, CentralTransform, Transform, Context);
	Result = Transform.GetRotation();
}

