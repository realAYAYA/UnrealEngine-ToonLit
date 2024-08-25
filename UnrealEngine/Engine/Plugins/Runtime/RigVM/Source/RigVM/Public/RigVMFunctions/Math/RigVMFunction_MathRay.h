// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Ray.h"
#include "Math/Plane.h"
#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathRay.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Ray", MenuDescSuffix="(Ray)"))
struct RIGVM_API FRigVMFunction_MathRayBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

/**
 * Returns the closest point intersection of a ray with another ray
 */
USTRUCT(meta = (DisplayName = "Intersect Ray", Keywords = "Closest,Ray,Cross"))
struct RIGVM_API FRigVMFunction_MathRayIntersectRay : public FRigVMFunction_MathRayBase
{
	GENERATED_BODY()

	FRigVMFunction_MathRayIntersectRay()
	{
		A = B = FRay();
		Result = FVector::ZeroVector;
		Distance = RatioA = RatioB = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRay A;

	UPROPERTY(meta = (Input))
	FRay B;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY(meta = (Output))
	float Distance;

	UPROPERTY(meta = (Output))
	float RatioA;

	UPROPERTY(meta = (Output))
	float RatioB;
};

/**
 * Returns the closest point intersection of a ray with a plane
 */
USTRUCT(meta = (DisplayName = "Intersect Plane", Keywords = "Closest,Ray,Cross"))
struct RIGVM_API FRigVMFunction_MathRayIntersectPlane : public FRigVMFunction_MathRayBase
{
	GENERATED_BODY()

	FRigVMFunction_MathRayIntersectPlane()
	{
		Ray = FRay();
		PlanePoint = FVector::ZeroVector;
		PlaneNormal = FVector::UpVector;
		Result = FVector::ZeroVector;
		Distance = Ratio = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRay Ray;

	UPROPERTY(meta = (Input))
	FVector PlanePoint;

	UPROPERTY(meta = (Input))
	FVector PlaneNormal;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY(meta = (Output))
	float Distance;

	UPROPERTY(meta = (Output))
	float Ratio;
};

/**
 * Returns the position on a ray
 */
USTRUCT(meta=(DisplayName="GetAt", Keywords="Ratio,Percentage"))
struct RIGVM_API FRigVMFunction_MathRayGetAt : public FRigVMFunction_MathRayBase
{
	GENERATED_BODY()

	FRigVMFunction_MathRayGetAt()
	{
		Ray = FRay();
		Ratio = 0.f;
		Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRay Ray;

	UPROPERTY(meta = (Input))
	float Ratio;

	UPROPERTY(meta = (Output))
	FVector Result;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Transform Ray", Keywords="Multiply,Project"))
struct RIGVM_API FRigVMFunction_MathRayTransform : public FRigVMFunction_MathRayBase
{
	GENERATED_BODY()

	FRigVMFunction_MathRayTransform()
	{
		Ray = Result = FRay();
		Transform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FRay Ray;

	UPROPERTY(meta = (Input))
	FTransform Transform;

	UPROPERTY(meta = (Output))
	FRay Result;
};
