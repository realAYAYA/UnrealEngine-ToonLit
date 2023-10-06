// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelingOperators.h"

#include "TriangulateCurvesOp.generated.h"

class USplineComponent;

UENUM()
enum class EFlattenCurveMethod : uint8
{
	DoNotFlatten,
	ToBestFitPlane,
	AlongX,
	AlongY,
	AlongZ
};

UENUM()
enum class ECombineCurvesMethod : uint8
{
	LeaveSeparate,
	Union,
	Intersect,
	Difference,
	ExclusiveOr
};

UENUM()
enum class EOffsetClosedCurvesMethod : uint8
{
	DoNotOffset,
	OffsetOuterSide,
	OffsetBothSides
};

UENUM()
enum class EOffsetOpenCurvesMethod : uint8
{
	TreatAsClosed,
	Offset
};

UENUM()
enum class EOffsetJoinMethod : uint8
{
	Square,
	Miter,
	Round
};

UENUM()
enum class EOpenCurveEndShapes : uint8
{
	Square,
	Round,
	Butt
};

namespace UE {
namespace Geometry {

class FDynamicMesh3;

/**
 * FTriangulateCurvesOp triangulates polygons/paths generated from USplineComponent inputs.
 */
class MODELINGOPERATORS_API FTriangulateCurvesOp : public FDynamicMeshOperator
{
public:
	virtual ~FTriangulateCurvesOp() {}

	//
	// Inputs
	//

	void AddSpline(USplineComponent* Spline, double ErrorTolerance);

	//
	// Parameters
	//

	// TODO: Add more options, e.g. for flattening the input curves, setting triangulation method, and additional processing of the curves (e.g., union, offset, etc)

	// Scaling applied to the default UV values
	double UVScaleFactor = 1.0;

	// If > 0, thicken the result mesh to make a solid
	double Thickness = 0.0;

	EFlattenCurveMethod FlattenMethod = EFlattenCurveMethod::DoNotFlatten;

	// Note: Combining and offsetting curves only works when curves are flattened; curves will be left separate and non-offset if FlattenMethod is DoNotFlatten

	ECombineCurvesMethod CombineMethod = ECombineCurvesMethod::LeaveSeparate;

	EOffsetClosedCurvesMethod OffsetClosedMethod = EOffsetClosedCurvesMethod::DoNotOffset;
	EOffsetOpenCurvesMethod OffsetOpenMethod = EOffsetOpenCurvesMethod::TreatAsClosed;
	EOffsetJoinMethod OffsetJoinMethod = EOffsetJoinMethod::Square;
	EOpenCurveEndShapes OpenEndShape = EOpenCurveEndShapes::Square;
	double MiterLimit = 1.0;
	double CurveOffset = 1.0;

	bool bFlipResult = false;

	//
	// FDynamicMeshOperator interface 
	//
	virtual void CalculateResult(FProgressCancel* Progress) override;

private:

	struct FCurvePath
	{
		bool bClosed = false;
		TArray<FVector3d> Vertices;
	};

	// Paths for all splines, in world space
	TArray<FCurvePath> Paths;

	// Local to World transform of the first path
	FTransform FirstPathTransform;

	void ApplyThickness(double UseUVScaleFactor);
};

}} // end UE::Geometry
