// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Math/Aabb.h"
#include "CADKernel/Math/MathConst.h"
#include "CADKernel/Math/MatrixH.h"
#include "CADKernel/Math/Point.h"

namespace UE::CADKernel
{
enum EPolygonSide : uint8
{
	Side01 = 0,
	Side12,
	Side20,
	Side23,
	Side30,
};

namespace IntersectionTool
{
CADKERNEL_API void SetTolerance(const double Tolerance);
}


/**
 * https://en.wikipedia.org/wiki/Circumscribed_circle#Cartesian_coordinates_2
 * With A = (0, 0)
 */
CADKERNEL_API inline FPoint2D ComputeCircumCircleCenter(const FPoint2D& InPoint0, const FPoint2D& InPoint1, const FPoint2D& InPoint2)
{
	FPoint2D Segment_P0_P1 = InPoint1 - InPoint0;
	FPoint2D Segment_P0_P2 = InPoint2 - InPoint0;

	// D = 2(BuCv - BvCu)
	double D = 2. * Segment_P0_P1 ^ Segment_P0_P2;
	if (FMath::IsNearlyZero(D, SMALL_NUMBER_SQUARE))
	{
		return FPoint2D::ZeroPoint;
	}

	// CenterU  = 1/D * (Cv.|B|.|B| - By.|C|.|C|) = 1/D * CBv ^ SquareNorms 
	// CenterV  = 1/D * (Bu.|B|.|B| - Cu.|C|.|C|) = -1/D * SquareNorms ^ CBu
	double SquareNormB = Segment_P0_P1.SquareLength();
	double SquareNormC = Segment_P0_P2.SquareLength();
	double CenterU = (SquareNormB * Segment_P0_P2.V - SquareNormC * Segment_P0_P1.V) / D;
	double CenterV = (SquareNormC * Segment_P0_P1.U - SquareNormB * Segment_P0_P2.U) / D;

	return FPoint2D(CenterU, CenterV) + InPoint0;
}

CADKERNEL_API inline FPoint ComputeCircumCircleCenter(const FPoint& Point0, const FPoint& Point1, const FPoint& Point2)
{
	FPoint AxisZ = (Point1 - Point0) ^ (Point2 - Point0);
	AxisZ.Normalize();

	FPoint AxisX = Point1 - Point0;
	AxisX.Normalize();
	FPoint AxisY = AxisZ ^ AxisX;

	FMatrixH Matrix(Point0, AxisX, AxisY, AxisZ);
	FMatrixH MatrixInverse = Matrix;
	MatrixInverse.Inverse();

	FPoint2D D2Point1 = MatrixInverse * Point1;
	FPoint2D D2Point2 = MatrixInverse * Point2;

	double D = 2. * D2Point1 ^ D2Point2;
	if (FMath::IsNearlyZero(D, SMALL_NUMBER_SQUARE))
	{
		return FPoint::ZeroPoint;
	}

	double SquareNormB = D2Point1.SquareLength();
	double SquareNormC = D2Point2.SquareLength();

	double CenterU = (SquareNormB * D2Point2.V - SquareNormC * D2Point1.V) / D;
	double CenterV = (SquareNormC * D2Point1.U - SquareNormB * D2Point2.U) / D;

	FPoint Center2D(CenterU, CenterV, 0);
	return Matrix * Center2D;
}


CADKERNEL_API inline FPoint2D ComputeCircumCircleCenterAndSquareRadius(const FPoint2D& InPoint0, const FPoint2D& InPoint1, const FPoint2D& InPoint2, double& OutSquareRadius)
{
	FPoint2D Segment_P0_P1 = InPoint1 - InPoint0;
	FPoint2D Segment_P0_P2 = InPoint2 - InPoint0;

	double D = 2. * Segment_P0_P1 ^ Segment_P0_P2;
	if (FMath::IsNearlyZero(D, SMALL_NUMBER_SQUARE))
	{
		OutSquareRadius = 0;
		return FPoint2D::ZeroPoint;
	}

	double SquareNormB = Segment_P0_P1.SquareLength();
	double SquareNormC = Segment_P0_P2.SquareLength();
	double CenterU = (SquareNormB * Segment_P0_P2.V - SquareNormC * Segment_P0_P1.V) / D;
	double CenterV = (SquareNormC * Segment_P0_P1.U - SquareNormB * Segment_P0_P2.U) / D;

	FPoint2D Center(CenterU, CenterV);
	OutSquareRadius = Center.SquareLength();

	return Center + InPoint0;
}

template<class PointType>
struct CADKERNEL_API TSegment
{
	const PointType& Point0;
	const PointType& Point1;

