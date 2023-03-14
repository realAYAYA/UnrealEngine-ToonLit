// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "ControlRigDefines.h"
#include "Units/RigUnit.h"
#include "ControlRig.h"
#include "ControlRigSplineTypes.h"
#include "ControlRig/Private/Units/Highlevel/Hierarchy/RigUnit_FitChainToCurve.h"
#include "ControlRigSplineUnits.generated.h"

USTRUCT(meta = (Abstract, NodeColor = "0.3 0.1 0.1"))
struct CONTROLRIGSPLINE_API FRigUnit_ControlRigSplineBase : public FRigUnit
{
	GENERATED_BODY()
};

/*
 * Creates a Spline curve from an array of positions
 */
USTRUCT(meta = (DisplayName = "Spline From Points", Keywords="Spline From Positions", Category = "Splines", Varying, NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_ControlRigSplineFromPoints : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_ControlRigSplineFromPoints()
	{
		SplineMode = ESplineType::Hermite;
		SamplesPerSegment = 16;
		Compression = 0.f;
		Stretch = 0.f;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	TArray<FVector> Points;

	UPROPERTY(meta = (Input))
	ESplineType SplineMode;

	UPROPERTY(meta = (Input))
	int32 SamplesPerSegment;

	UPROPERTY(meta = (Input))
	float Compression;

	UPROPERTY(meta = (Input))
	float Stretch;

	UPROPERTY(meta = (Output))
	FControlRigSpline Spline;
};

/*
 * Set the points of a spline, given a spline and an array of positions
 */
USTRUCT(meta = (DisplayName = "Set Spline Points", Category = "Splines", Varying, NodeColor = "0.0 0.36470600962638855 1.0"))
struct CONTROLRIGSPLINE_API FRigUnit_SetSplinePoints : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_SetSplinePoints()
	{
		
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	TArray<FVector> Points;

	UPROPERTY(meta = (Input, Output))
	FControlRigSpline Spline;
};

/*
 * Retrieves the position from a given Spline and U value
 */
USTRUCT(meta = (DisplayName = "Position From Spline", Keywords="Point From Spline", Category = "Splines", NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_PositionFromControlRigSpline : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_PositionFromControlRigSpline()
	{
		U = 0.f;
		Position = FVector::ZeroVector;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	float U;

	UPROPERTY(meta = (Output))
	FVector Position;
};

/*
 * Retrieves the transform from a given Spline and U value based on the given Up Vector and Roll
 */
USTRUCT(meta = (DisplayName = "Transform From Spline", Category = "Splines", NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_TransformFromControlRigSpline : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_TransformFromControlRigSpline()
	{
		UpVector = FVector::UpVector;
		Roll = 0.f;
		U = 0.f;
		Transform = FTransform::Identity;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	FVector UpVector;

	UPROPERTY(meta = (Input))
	float Roll;

	UPROPERTY(meta = (Input))
	float U;

	UPROPERTY(meta = (Output))
	FTransform Transform;
};

/*
 * Retrieves the tangent from a given Spline and U value
 */
USTRUCT(meta = (DisplayName = "Tangent From Spline", Category = "Splines", NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_TangentFromControlRigSpline : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_TangentFromControlRigSpline()
	{
		U = 0.f;
		Tangent = FVector::ZeroVector;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	float U;

	UPROPERTY(meta = (Output))
	FVector Tangent;
};

/*
 * Draws the given spline in the viewport
 */
USTRUCT(meta = (DisplayName = "Draw Spline", Category = "Splines", NodeColor = "0.83077 0.846873 0.049707"))
struct CONTROLRIGSPLINE_API FRigUnit_DrawControlRigSpline : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_DrawControlRigSpline()
	{
		Color = FLinearColor::Red;
		Thickness = 1.f;
		Detail = 16.f;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	FLinearColor Color;

	UPROPERTY(meta = (Input))
	float Thickness;

	UPROPERTY(meta = (Input))
	int32 Detail;
};

/*
 * Retrieves the length from a given Splin
 */
USTRUCT(meta = (DisplayName = "Get Length Of Spline", Category = "Splines", NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_GetLengthControlRigSpline : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetLengthControlRigSpline()
	{
		Length = 0.f;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Output))
	float Length;
};

/**
 * Fits a given chain to a spline curve.
 * Additionally provides rotational control matching the features of the Distribute Rotation node.
 */
USTRUCT(meta=(DisplayName="Fit Chain on Spline Curve", Category="Hierarchy", Keywords="Fit,Resample,Spline", Deprecated = "5.0"))
struct CONTROLRIGSPLINE_API FRigUnit_FitChainToSplineCurve : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_FitChainToSplineCurve()
	{
		Alignment = EControlRigCurveAlignment::Stretched;
		Minimum = 0.f;
		Maximum = 1.f;
		SamplingPrecision = 12;
		PrimaryAxis = FVector(1.f, 0.f, 0.f);
		SecondaryAxis = FVector(0.f, 0.f, 0.f);
		PoleVectorPosition = FVector::ZeroVector;
		RotationEaseType = EControlRigAnimEasingType::Linear;
		Weight = 1.f;
		bPropagateToChildren = true;
		DebugSettings = FRigUnit_FitChainToCurve_DebugSettings();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The items to align
	 */
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Items;

	/** 
	 * The curve to align to
	 */
	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	/** 
	 * Specifies how to align the chain on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	EControlRigCurveAlignment Alignment;

	/** 
	 * The minimum U value to use on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	float Minimum;

	/** 
	 * The maximum U value to use on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	float Maximum;

	/**
	 * The number of samples to use on the curve. Clamped at 64.
	 */
	UPROPERTY(meta = (Input, Constant))
	int32 SamplingPrecision;

	/**
	 * The major axis being aligned - along the bone
	 */
	UPROPERTY(meta = (Input))
	FVector PrimaryAxis;

	/**
	 * The minor axis being aligned - towards the pole vector.
	 * You can use (0.0, 0.0, 0.0) to disable it.
	 */
	UPROPERTY(meta = (Input))
	FVector SecondaryAxis;

	/**
	 * The the position of the pole vector used for aligning the secondary axis.
	 * Only has an effect if the secondary axis is set.
	 */
	UPROPERTY(meta = (Input))
	FVector PoleVectorPosition;

	/** 
	 * The list of rotations to be applied along the curve
	 */
	UPROPERTY(meta = (Input))
	TArray<FRigUnit_FitChainToCurve_Rotation> Rotations;

	/**
	 * The easing to use between to rotations.
	 */
	UPROPERTY(meta = (Input, Constant))
	EControlRigAnimEasingType RotationEaseType;

	/**
	 * The weight of the solver - how much the rotation should be applied
	 */
	UPROPERTY(meta = (Input))
	float Weight;

	/**
	 * If set to true all of the global transforms of the children
	 * of this bone will be recalculated based on their local transforms.
	 * Note: This is computationally more expensive than turning it off.
	 */
	UPROPERTY(meta = (Input, Constant))
	bool bPropagateToChildren;

	UPROPERTY(meta = (Input, DetailsOnly))
	FRigUnit_FitChainToCurve_DebugSettings DebugSettings;

	UPROPERTY(transient)
	FRigUnit_FitChainToCurve_WorkData WorkData;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Fits a given chain to a spline curve.
 * Additionally provides rotational control matching the features of the Distribute Rotation node.
 */
USTRUCT(meta=(DisplayName="Fit Chain on Spline Curve", Category="Splines", Keywords="Fit,Resample,Spline"))
struct CONTROLRIGSPLINE_API FRigUnit_FitChainToSplineCurveItemArray : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_FitChainToSplineCurveItemArray()
	{
		Alignment = EControlRigCurveAlignment::Stretched;
		Minimum = 0.f;
		Maximum = 1.f;
		SamplingPrecision = 12;
		PrimaryAxis = FVector(1.f, 0.f, 0.f);
		SecondaryAxis = FVector(0.f, 0.f, 0.f);
		PoleVectorPosition = FVector::ZeroVector;
		RotationEaseType = EControlRigAnimEasingType::Linear;
		Weight = 1.f;
		bPropagateToChildren = true;
		DebugSettings = FRigUnit_FitChainToCurve_DebugSettings();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The items to align
	 */
	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	/** 
	 * The curve to align to
	 */
	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	/** 
	 * Specifies how to align the chain on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	EControlRigCurveAlignment Alignment;

	/** 
	 * The minimum U value to use on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	float Minimum;

	/** 
	 * The maximum U value to use on the curve
	 */
	UPROPERTY(meta = (Input, Constant))
	float Maximum;

	/**
	 * The number of samples to use on the curve. Clamped at 64.
	 */
	UPROPERTY(meta = (Input, Constant))
	int32 SamplingPrecision;

	/**
	 * The major axis being aligned - along the bone
	 */
	UPROPERTY(meta = (Input))
	FVector PrimaryAxis;

	/**
	 * The minor axis being aligned - towards the pole vector.
	 * You can use (0.0, 0.0, 0.0) to disable it.
	 */
	UPROPERTY(meta = (Input))
	FVector SecondaryAxis;

	/**
	 * The the position of the pole vector used for aligning the secondary axis.
	 * Only has an effect if the secondary axis is set.
	 */
	UPROPERTY(meta = (Input))
	FVector PoleVectorPosition;

	/** 
	 * The list of rotations to be applied along the curve
	 */
	UPROPERTY(meta = (Input))
	TArray<FRigUnit_FitChainToCurve_Rotation> Rotations;

	/**
	 * The easing to use between to rotations.
	 */
	UPROPERTY(meta = (Input, Constant))
	EControlRigAnimEasingType RotationEaseType;

	/**
	 * The weight of the solver - how much the rotation should be applied
	 */
	UPROPERTY(meta = (Input))
	float Weight;

	/**
	 * If set to true all of the global transforms of the children
	 * of this bone will be recalculated based on their local transforms.
	 * Note: This is computationally more expensive than turning it off.
	 */
	UPROPERTY(meta = (Input, Constant))
	bool bPropagateToChildren;

	UPROPERTY(meta = (Input, DetailsOnly))
	FRigUnit_FitChainToCurve_DebugSettings DebugSettings;

	UPROPERTY(transient)
	FRigUnit_FitChainToCurve_WorkData WorkData;
};

/**
 * Fits a given spline curve to a chain.
 */
USTRUCT(meta=(DisplayName="Fit Spline Curve on Chain", Category="Hierarchy", Keywords="Fit,Resample,Spline", Deprecated = "5.0"))
struct CONTROLRIGSPLINE_API FRigUnit_FitSplineCurveToChain : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_FitSplineCurveToChain()
	{
		
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The items to align to
	 */
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Items;

	/** 
	 * The curve to align
	 */
	UPROPERTY(meta = (Input, Output))
	FControlRigSpline Spline;	

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Fits a given spline curve to a chain.
 */
USTRUCT(meta=(DisplayName="Fit Spline Curve on Chain", Category="Splines", Keywords="Fit,Resample,Spline"))
struct CONTROLRIGSPLINE_API FRigUnit_FitSplineCurveToChainItemArray : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_FitSplineCurveToChainItemArray()
	{
		
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The items to align to
	 */
	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	/** 
	 * The curve to align
	 */
	UPROPERTY(meta = (Input, Output))
	FControlRigSpline Spline;	
};

/*
 * Retrieves the closest U value from a given Spline and a position
 */
USTRUCT(meta = (DisplayName = "Closest Parameter From Spline", Category = "Splines", NodeColor="0.737911 0.099899 0.099899"))
struct CONTROLRIGSPLINE_API FRigUnit_ClosestParameterFromControlRigSpline : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_ClosestParameterFromControlRigSpline()
	{
		Position = FVector::ZeroVector;
		U = 0.f;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	FVector Position;

	UPROPERTY(meta = (Output))
	float U;
	
};

/*
 * Returns the U parameter of a spline given a length percentage (0.0 - 1.0)
 */
USTRUCT(meta = (DisplayName = "Parameter At Length Percentage", Category = "Splines"))
struct CONTROLRIGSPLINE_API FRigUnit_ParameterAtPercentage : public FRigUnit_ControlRigSplineBase
{
	GENERATED_BODY()

	FRigUnit_ParameterAtPercentage()
	{
		Percentage = 0.f;
		U = 0.f;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FControlRigSpline Spline;

	UPROPERTY(meta = (Input))
	float Percentage;

	UPROPERTY(meta = (Output))
	float U;
	
};

