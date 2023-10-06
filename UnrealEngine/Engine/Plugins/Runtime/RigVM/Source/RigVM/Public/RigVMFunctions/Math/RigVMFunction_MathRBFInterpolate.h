// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RBF/RBFInterpolator.h"

#include "RigVMFunction_MathRBFInterpolate.generated.h"

template<typename T> class TRBFInterpolator;
struct RIGVM_API FRigVMFunction_MathRBFQuatWeightFunctor;
struct RIGVM_API FRigVMFunction_MathRBFVectorWeightFunctor;


/** Function to use for each target falloff */
UENUM()
enum class ERBFKernelType : uint8
{
	Gaussian,
	Exponential,
	Linear,
	Cubic,
	Quintic
};

/** Function to use for computing distance between the input and target 
	quaternions. */
UENUM()
enum class ERBFQuatDistanceType : uint8
{
	Euclidean,
	ArcLength,
	SwingAngle,
	TwistAngle,
};

/** Function to use for computing distance between the input and target 
	quaternions. */
UENUM()
enum class ERBFVectorDistanceType : uint8
{
	Euclidean,
	Manhattan,
	ArcLength
};


USTRUCT()
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatWorkData
{
	GENERATED_BODY()

	// There's no current mechanism for detecting whether an entire input stream is
	// constant or variable in the RigVM. So what we do here is that the firs time
	// around we set up an interpolator 
	TRBFInterpolator<FQuat> Interpolator;
	TArray<FQuat> Targets;
	uint64 Hash = 0;
	bool bAreTargetsConstant = true;
};

USTRUCT()
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorWorkData
{
	GENERATED_BODY()

	TRBFInterpolator<FVector> Interpolator;
	TArray<FVector> Targets;
	uint64 Hash = 0;
	bool bAreTargetsConstant = true;
};