	TSegment(const PointType& InPoint0, const PointType& InPoint1)
		: Point0(InPoint0)
		, Point1(InPoint1)
	{
	}

	constexpr const PointType& operator[](int32 Index) const
	{
		ensureCADKernel(Index < 2);
		switch (Index)
		{
		case 0:
			return Point0;
		case 1:
			return Point1;
		default:
			return PointType::ZeroPoint;
		}
	}

	double SquareLength() const
	{
		return Point0.SquareDistance(Point1);
	}

	PointType GetVector() const
	{
		return Point1 - Point0;
	}
};

using FSegment2D = TSegment<FPoint2D>;
using FSegment3D = TSegment<FPoint>;

template<class PointType>
struct CADKERNEL_API TTriangle
{
	const PointType& Point0;
	const PointType& Point1;
	const PointType& Point2;

	TTriangle(const PointType& InPoint0, const PointType& InPoint1, const PointType& InPoint2)
		: Point0(InPoint0)
		, Point1(InPoint1)
		, Point2(InPoint2)
	{
	}

	constexpr const PointType& operator[](int32 Index) const
	{
		ensureCADKernel(Index < 3);
		switch (Index)
		{
		case 0:
			return Point0;
		case 1:
			return Point1;
		case 2:
			return Point2;
		default:
			return PointType::ZeroPoint;
		}
	}

	virtual inline PointType ProjectPoint(const PointType& InPoint, FPoint2D& OutCoordinate)
	{
		PointType Segment01 = Point1 - Point0;
		PointType Segment02 = Point2 - Point0;
		double SquareLength01 = Segment01.SquareLength();
		double SquareLength02 = Segment02.SquareLength();
		double Seg01Seg02 = Segment01 * Segment02;
		double Det = SquareLength01 * SquareLength02 - FMath::Square(Seg01Seg02);

		int32 SideIndex;
		// If the 3 points are aligned
		if (FMath::IsNearlyZero(Det))
		{
			double MaxLength = SquareLength01;
			SideIndex = Side01;
			if (SquareLength02 > MaxLength)
			{
				MaxLength = SquareLength02;
				SideIndex = Side20;
			}

			PointType Segment12 = Point2 - Point1;
			if (Segment12.SquareLength() > MaxLength)
			{
				SideIndex = Side12;
			}
		}
		else
		{
			// Resolve
			PointType Segment1Point = InPoint - Point0;
			double Segment1PointSegment01 = Segment1Point * Segment01;
			double Segment1PointSegment02 = Segment1Point * Segment02;

			OutCoordinate.U = ((Segment1PointSegment01 * SquareLength02) - (Segment1PointSegment02 * Seg01Seg02)) / Det;
			OutCoordinate.V = ((Segment1PointSegment02 * SquareLength01) - (Segment1PointSegment02 * Seg01Seg02)) / Det;

			// tester la solution pour choisir parmi 4 possibilites
			if (OutCoordinate.U < 0.0)
			{
				// the project point is on the segment 02
				SideIndex = Side20;
			}
			else if (OutCoordinate.V < 0.0)
			{
				// the project point is on the segment 01
				SideIndex = Side01;
			}
			else if ((OutCoordinate.U + OutCoordinate.V) > 1.0)
			{
				// the project point is on the segment 12
				SideIndex = Side12;
			}
			else {
				// the project point is inside the Segment
				Segment01 = Segment01 * OutCoordinate.U;
				Segment02 = Segment02 * OutCoordinate.V;
				PointType ProjectedPoint = Segment01 + Segment02;
				ProjectedPoint = ProjectedPoint + Point0;
				return ProjectedPoint;
			}
		}

		// projects the point on the nearest side
		PointType ProjectedPoint;
		double SegmentCoordinate;
		switch (SideIndex)
		{
		case Side01:
			ProjectedPoint = ProjectPointOnSegment(InPoint, Point0, Point1, SegmentCoordinate);
			OutCoordinate.U = SegmentCoordinate;
			OutCoordinate.V = 0.0;
			break;
		case Side20:
			ProjectedPoint = ProjectPointOnSegment(InPoint, Point0, Point2, SegmentCoordinate);
			OutCoordinate.U = 0.0;
			OutCoordinate.V = SegmentCoordinate;
			break;
		case Side12:
			ProjectedPoint = ProjectPointOnSegment(InPoint, Point1, Point2, SegmentCoordinate);
			OutCoordinate.U = 1.0 - SegmentCoordinate;
			OutCoordinate.V = SegmentCoordinate;
			break;
		}
		return ProjectedPoint;
	}

