// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EulerTransform.h"
#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathTransform.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Transform", MenuDescSuffix="(Transform)"))
struct RIGVM_API FRigVMFunction_MathTransformBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category="Math|Transform", MenuDescSuffix="(Transform)"))
struct RIGVM_API FRigVMFunction_MathTransformMutableBase : public FRigVMFunction_MathMutableBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathTransformUnaryOp : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformUnaryOp()
	{
		Value = Result = FTransform::Identity;
	}

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathTransformBinaryOp : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformBinaryOp()
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
struct RIGVM_API FRigVMFunction_MathTransformBinaryAggregateOp : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformBinaryAggregateOp()
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
 * Makes a transform from its components
 */
USTRUCT(meta=(DisplayName="Make Transform", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathTransformMake : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathTransformMake()
	{
		Translation = FVector::ZeroVector;
		Rotation = FQuat::Identity;
		Scale = FVector::OneVector;
		Result = FTransform::Identity;
	}

	UPROPERTY(meta=(Input))
	FVector Translation;

	UPROPERTY(meta=(Input))
	FQuat Rotation;

	UPROPERTY(meta=(Input))
	FVector Scale;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Makes a quaternion based transform from a euler based transform
 */
USTRUCT(meta=(DisplayName="From Euler Transform", TemplateName="FromEulerTransform", Keywords="Make,Construct", Deprecated="5.0.1"))
struct RIGVM_API FRigVMFunction_MathTransformFromEulerTransform : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformFromEulerTransform()
	{
		EulerTransform = FEulerTransform::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
USTRUCT(meta=(DisplayName="To Transform", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathTransformFromEulerTransformV2 : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformFromEulerTransformV2()
	{
		Value = FEulerTransform::Identity;
		Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FEulerTransform Value;

	UPROPERTY(meta=(Output))
	FTransform Result;
};

/**
 * Retrieves a euler based transform from a quaternion based transform
 */
USTRUCT(meta=(DisplayName="To Euler Transform", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathTransformToEulerTransform : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformToEulerTransform()
	{
		Value = FTransform::Identity;
		Result = FEulerTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FEulerTransform Result;
};

/**
 * Retrieves the forward, right and up vectors of a transform's quaternion
 */
USTRUCT(meta=(DisplayName="To Vectors", TemplateName="ToVectors", Keywords="Forward,Right,Up"))
struct RIGVM_API FRigVMFunction_MathTransformToVectors : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformToVectors()
	{
		Value = FTransform::Identity;
		Forward = Right = Up = FVector(0.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FTransform Value;

	UPROPERTY(meta=(Output))
	FVector Forward;

	UPROPERTY(meta=(Output))
	FVector Right;

	UPROPERTY(meta=(Output))
	FVector Up;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*,Global"))
struct RIGVM_API FRigVMFunction_MathTransformMul : public FRigVMFunction_MathTransformBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the relative local transform within a parent's transform
 */
USTRUCT(meta=(DisplayName="Make Relative", TemplateName="Make Relative", Keywords="Local,Global,Absolute"))
struct RIGVM_API FRigVMFunction_MathTransformMakeRelative : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformMakeRelative()
	{
		Global = Parent = Local = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformMakeAbsolute : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformMakeAbsolute()
	{
		Global = Parent = Local = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformAccumulateArray : public FRigVMFunction_MathTransformMutableBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformAccumulateArray()
	{
		TargetSpace = ERigVMTransformSpace::GlobalSpace;
		Root = FTransform::Identity;
	}

	virtual FString GetUnitLabel() const override;

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Output))
	TArray<FTransform> Transforms;

	/**
	* Defines the space to project to
	*/ 
	UPROPERTY(meta = (Input))
	ERigVMTransformSpace TargetSpace;

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
struct RIGVM_API FRigVMFunction_MathTransformInverse : public FRigVMFunction_MathTransformUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct RIGVM_API FRigVMFunction_MathTransformLerp : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformLerp()
	{
		A = B = Result = FTransform::Identity;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
USTRUCT(meta=(DisplayName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct RIGVM_API FRigVMFunction_MathTransformSelectBool : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformSelectBool()
	{
		Condition = false;
		IfTrue = IfFalse = Result = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformRotateVector : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformRotateVector()
	{
		Transform = FTransform::Identity;
		Vector = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformTransformVector : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformTransformVector()
	{
		Transform = FTransform::Identity;
		Location = Result = FVector::ZeroVector;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformFromSRT : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformFromSRT()
	{
		Location = FVector::ZeroVector;
		Rotation = FVector::ZeroVector;
		RotationOrder = EEulerRotationOrder::XYZ;
		Scale = FVector::OneVector;
		Transform = FTransform::Identity;
		EulerTransform = FEulerTransform::Identity;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformArrayToSRT : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformArrayToSRT()
	{
	}

	RIGVM_METHOD()
		virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathTransformClampSpatially : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformClampSpatially()
	{
		Value = Result = FTransform::Identity;
		Axis = EAxis::X;
		Type = ERigVMClampSpatialMode::Plane;
		Minimum = 0.f;
		Maximum = 100.f;
		Space = FTransform::Identity;
		bDrawDebug = false;
		DebugColor = FLinearColor::Red;
		DebugThickness = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FTransform Value;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EAxis::Type> Axis;

	UPROPERTY(meta = (Input))
	TEnumAsByte<ERigVMClampSpatialMode::Type> Type;

	// The minimum allowed distance at which a collision occurs. 
	// Note: For capsule this represents the radius.
	// Disable by setting to 0.0.
	UPROPERTY(meta = (Input))
	float Minimum;

	// This maximum allowed distance.
	// A collision will occur towards the center at this wall.
	// Note: For capsule this represents the length.
	// Disable by setting to 0.0.
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
struct RIGVM_API FRigVMFunction_MathTransformMirrorTransform : public FRigVMFunction_MathTransformBase
{
	GENERATED_BODY()

	FRigVMFunction_MathTransformMirrorTransform()
	{
		Value = Result = FTransform::Identity;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
