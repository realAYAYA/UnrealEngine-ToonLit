// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VectorTypes.h"
#include "VectorUtil.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Triangle utility functions
 */
namespace TriangleUtil
{
	/**
	 * @return the edge length of an equilateral/regular triangle with the given area
	 */
	template<typename RealType>
	RealType EquilateralEdgeLengthForArea(RealType TriArea)
	{
		return TMathUtil<RealType>::Sqrt(((RealType)4 * TriArea) / TMathUtil<RealType>::Sqrt3);
	}

};




template<typename RealType>
struct TTriangle2
{
	TVector2<RealType> V[3];

	TTriangle2() {}

	TTriangle2(const TVector2<RealType>& V0, const TVector2<RealType>& V1, const TVector2<RealType>& V2)
	{
		V[0] = V0;
		V[1] = V1;
		V[2] = V2;
	}

	TTriangle2(const TVector2<RealType> VIn[3])
	{
		V[0] = VIn[0];
		V[1] = VIn[1];
		V[2] = VIn[2];
	}

	TVector2<RealType> BarycentricPoint(RealType Bary0, RealType Bary1, RealType Bary2) const
	{
		return Bary0 * V[0] + Bary1 * V[1] + Bary2 * V[2];
	}

	TVector2<RealType> BarycentricPoint(const TVector<RealType>& BaryCoords) const
	{
		return BaryCoords[0] * V[0] + BaryCoords[1] * V[1] + BaryCoords[2] * V[2];
	}

	TVector<RealType> GetBarycentricCoords(const TVector2<RealType>& Point) const
	{
		return VectorUtil::BarycentricCoords(Point, V[0], V[1], V[2]);
	}

	/**
	 * @param A first vertex of triangle
	 * @param B second vertex of triangle
	 * @param C third vertex of triangle
	 * @return signed area of triangle
	 */
	static RealType SignedArea(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C)
	{
		return ((RealType)0.5) * ((A.X*B.Y - A.Y*B.X) + (B.X*C.Y - B.Y*C.X) + (C.X*A.Y - C.Y*A.X));
	}

	/** @return signed area of triangle */
	RealType SignedArea() const
	{
		return SignedArea(V[0], V[1], V[2]);
	}
	
	/** @return unsigned area of the triangle */
	RealType Area() const
	{
		return TMathUtil<RealType>::Abs(SignedArea());
	}


	/**
	 * @param A first vertex of triangle
	 * @param B second vertex of triangle
	 * @param C third vertex of triangle
	 * @param QueryPoint test point
	 * @return true if QueryPoint is inside triangle
	 */
	static bool IsInside(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C, const TVector2<RealType>& QueryPoint)
	{
		RealType Sign1 = Orient(A, B, QueryPoint);
		RealType Sign2 = Orient(B, C, QueryPoint);
		RealType Sign3 = Orient(C, A, QueryPoint);
		return (Sign1*Sign2 > 0) && (Sign2*Sign3 > 0) && (Sign3*Sign1 > 0);
	}

	/** @return true if QueryPoint is inside triangle */
	bool IsInside(const TVector2<RealType>& QueryPoint) const
	{
		return IsInside(V[0], V[1], V[2], QueryPoint);
	}


	/** @return true if QueryPoint is inside triangle or on edge */
	static bool IsInsideOrOn(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C, const TVector2<RealType>& QueryPoint)
	{
		RealType Sign1 = Orient(A, B, QueryPoint);
		RealType Sign2 = Orient(B, C, QueryPoint);
		RealType Sign3 = Orient(C, A, QueryPoint);
		return (Sign1*Sign2 >= 0) && (Sign2*Sign3 >= 0) && (Sign3*Sign1 >= 0);
	}

	/** @return true if QueryPoint is inside triangle or on edge */
	bool IsInsideOrOn(const TVector2<RealType>& QueryPoint, RealType Epsilon = 0) const
	{
		return IsInsideOrOn(V[0], V[1], V[2], QueryPoint);
	}


