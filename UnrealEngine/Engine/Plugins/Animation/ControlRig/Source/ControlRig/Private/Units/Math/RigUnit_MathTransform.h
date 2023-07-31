// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EulerTransform.h"
#include "RigUnit_MathBase.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigUnit_MathTransform.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Transform", MenuDescSuffix="(Transform)"))
struct CONTROLRIG_API FRigUnit_MathTransformBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category="Math|Transform", MenuDescSuffix="(Transform)"))
struct CONTROLRIG_API FRigUnit_MathTransformMutableBase : public FRigUnit_MathMutableBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathTransformUnaryOp : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformUnaryOp()
	{
		Value = Result = FTransform::Identity;
	}

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathTransformBinaryOp : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformBinaryOp()
	{
		A = B = Result = FTransform::Identity;
	}

	UPROPERTY(meta=(Input))
	FTransform A;

	UPROPERTY(meta=(Input))
	FTransform B;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathTransformBinaryAggregateOp : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformBinaryAggregateOp()
	{
		A = B = Result = FTransform::Identity;
	}

	UPROPERTY(meta=(Input, Aggregate))
	FTransform A;

	UPROPERTY(meta=(Input, Aggregate))
	FTransform B;

	UPROPERTY(meta=(Output, Aggregate))
	FTransform Result;
};

/**
 * Makes a quaternion based transform from a euler based transform
 */
