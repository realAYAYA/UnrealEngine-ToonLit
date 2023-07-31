// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/Math/RigUnit_MathVector.h"
#include "Math/ControlRigMathLibrary.h"
#include "Units/RigUnitContext.h"
#include "AnimationCoreLibrary.h"
#include "Rigs/RigHierarchyDefines.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathTransform)

FRigUnit_MathTransformFromEulerTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = EulerTransform.ToFTransform();
}

FRigVMStructUpgradeInfo FRigUnit_MathTransformFromEulerTransform::GetUpgradeInfo() const
{
	FRigUnit_MathTransformFromEulerTransformV2 NewNode;
	NewNode.Value = EulerTransform;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("EulerTransform"), TEXT("Value"));
	return Info;
}

FRigUnit_MathTransformFromEulerTransformV2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.ToFTransform();
}

FRigUnit_MathTransformToEulerTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result.FromFTransform(Value);
}

FRigUnit_MathTransformMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathTransformMakeRelative_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Local = Global.GetRelativeTransform(Parent);
	Local.NormalizeRotation();
}

FRigUnit_MathTransformMakeAbsolute_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Global = Local * Parent;
	Global.NormalizeRotation();
}

FString FRigUnit_MathTransformAccumulateArray::GetUnitLabel() const
{
	static const FString RelativeLabel = TEXT("Make Transform Array Relative");
	static const FString AbsoluteLabel = TEXT("Make Transform Array Absolute");
	return (TargetSpace == EBoneGetterSetterMode::GlobalSpace) ? AbsoluteLabel : RelativeLabel;
}

FRigUnit_MathTransformAccumulateArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Transforms.Num() == 0)
	{
		return;
	}

	if(ParentIndices.Num() > 0 && ParentIndices.Num() != Transforms.Num())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("If the indices are specified their num (%d) has to match the transforms (%d)."), ParentIndices.Num(), Transforms.Num());
		return;
	}

	if(TargetSpace == EBoneGetterSetterMode::LocalSpace)
	{
		if(ParentIndices.IsEmpty())
		{
			for(int32 Index=Transforms.Num()-1; Index>=0;Index--)
			{
				const FTransform& ParentTransform = (Index == 0) ? Root : Transforms[Index - 1];
				Transforms[Index] = Transforms[Index].GetRelativeTransform(ParentTransform);
			}
		}
		else
		{
			for(int32 Index=Transforms.Num()-1; Index>=0;Index--)
			{
				const int32 ParentIndex = ParentIndices[Index];
				const FTransform& ParentTransform = (ParentIndex == INDEX_NONE || ParentIndex >= Index) ? Root : Transforms[ParentIndex];
				Transforms[Index] = Transforms[Index].GetRelativeTransform(ParentTransform);
			}
		}
	}
	else
	{
		if(ParentIndices.IsEmpty())
		{
			for(int32 Index=0; Index<Transforms.Num(); Index++)
			{
				const FTransform& ParentTransform = (Index == 0) ? Root : Transforms[Index - 1];
				Transforms[Index] = Transforms[Index] * ParentTransform;
			}
		}
		else
		{
			for(int32 Index=0; Index<Transforms.Num(); Index++)
			{
				const int32 ParentIndex = ParentIndices[Index];
				const FTransform& ParentTransform = (ParentIndex == INDEX_NONE || ParentIndex >= Index) ? Root : Transforms[ParentIndex];
				Transforms[Index] = Transforms[Index] * ParentTransform;
			}
		}
	}
}

FRigUnit_MathTransformInverse_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value.Inverse();
}

FRigUnit_MathTransformLerp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FControlRigMathLibrary::LerpTransform(A, B, T);
}

FRigUnit_MathTransformSelectBool_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Condition ? IfTrue : IfFalse;
}

FRigVMStructUpgradeInfo FRigUnit_MathTransformSelectBool::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathTransformRotateVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Transform.TransformVector(Vector);
}

FRigUnit_MathTransformTransformVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Transform.TransformPosition(Location);
}

FRigUnit_MathTransformFromSRT_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Transform.SetLocation(Location);
	Transform.SetRotation(AnimationCore::QuatFromEuler(Rotation, RotationOrder));
	Transform.SetScale3D(Scale);
	EulerTransform.FromFTransform(Transform);
}


FRigUnit_MathTransformArrayToSRT_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Transforms.Num() == 0)
	{
		return;
	}

	Translations.SetNumUninitialized(Transforms.Num());
	Rotations.SetNumUninitialized(Transforms.Num());
	Scales.SetNumUninitialized(Transforms.Num());

	for (int32 Index = 0; Index < Transforms.Num(); Index++)
	{
		Translations[Index] = Transforms[Index].GetTranslation();
		Rotations[Index] = Transforms[Index].GetRotation();
		Scales[Index] = Transforms[Index].GetScale3D();
	}

}

FRigUnit_MathTransformClampSpatially_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	FVector Position;
	FRigUnit_MathVectorClampSpatially::StaticExecute(RigVMExecuteContext, Value.GetTranslation(), Axis, Type, Minimum, Maximum, Space, bDrawDebug, DebugColor, DebugThickness, Position, Context);
	Result = Value;
	Result.SetTranslation(Position);
}

FRigUnit_MathTransformMirrorTransform_Execute()
{
	FRigMirrorSettings MirrorSettings;
	MirrorSettings.MirrorAxis = MirrorAxis;
	MirrorSettings.AxisToFlip = AxisToFlip;

	FTransform Local = FTransform::Identity;
	FRigUnit_MathTransformMakeRelative::StaticExecute(RigVMExecuteContext, Value, CentralTransform, Local, Context);
	Local = MirrorSettings.MirrorTransform(Local);
	FRigUnit_MathTransformMakeAbsolute::StaticExecute(RigVMExecuteContext, Local, CentralTransform, Result, Context);
}