	/**
	 * More robust (because it doesn't multiply orientation test results) inside-triangle test for oriented triangles only
	 *  (the code early-outs at the first 'outside' edge, which only works if the triangle is oriented as expected)
	 * @return 1 if outside, -1 if inside, 0 if on boundary
	 */
	int IsInsideOrOn_Oriented(const TVector2<RealType>& QueryPoint) const
	{
		return IsInsideOrOn_Oriented(V[0], V[1], V[2], QueryPoint);
	}

	/**
	 * More robust (because it doesn't multiply orientation test results) inside-triangle test for oriented triangles only
	 *  (the code early-outs at the first 'outside' edge, which only works if the triangle is oriented as expected)
	 * @return 1 if outside, -1 if inside, 0 if on boundary
	 */
	static int IsInsideOrOn_Oriented(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C, const TVector2<RealType>& QueryPoint)
	{
		checkSlow(Orient(A, B, C) <= 0); // TODO: remove this checkSlow; it's just to make sure the orientation is as expected

		RealType Sign1 = Orient(A, B, QueryPoint);
		if (Sign1 > 0)
		{
			return 1;
		}
		
		RealType Sign2 = Orient(B, C, QueryPoint);
		if (Sign2 > 0)
		{
			return 1;
		}
		
		RealType Sign3 = Orient(A, C, QueryPoint);
		if (Sign3 < 0) // note this edge is queried backwards so the sign test is also backwards
		{
			return 1;
		}

		return (Sign1 != 0 && Sign2 != 0 && Sign3 != 0) ? -1 : 0;
	}


};

typedef TTriangle2<float> FTriangle2f;
typedef TTriangle2<double> FTriangle2d;






template<typename RealType>
struct TTriangle3
{
	TVector<RealType> V[3];

	TTriangle3() {}

	TTriangle3(const TVector<RealType>& V0, const TVector<RealType>& V1, const TVector<RealType>& V2)
	{
		V[0] = V0;
		V[1] = V1;
		V[2] = V2;
	}

	TTriangle3(const TVector<RealType> VIn[3])
	{
		V[0] = VIn[0];
		V[1] = VIn[1];
		V[2] = VIn[2];
	}

	TVector<RealType> BarycentricPoint(RealType Bary0, RealType Bary1, RealType Bary2) const
	{
		return Bary0*V[0] + Bary1*V[1] + Bary2*V[2];
	}

	TVector<RealType> BarycentricPoint(const TVector<RealType> & BaryCoords) const
	{
		return BaryCoords[0]*V[0] + BaryCoords[1]*V[1] + BaryCoords[2]*V[2];
	}

	TVector<RealType> GetBarycentricCoords(const TVector<RealType>& Point) const
	{
		return VectorUtil::BarycentricCoords(Point, V[0], V[1], V[2]);
	}

	/** @return vector that is perpendicular to the plane of this triangle */
	TVector<RealType> Normal() const
	{
		return VectorUtil::Normal(V[0], V[1], V[2]);
	}

	/** @return centroid of this triangle */
	TVector<RealType> Centroid() const
	{
		constexpr RealType f = 1.0 / 3.0;
		return TVector<RealType>(
			(V[0].X + V[1].X + V[2].X) * f,
			(V[0].Y + V[1].Y + V[2].Y) * f,
			(V[0].Z + V[1].Z + V[2].Z) * f
		);
	}

	/** grow the triangle around the centroid */
	void Expand(RealType Delta)
	{
		TVector<RealType> Centroid(Centroid());
		V[0] += Delta * ((V[0] - Centroid).Normalized());
		V[1] += Delta * ((V[1] - Centroid).Normalized());
		V[2] += Delta * ((V[2] - Centroid).Normalized());
	}
};

typedef TTriangle3<float> FTriangle3f;
typedef TTriangle3<double> FTriangle3d;
typedef TTriangle3<int> FTriangle3i;

} // end namespace UE::Geometry
} // end namespace UE