USTRUCT(meta=(DisplayName="From Euler Transform", TemplateName="FromEulerTransform", Keywords="Make,Construct", Deprecated="5.0.1"))
struct CONTROLRIG_API FRigUnit_MathTransformFromEulerTransform : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformFromEulerTransform()
	{
		EulerTransform = FEulerTransform::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FEulerTransform EulerTransform;

	UPROPERTY(meta=(Output))
	FTransform Result;
	
	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Makes a quaternion based transform from a euler based transform
 */
USTRUCT(meta=(DisplayName="To Transform", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathTransformFromEulerTransformV2 : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformFromEulerTransformV2()
	{
		Value = FEulerTransform::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FEulerTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Retrieves a euler based transform from a quaternion based transform
 */
USTRUCT(meta=(DisplayName="To Euler Transform", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathTransformToEulerTransform : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformToEulerTransform()
	{
		Value = FTransform::Identity;
		Result = FEulerTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FEulerTransform Result;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*,Global"))
struct CONTROLRIG_API FRigUnit_MathTransformMul : public FRigUnit_MathTransformBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the relative local transform within a parent's transform
 */
USTRUCT(meta=(DisplayName="Make Relative", TemplateName="Make Relative", Keywords="Local,Global,Absolute"))
struct CONTROLRIG_API FRigUnit_MathTransformMakeRelative : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformMakeRelative()
	{
		Global = Parent = Local = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Global;

	UPROPERTY(meta=(Input))
	FTransform Parent;

	UPROPERTY(meta=(Output))
	FTransform Local;
};

/**
 * Returns the absolute global transform within a parent's transform
 */
USTRUCT(meta = (DisplayName = "Make Absolute", TemplateName="Make Absolute", Keywords = "Local,Global,Relative"))
struct CONTROLRIG_API FRigUnit_MathTransformMakeAbsolute : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformMakeAbsolute()
	{
		Global = Parent = Local = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FTransform Local;

	UPROPERTY(meta = (Input))
	FTransform Parent;

	UPROPERTY(meta = (Output))
	FTransform Global;
};

/**
* Treats the provided transforms as a chain with global / local transforms, and
* projects each transform into the target space. Optionally you can provide
* a custom parent indices array, with which you can represent more than just chains.
*/
USTRUCT(meta=(DisplayName="Make Transform Array Relative", Keywords="Local,Global,Absolute,Array,Accumulate"))
struct CONTROLRIG_API FRigUnit_MathTransformAccumulateArray : public FRigUnit_MathTransformMutableBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformAccumulateArray()
	{
		TargetSpace = EBoneGetterSetterMode::GlobalSpace;
		Root = FTransform::Identity;
	}

	virtual FString GetUnitLabel() const override;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input, Output))
	TArray<FTransform> Transforms;

	/**
	* Defines the space to project to
	*/ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode TargetSpace;

	/**
	 * Provides the parent transform for the root
	 */
	UPROPERTY(meta=(Input))
	FTransform Root;

	/**
	 * If this array is the same size as the transforms array the indices will be used
	 * to look up each transform's parent. They are expected to be in order.
	 */
	UPROPERTY(meta=(Input))
	TArray<int32> ParentIndices;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Inverse", TemplateName="Inverse"))
struct CONTROLRIG_API FRigUnit_MathTransformInverse : public FRigUnit_MathTransformUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct CONTROLRIG_API FRigUnit_MathTransformLerp : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformLerp()
	{
		A = B = Result = FTransform::Identity;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform A;

	UPROPERTY(meta=(Input))
	FTransform B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Return one of the two values based on the condition
 */
USTRUCT(meta=(DisplayName="Select", TemplateName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct CONTROLRIG_API FRigUnit_MathTransformSelectBool : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformSelectBool()
	{
		Condition = false;
		IfTrue = IfFalse = Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	bool Condition;

	UPROPERTY(meta=(Input))
	FTransform IfTrue;

	UPROPERTY(meta=(Input))
	FTransform IfFalse;

	UPROPERTY(meta=(Output))
	FTransform Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Rotates a given vector (direction) by the transform
 */
USTRUCT(meta=(DisplayName="Rotate Vector", TemplateName="Rotate Vector", Keywords="Transform,Direction,TransformDirection"))
struct CONTROLRIG_API FRigUnit_MathTransformRotateVector : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformRotateVector()
	{
		Transform = FTransform::Identity;
		Vector = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Transform;

	UPROPERTY(meta=(Input))
	FVector Vector;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Multiplies a given vector (location) by the transform
 */
USTRUCT(meta=(DisplayName="Transform Location", Keywords="Multiply"))
struct CONTROLRIG_API FRigUnit_MathTransformTransformVector : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformTransformVector()
	{
		Transform = FTransform::Identity;
		Location = Result = FVector::ZeroVector;
	}
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Transform;

	UPROPERTY(meta=(Input))
	FVector Location;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Composes a Transform (and Euler Transform) from its components.
 */
USTRUCT(meta=(DisplayName="Transform from SRT", Keywords ="EulerTransform,Scale,Rotation,Orientation,Translation,Location"))
struct CONTROLRIG_API FRigUnit_MathTransformFromSRT : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformFromSRT()
	{
		Location = FVector::ZeroVector;
		Rotation = FVector::ZeroVector;
		RotationOrder = EEulerRotationOrder::XYZ;
		Scale = FVector::OneVector;
		Transform = FTransform::Identity;
		EulerTransform = FEulerTransform::Identity;
	}
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FVector Location;

	UPROPERTY(meta=(Input))
	FVector Rotation;

	UPROPERTY(meta=(Input))
	EEulerRotationOrder RotationOrder;

	UPROPERTY(meta=(Input))
	FVector Scale;

	UPROPERTY(meta=(Output))
	FTransform Transform;

	UPROPERTY(meta=(Output))
	FEulerTransform EulerTransform;
};


/**
 * Decomposes a Transform Array to its components.
 */
USTRUCT(meta = (DisplayName = "Transform Array to SRT", Keywords = "EulerTransform,Scale,Rotation,Orientation,Translation,Location"))
struct CONTROLRIG_API FRigUnit_MathTransformArrayToSRT : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformArrayToSRT()
	{
	}

	RIGVM_METHOD()
		virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	TArray<FTransform> Transforms;

	UPROPERTY(meta = (Output))
	TArray<FVector> Translations;

	UPROPERTY(meta = (Output))
	TArray<FQuat> Rotations;

	UPROPERTY(meta = (Output))
	TArray<FVector> Scales;
};

/**
 * Clamps a position using a plane collision, cylindric collision or spherical collision.
 */
USTRUCT(meta = (DisplayName = "Clamp Spatially", TemplateName="ClampSpatially", Keywords = "Collide,Collision"))
struct CONTROLRIG_API FRigUnit_MathTransformClampSpatially : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformClampSpatially()
	{
		Value = Result = FTransform::Identity;
		Axis = EAxis::X;
		Type = EControlRigClampSpatialMode::Plane;
		Minimum = 0.f;
		Maximum = 100.f;
		Space = FTransform::Identity;
		bDrawDebug = false;
		DebugColor = FLinearColor::Red;
		DebugThickness = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FTransform Value;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EAxis::Type> Axis;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EControlRigClampSpatialMode::Type> Type;

	UPROPERTY(meta = (Input))
	float Minimum;

	UPROPERTY(meta = (Input))
	float Maximum;

	// The space this spatial clamp happens within.
	// The input position will be projected into this space.
	UPROPERTY(meta = (Input))
	FTransform Space;

	UPROPERTY(meta = (Input))
	bool bDrawDebug;

	UPROPERTY(meta = (Input))
	FLinearColor DebugColor;

	UPROPERTY(meta = (Input))
	float DebugThickness;

	UPROPERTY(meta = (Output))
	FTransform Result;
};

/**
 * Mirror a transform about a central transform.
 */
USTRUCT(meta=(DisplayName="Mirror", TemplateName="Mirror"))
struct CONTROLRIG_API FRigUnit_MathTransformMirrorTransform : public FRigUnit_MathTransformBase
{
	GENERATED_BODY()

	FRigUnit_MathTransformMirrorTransform()
	{
		Value = Result = FTransform::Identity;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	// the axis to mirror against
	UPROPERTY(meta=(Input))
	TEnumAsByte<EAxis::Type> MirrorAxis;

	// the axis to flip for rotations
	UPROPERTY(meta=(Input))
	TEnumAsByte<EAxis::Type> AxisToFlip;

	// The transform about which to mirror
	UPROPERTY(meta=(Input))
	FTransform CentralTransform;

	UPROPERTY(meta=(Output))
	FTransform Result;
};