	virtual PointType CircumCircleCenter() const
	{
		return ComputeCircumCircleCenter(Point0, Point1, Point2);
	}
};

struct CADKERNEL_API FTriangle : public TTriangle<FPoint>
{
	FTriangle(const FPoint& InPoint0, const FPoint& InPoint1, const FPoint& InPoint2)
		: TTriangle<FPoint>(InPoint0, InPoint1, InPoint2)
	{
	}

	virtual FPoint ComputeNormal() const
	{
		FPoint Normal = (Point1 - Point0) ^ (Point2 - Point0);
		Normal.Normalize();
		return Normal;
	}
};

struct CADKERNEL_API FTriangle2D : public TTriangle<FPoint2D>
{
	FTriangle2D(const FPoint2D& InPoint0, const FPoint2D& InPoint1, const FPoint2D& InPoint2)
		: TTriangle<FPoint2D>(InPoint0, InPoint1, InPoint2)
	{
	}

	FPoint2D CircumCircleCenterWithSquareRadius(double& SquareRadius) const
	{
		return ComputeCircumCircleCenterAndSquareRadius(this->Point0, this->Point1, this->Point2, SquareRadius);
	}
};


template<class PointType>
struct CADKERNEL_API TQuadrangle
{
	const PointType& Point0;
	const PointType& Point1;
	const PointType& Point2;
	const PointType& Point3;

	TQuadrangle(const PointType& InPoint0, const PointType& InPoint1, const PointType& InPoint2, const PointType& InPoint3)
		: Point0(InPoint0)
		, Point1(InPoint1)
		, Point2(InPoint2)
		, Point3(InPoint3)
	{
	}

	constexpr const PointType& operator[](int32 Index) const
	{
		ensureCADKernel(Index < 4);
		switch (Index)
		{
		case 0:
			return Point0;
		case 1:
			return Point1;
		case 2:
			return Point2;
		case 3:
			return Point3;
		default:
			return PointType::ZeroPoint;
		}
	}

	inline FPoint ComputeNormal(const TQuadrangle<FPoint>& InQuadrangle) const
	{
		FPoint Normal = (InQuadrangle[1] - InQuadrangle[0]) ^ (InQuadrangle[2] - InQuadrangle[0]);
		Normal += (InQuadrangle[2] - InQuadrangle[0]) ^ (InQuadrangle[3] - InQuadrangle[0]);
		Normal += (InQuadrangle[1] - InQuadrangle[0]) ^ (InQuadrangle[3] - InQuadrangle[0]);
		Normal += (InQuadrangle[1] - InQuadrangle[3]) ^ (InQuadrangle[2] - InQuadrangle[3]);
		Normal.Normalize();
		return Normal;
	}