USTRUCT(meta = (Abstract, Category = "Math|RBF Interpolation"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateBase : 
	public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (Abstract, TemplateName="RBF Quaternion", Keywords = "RBF,Interpolate,Quaternion"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatBase :
	public FRigVMFunction_MathRBFInterpolateBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Input = FQuat::Identity;

	UPROPERTY(meta = (Input, Constant))
	ERBFQuatDistanceType DistanceFunction = ERBFQuatDistanceType::ArcLength;

	UPROPERTY(meta = (Input, Constant))
	ERBFKernelType SmoothingFunction = ERBFKernelType::Gaussian;

	UPROPERTY(meta = (Input, Constant))
	float SmoothingAngle = 45.0f;

	UPROPERTY(meta = (Input, Constant))
	bool bNormalizeOutput = false;

	UPROPERTY(meta = (Input, EditCondition = "DistanceFunction == ERBFQuatDistanceType::SwingAngle || DistanceFunction == ERBFQuatDistanceType::TwistAngle"))
	FVector TwistAxis = FVector::ForwardVector;

	UPROPERTY(transient)
	FRigVMFunction_MathRBFInterpolateQuatWorkData WorkData;

protected:
	
	template<typename T>
	static void GetInterpolatedWeights(
		FRigVMFunction_MathRBFInterpolateQuatWorkData& WorkData,
		const TArrayView<const T>& Targets,
		const FQuat& Input,
		ERBFQuatDistanceType DistanceFunction,
		ERBFKernelType SmoothingFunction,
		float SmoothingAngle,
		bool bNormalizeOutput,
		FVector TwistAxis,
		TArray<float>& Weights
	);

	static uint64 HashTargets(const TArrayView<const FQuat>& Targets);
};

USTRUCT(meta = (Abstract, TemplateName="RBF Vector", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorBase :
	public FRigVMFunction_MathRBFInterpolateBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Input = FVector::ZeroVector;

	UPROPERTY(meta = (Input, Constant))
	ERBFVectorDistanceType DistanceFunction = ERBFVectorDistanceType::Euclidean;

	UPROPERTY(meta = (Input, Constant))
	ERBFKernelType SmoothingFunction = ERBFKernelType::Gaussian;

	UPROPERTY(meta = (Input, Constant))
	float SmoothingRadius = 5.0f;

	UPROPERTY(meta = (Input, Constant))
	bool bNormalizeOutput = false;

	UPROPERTY(transient)
	FRigVMFunction_MathRBFInterpolateVectorWorkData WorkData;

protected:
	template<typename T>
	static void GetInterpolatedWeights(
		FRigVMFunction_MathRBFInterpolateVectorWorkData& WorkData,
		const TArrayView<const T>& Targets,
		const FVector& Input,
		ERBFVectorDistanceType DistanceFunction,
		ERBFKernelType SmoothingFunction,
		float SmoothingRadius,
		bool bNormalizeOutput,
		TArray<float>& Weights
	);

	static uint64 HashTargets(const TArrayView<const FVector>& Targets);
};

// The actual unit implementation declarations.

// Quat -> T

USTRUCT()
struct FMathRBFInterpolateQuatFloat_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Target = FQuat::Identity;

	UPROPERTY(meta = (Input))
	float Value = 0.0f;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Quaternion to Float"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatFloat : 
	public FRigVMFunction_MathRBFInterpolateQuatBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateQuatFloat_Target> Targets;

	UPROPERTY(meta = (Output))
	float Output = 0.0f;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateQuatVector_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Target = FQuat::Identity;

	UPROPERTY(meta = (Input))
	FVector Value = FVector::ZeroVector;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Quaternion to Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatVector :
	public FRigVMFunction_MathRBFInterpolateQuatBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateQuatVector_Target> Targets;

	UPROPERTY(meta = (Output))
	FVector Output = FVector::ZeroVector;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateQuatColor_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Target = FQuat::Identity;

	UPROPERTY(meta = (Input))
	FLinearColor Value = FLinearColor::Transparent;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Quaternion to Color"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatColor :
	public FRigVMFunction_MathRBFInterpolateQuatBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateQuatColor_Target> Targets;

	UPROPERTY(meta = (Output))
	FLinearColor Output = FLinearColor::Transparent;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateQuatQuat_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Target = FQuat::Identity;

	UPROPERTY(meta = (Input))
	FQuat Value = FQuat::Identity;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Quaternion to Quaternion"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatQuat :
	public FRigVMFunction_MathRBFInterpolateQuatBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateQuatQuat_Target> Targets;

	UPROPERTY(meta = (Output))
	FQuat Output = FQuat::Identity;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateQuatXform_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FQuat Target = FQuat::Identity;

	UPROPERTY(meta = (Input))
	FTransform Value = FTransform::Identity;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Quaternion to Transform"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateQuatXform :
	public FRigVMFunction_MathRBFInterpolateQuatBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateQuatXform_Target> Targets;

	UPROPERTY(meta = (Output))
	FTransform Output = FTransform::Identity;

	RIGVM_METHOD()
	void Execute() override;
};


/// Vector->T

USTRUCT()
struct FMathRBFInterpolateVectorFloat_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Target = FVector::ZeroVector;

	UPROPERTY(meta = (Input))
	float Value = 0.0f;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Vector to Float", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorFloat :
	public FRigVMFunction_MathRBFInterpolateVectorBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateVectorFloat_Target> Targets;

	UPROPERTY(meta = (Output))
	float Output = 0.0f;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateVectorVector_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Target = FVector::ZeroVector;

	UPROPERTY(meta = (Input))
	FVector Value = FVector::ZeroVector;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Vector to Vector", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorVector :
	public FRigVMFunction_MathRBFInterpolateVectorBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateVectorVector_Target> Targets;

	UPROPERTY(meta = (Output))
	FVector Output = FVector::ZeroVector;

	RIGVM_METHOD()
	void Execute() override;
};


USTRUCT()
struct FMathRBFInterpolateVectorColor_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Target = FVector::ZeroVector;

	UPROPERTY(meta = (Input))
	FLinearColor Value = FLinearColor::Transparent;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Vector to Color", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorColor :
	public FRigVMFunction_MathRBFInterpolateVectorBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateVectorColor_Target> Targets;

	UPROPERTY(meta = (Output))
	FLinearColor Output = FLinearColor::Transparent;

	RIGVM_METHOD()
	void Execute() override;
};



USTRUCT()
struct FMathRBFInterpolateVectorQuat_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Target = FVector::ZeroVector;

	UPROPERTY(meta = (Input))
	FQuat Value = FQuat::Identity;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Vector to Quat", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorQuat :
	public FRigVMFunction_MathRBFInterpolateVectorBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateVectorQuat_Target> Targets;

	UPROPERTY(meta = (Output))
	FQuat Output = FQuat::Identity;

	RIGVM_METHOD()
	void Execute() override;
};

USTRUCT()
struct FMathRBFInterpolateVectorXform_Target
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	FVector Target = FVector::ZeroVector;

	UPROPERTY(meta = (Input))
	FTransform Value = FTransform::Identity;
};

USTRUCT(meta = (Abstract, DisplayName = "RBF Vector to Transform", Keywords = "RBF,Interpolate,Vector"))
struct RIGVM_API FRigVMFunction_MathRBFInterpolateVectorXform :
	public FRigVMFunction_MathRBFInterpolateVectorBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TArray<FMathRBFInterpolateVectorXform_Target> Targets;

	UPROPERTY(meta = (Output))
	FTransform Output = FTransform::Identity;

	RIGVM_METHOD()
	void Execute() override;
};
