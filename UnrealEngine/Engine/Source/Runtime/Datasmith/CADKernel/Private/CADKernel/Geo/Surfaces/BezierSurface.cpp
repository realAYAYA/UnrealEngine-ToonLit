// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Surfaces/BezierSurface.h"

#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Math/BSpline.h"

namespace UE::CADKernel
{

void FBezierSurface::EvaluatePoint(const FPoint2D& InPoint2D, FSurfacicPoint& OutPoint3D, int32 InDerivativeOrder) const
{
	TArray<FPoint> VCurvePoles;
	TArray<FPoint> VCurveUGradient;
	TArray<FPoint> VCurveULaplacian;

	VCurvePoles.SetNum(VPoleNum);

	OutPoint3D.DerivativeOrder = InDerivativeOrder;
	
	if (InDerivativeOrder > 0)
	{
		VCurveUGradient.Init(FPoint::ZeroPoint, VPoleNum);
	}

	if (InDerivativeOrder > 1)
	{
		VCurveULaplacian.SetNum(VPoleNum);
	}

	{
		double Coordinate = InPoint2D.U;

		TArray<FPoint> UAuxiliaryPoles;
		UAuxiliaryPoles.SetNum(UPoleNum);

		TArray<FPoint> UAuxiliaryGradient;
		TArray<FPoint> UAuxiliaryLaplacian;

		// For each iso V Curve compute Point, Gradient and Laplacian at U coordinate
		for (int32 Vndex = 0, PoleIndex = 0; Vndex < VPoleNum; Vndex++, PoleIndex += UPoleNum)
		{
			UAuxiliaryPoles.Empty(UPoleNum);
			UAuxiliaryPoles.Append(Poles.GetData() + PoleIndex, UPoleNum);

			if (InDerivativeOrder > 0)
			{
				UAuxiliaryGradient.Init(FPoint::ZeroPoint, UPoleNum);
			}
			if (InDerivativeOrder > 1)
			{
				UAuxiliaryLaplacian.Init(FPoint::ZeroPoint, UPoleNum);
			}

			// Compute Point, Gradient and Laplacian at U coordinate with De Casteljau's algorithm, 
			for (int32 Undex = UPoleNum - 2; Undex >= 0; Undex--)
			{
				for (int32 Index = 0; Index <= Undex; Index++)
				{
					const FPoint PointI = UAuxiliaryPoles[Index];
					const FPoint& PointB = UAuxiliaryPoles[Index + 1];
					const FPoint VectorIB = PointB - PointI;

					UAuxiliaryPoles[Index] = PointI + VectorIB * Coordinate;
					if (InDerivativeOrder > 0)
					{
						const FPoint GradientI = UAuxiliaryGradient[Index];
						const FPoint& GradientB = UAuxiliaryGradient[Index + 1];
						const FPoint VectorGradientIB = GradientB - GradientI;

						UAuxiliaryGradient[Index] = GradientI + VectorGradientIB * Coordinate + VectorIB;
						if (InDerivativeOrder > 1)
						{
							//UAuxiliaryLaplacian[Index] = UAuxiliaryLaplacian[Index] + (UAuxiliaryLaplacian[Index + 1] - UAuxiliaryLaplacian[Index]) * Coordinate + 2.0 * VectorGradientIB;
							UAuxiliaryLaplacian[Index] = PolylineTools::LinearInterpolation(UAuxiliaryLaplacian, Index, Coordinate) + 2.0 * VectorGradientIB;
						}
					}
				}
			}

			// Point, Gradient and Laplacian of the iso v curve are saved to defined the VCurve
			VCurvePoles[Vndex] = UAuxiliaryPoles[0];
			if (InDerivativeOrder > 0)
			{
				VCurveUGradient[Vndex] = UAuxiliaryGradient[0];

				if (InDerivativeOrder > 1)
				{
					VCurveULaplacian[Vndex] = UAuxiliaryLaplacian[0];
				}
			}
		}
	}

	TArray<FPoint> VAuxiliaryGradient;
	TArray<FPoint> UVAuxiliaryLaplacian;
	TArray<FPoint> VVAuxiliaryLaplacian;
	if (InDerivativeOrder > 0)
	{
		VAuxiliaryGradient.Init(FPoint::ZeroPoint, VPoleNum);
		if (InDerivativeOrder > 1)
		{
			VVAuxiliaryLaplacian.Init(FPoint::ZeroPoint, VPoleNum);
			UVAuxiliaryLaplacian.Init(FPoint::ZeroPoint, VPoleNum);
		}
	}

	double Coordinate = InPoint2D.V;

	// Compute Point, Gradient and Laplacian at V coordinate with De Casteljau's algorithm,
	for (int32 Vndex = VPoleNum - 2; Vndex >= 0; Vndex--)
	{
		for (int32 Index = 0; Index <= Vndex; Index++)
		{
			const FPoint PointI = VCurvePoles[Index];
			const FPoint& PointB = VCurvePoles[Index + 1];
			const FPoint VectorIB = PointB - PointI;
			VCurvePoles[Index] = PointI + VectorIB * Coordinate;

			if (InDerivativeOrder > 0) 
			{
				const FPoint UGradientI = VCurveUGradient[Index];
				const FPoint& UGradientB = VCurveUGradient[Index + 1];
				const FPoint VectorUGradientIB = UGradientB - UGradientI;

				VCurveUGradient[Index] = UGradientI + VectorUGradientIB * Coordinate;

				const FPoint VGradientI = VAuxiliaryGradient[Index]; 
				const FPoint& VGradientB = VAuxiliaryGradient[Index + 1];
				const FPoint VectorVGradientIB = VGradientB - VGradientI;

				VAuxiliaryGradient[Index] = VGradientI + VectorVGradientIB * Coordinate + VectorIB;

				if (InDerivativeOrder > 1)
				{
					//UVAuxiliaryLaplacian[Index] = UVAuxiliaryLaplacian[Index] + (UVAuxiliaryLaplacian[Index + 1] - UVAuxiliaryLaplacian[Index]) * Coordinate + VectorUGradientIB;
					UVAuxiliaryLaplacian[Index] = PolylineTools::LinearInterpolation(UVAuxiliaryLaplacian, Index, Coordinate) + VectorUGradientIB;

					//VCurveULaplacian[Index] = VCurveULaplacian[Index] + (VCurveULaplacian[Index + 1] - VCurveULaplacian[Index]) * Coordinate;
					VCurveULaplacian[Index] = PolylineTools::LinearInterpolation(VCurveULaplacian, Index, Coordinate);

					//VVAuxiliaryLaplacian[Index] = VVAuxiliaryLaplacian[Index] + (VVAuxiliaryLaplacian[Index + 1] - VVAuxiliaryLaplacian[Index]) * Coordinate + 2.0 * VectorVGradientIB;
					VVAuxiliaryLaplacian[Index] = PolylineTools::LinearInterpolation(VVAuxiliaryLaplacian, Index, Coordinate) + 2.0 * VectorVGradientIB;;
				}
			}
		}
	}

	OutPoint3D.Point = VCurvePoles[0];

	if (InDerivativeOrder > 0) 
	{
		OutPoint3D.GradientU = VCurveUGradient[0];
		OutPoint3D.GradientV = VAuxiliaryGradient[0];
	}

	if (InDerivativeOrder > 1) 
	{
		OutPoint3D.LaplacianU = VCurveULaplacian[0];
		OutPoint3D.LaplacianV = VVAuxiliaryLaplacian[0];
		OutPoint3D.LaplacianUV = UVAuxiliaryLaplacian[0];
	}
}

void FBezierSurface::Presample(const FSurfacicBoundary& InBoundaries, FCoordinateGrid& Coordinates)
{
	ensureCADKernel(false);
}

TSharedPtr<FEntityGeom> FBezierSurface::ApplyMatrix(const FMatrixH& InMatrix) const
{
	TArray<FPoint> TransformedPoles;
	TransformedPoles.Reserve(Poles.Num());
	for (const FPoint& Pole : Poles) 
	{
		TransformedPoles.Emplace(InMatrix.Multiply(Pole));
	}
	return FEntity::MakeShared<FBezierSurface>(Tolerance3D, UDegre, VDegre, TransformedPoles);
}

#ifdef CADKERNEL_DEV
FInfoEntity& FBezierSurface::GetInfo(FInfoEntity& Info) const
{
	return FSurface::GetInfo(Info).Add(TEXT("degre U"), UDegre)
		.Add(TEXT("degre V"), VDegre)
		.Add(TEXT("poles"), Poles);
}
#endif

}