	inline PointType  ProjectPoint(const PointType& InPoint, FPoint2D& OutCoordinate)
	{
		FPoint2D Coordinate013;
		TTriangle<PointType> Triangle013(Point0, Point1, Point3);
		PointType Projection013 = Triangle013.ProjectPoint(InPoint, Coordinate013);

		FPoint2D Coordinate231;
		TTriangle<PointType> Triangle231(Point2, Point3, Point1);
		PointType Projection231 = Triangle231.ProjectPoint(InPoint, Coordinate231);

		double DistanceTo013 = Projection013.Distance(InPoint);
		double DistanceTo231 = Projection231.Distance(InPoint);
		if (DistanceTo013 < DistanceTo231)
		{
			OutCoordinate = Coordinate013;
			return Projection013;
		}
		else
		{
			OutCoordinate = { 1.0 , 1.0 };
			OutCoordinate -= Coordinate231;
			return Projection231;
		}
	}

};

inline FPoint ProjectPointOnPlane(const FPoint& Point, const FPoint& Origin, const FPoint& InNormal, double& OutDistance)
{
	FPoint Normal = InNormal;
	ensureCADKernel(!FMath::IsNearlyZero(Normal.Length()));
	Normal.Normalize();

	FPoint OP = Point - Origin;
	OutDistance = OP * Normal;

	return Point - (Normal * OutDistance);
}

/**
 * @return InSegmentA + (InSegmentB - InSegmentA) * InCoordinate
 */
template<class PointType>
inline PointType PointOnSegment(const PointType& InSegmentA, const PointType& InSegmentB, double InCoordinate)
{
	PointType Segment = InSegmentB - InSegmentA;
	return InSegmentA + Segment * InCoordinate;
}

/**
 * @return the distance between the point and the segment. If the projection of the point on the segment
 * is not inside it, return the distance of the point to nearest the segment extremity
 */
template<class PointType>
inline double DistanceOfPointToSegment(const PointType& Point, const PointType& SegmentPoint1, const PointType& SegmentPoint2)
{
	double Coordinate;
	return ProjectPointOnSegment(Point, SegmentPoint1, SegmentPoint2, Coordinate, /*bRestrictCoodinateToInside*/ true).Distance(Point);
}

/**
 * @return the distance between the point and the segment. If the projection of the point on the segment
 * is not inside it, return the distance of the point to nearest the segment extremity
 */
template<class PointType>
inline double SquareDistanceOfPointToSegment(const PointType& Point, const PointType& SegmentPoint1, const PointType& SegmentPoint2)
{
	double Coordinate;
	return ProjectPointOnSegment(Point, SegmentPoint1, SegmentPoint2, Coordinate, /*bRestrictCoodinateToInside*/ true).SquareDistance(Point);
}

/**
 * @return the distance between the point and the line i.e. the distance between the point and its projection on the line
 */
template<class PointType>
inline double DistanceOfPointToLine(const PointType& Point, const PointType& LinePoint1, const PointType& LineDirection)
{
	double Coordinate;
	return ProjectPointOnSegment(Point, LinePoint1, LinePoint1 + LineDirection, Coordinate, /*bRestrictCoodinateToInside*/ false).Distance(Point);
}

CADKERNEL_API double ComputeCurvature(const FPoint& Gradient, const FPoint& Laplacian);
CADKERNEL_API double ComputeCurvature(const FPoint& normal, const FPoint& Gradient, const FPoint& Laplacian);

/**
 * @param OutCoordinate: the coordinate of the projected point in the segment AB (coodinate of A = 0 and of B = 1)
 * @return Projected point
 */
template<class PointType>
inline PointType ProjectPointOnSegment(const PointType& Point, const PointType& InSegmentA, const PointType& InSegmentB, double& OutCoordinate, bool bRestrictCoodinateToInside = true)
{
	PointType Segment = InSegmentB - InSegmentA;

	double SquareLength = Segment * Segment;

	if (SquareLength <= 0.0)
	{
		OutCoordinate = 0.0;
		return InSegmentA;
	}
	else
	{
		PointType APoint = Point - InSegmentA;
		OutCoordinate = (APoint * Segment) / SquareLength;

		if (bRestrictCoodinateToInside)
		{
			if (OutCoordinate < 0.0)
			{
				OutCoordinate = 0.0;
				return InSegmentA;
			}

			if (OutCoordinate > 1.0)
			{
				OutCoordinate = 1.0;
				return InSegmentB;
			}
		}

		PointType ProjectedPoint = Segment * OutCoordinate;
		ProjectedPoint += InSegmentA;
		return ProjectedPoint;
	}
}

/**
 * @return Coordinate of the projected point in the segment AB (coordinate of A = 0 and of B = 1)
 */
template<class PointType>
inline double CoordinateOfProjectedPointOnSegment(const PointType& Point, const PointType& InSegmentA, const PointType& InSegmentB, bool bRestrictCoodinateToInside = true)
{
	PointType Segment = InSegmentB - InSegmentA;

	double SquareLength = Segment * Segment;

	if (SquareLength <= 0.0)
	{
		return 0.0;
	}
	else
	{
		PointType APoint = Point - InSegmentA;
		double Coordinate = (APoint * Segment) / SquareLength;

		if (bRestrictCoodinateToInside)
		{
			if (Coordinate < 0.0)
			{
				return 0.0;
			}

			if (Coordinate > 1.0)
			{
				return 1.0;
			}
		}

		return Coordinate;
	}
}

CADKERNEL_API void FindLoopIntersectionsWithIso(const EIso Iso, const double IsoParameter, const TArray<TArray<FPoint2D>>& Loops, TArray<double>& OutIntersections);

/**
 * The segments must intersect because no check is done
 */
inline FPoint2D FindIntersectionOfSegments2D(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD, double& OutABIntersectionCoordinate)
{
	const FPoint2D AB = SegmentAB[1] - SegmentAB[0];
	const FPoint2D DC = SegmentCD[0] - SegmentCD[1];
	const FPoint2D AC = SegmentCD[0] - SegmentAB[0];

	double ParallelCoef = DC ^ AB;
	if (FMath::IsNearlyZero(ParallelCoef))
	{
		const double SquareAB = AB * AB;
		double CCoordinate = (AB * AC) / SquareAB;

		const FPoint2D AD = SegmentCD[1] - SegmentAB[0];
		double DCoordinate = (AB * AD) / SquareAB;

		if (CCoordinate >= -DOUBLE_KINDA_SMALL_NUMBER && CCoordinate <= 1 + DOUBLE_KINDA_SMALL_NUMBER)
		{
			if (DCoordinate >= -DOUBLE_KINDA_SMALL_NUMBER && DCoordinate <= 1 + DOUBLE_KINDA_SMALL_NUMBER)
			{
				OutABIntersectionCoordinate = (DCoordinate + CCoordinate) * 0.5;
				return SegmentCD[0].Middle(SegmentCD[1]);
			}

			CCoordinate = FMath::Clamp(CCoordinate, 0., 1.);
			OutABIntersectionCoordinate = CCoordinate;
			return SegmentCD[0];
		}
		else if (DCoordinate >= -DOUBLE_KINDA_SMALL_NUMBER && DCoordinate <= 1 + DOUBLE_KINDA_SMALL_NUMBER)
		{
			DCoordinate = FMath::Clamp(DCoordinate, 0., 1.);
			OutABIntersectionCoordinate = DCoordinate;
			return SegmentCD[1];
		}
		else
		{
			OutABIntersectionCoordinate = 0.5;
			return SegmentAB[0].Middle(SegmentAB[1]);
		}
	}

	OutABIntersectionCoordinate = (DC ^ AC) / ParallelCoef;
	OutABIntersectionCoordinate = FMath::Clamp(OutABIntersectionCoordinate, 0., 1.);

	return SegmentAB[0] + AB * OutABIntersectionCoordinate;
}

/**
 * The segments must intersect because no check is done
 */
inline FPoint2D FindIntersectionOfSegments2D(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD)
{
	double ABIntersectionCoordinate;
	return FindIntersectionOfSegments2D(SegmentAB, SegmentCD, ABIntersectionCoordinate);
}

/**
 * @return false if the lines are parallel
 */
inline bool FindIntersectionOfLines2D(const FSegment2D& LineAB, const FSegment2D& LineCD, FPoint2D& OutIntersectionPoint)
{
	constexpr const double Min = -DOUBLE_SMALL_NUMBER;
	constexpr const double Max = 1. + DOUBLE_SMALL_NUMBER;

	const FPoint2D AB = LineAB[1] - LineAB[0];
	const FPoint2D DC = LineCD[0] - LineCD[1];
	const FPoint2D AC = LineCD[0] - LineAB[0];

	double ParallelCoef = DC ^ AB;
	if (FMath::IsNearlyZero(ParallelCoef))
	{
		return false;
	}

	double OutABIntersectionCoordinate = (DC ^ AC) / ParallelCoef;
	OutIntersectionPoint = LineAB[0] + AB * OutABIntersectionCoordinate;
	return true;
}

/**
 * Similar as FastDoIntersect but check intersection if both segment are carried by the same line.
 * This method is 50% slower than FastIntersectSegments2D even if the segments tested are never carried by the same line
 */
CADKERNEL_API bool DoIntersect(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD);
CADKERNEL_API bool DoIntersectInside(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD);

inline bool AreParallel(const FSegment2D& SegmentAB, const FSegment2D& SegmentCD)
{
	const FPoint2D AB = SegmentAB.GetVector().Normalize();
	const FPoint2D CD = SegmentCD.GetVector().Normalize();
	const double ParallelCoef = AB ^ CD;
	return (FMath::IsNearlyZero(ParallelCoef, DOUBLE_KINDA_SMALL_NUMBER));
};

inline double ComputeCosinus(FVector Vector, FVector OtherVector)
{
	Vector.Normalize();
	OtherVector.Normalize();

	double Cosinus = Vector.X * OtherVector.X + Vector.Y * OtherVector.Y + Vector.Z * OtherVector.Z;

	return FMath::Max(-1.0, FMath::Min(Cosinus, 1.0));
}


} // namespace UE::CADKernel

