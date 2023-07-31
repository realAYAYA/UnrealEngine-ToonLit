// Copyright Epic Games, Inc. All Rights Reserved.

// Port of WildMagic IntrRay3Triangle3

#pragma once

#include "VectorTypes.h"
#include "TriangleTypes.h"
#include "VectorUtil.h"
#include "Math/Ray.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Compute intersection between 3D ray and 3D triangle
 */
template <typename Real>
class TIntrRay3Triangle3
{
public:
	// Input
	TRay<Real> Ray;
	TTriangle3<Real> Triangle;

	// Output
	Real RayParameter;
	FVector3d TriangleBaryCoords;
	EIntersectionType IntersectionType;


	TIntrRay3Triangle3(const TRay<Real>& RayIn, const TTriangle3<Real>& TriangleIn)
	{
		Ray = RayIn;
		Triangle = TriangleIn;
	}


	/**
	 * @return true if ray intersects triangle
	 */
	bool Test()
	{
		// Compute the offset origin, edges, and normal.
		TVector<Real> diff = Ray.Origin - Triangle.V[0];
		TVector<Real> edge1 = Triangle.V[1] - Triangle.V[0];
		TVector<Real> edge2 = Triangle.V[2] - Triangle.V[0];
		TVector<Real> normal = edge1.Cross(edge2);

		// Solve Q + t*D = b1*E1 + b2*E2 (Q = kDiff, D = ray direction,
		// E1 = kEdge1, E2 = kEdge2, N = Cross(E1,E2)) by
		//   |Dot(D,N)|*b1 = sign(Dot(D,N))*Dot(D,Cross(Q,E2))
		//   |Dot(D,N)|*b2 = sign(Dot(D,N))*Dot(D,Cross(E1,Q))
		//   |Dot(D,N)|*t = -sign(Dot(D,N))*Dot(Q,N)
		Real DdN = Ray.Direction.Dot(normal);
		Real sign;
		if (DdN > TMathUtil<Real>::ZeroTolerance)
		{
			sign = (Real)1;
		}
		else if (DdN < -TMathUtil<Real>::ZeroTolerance)
		{
			sign = (Real)-1;
			DdN = -DdN;
		}
		else
		{
			// Ray and triangle are parallel, call it a "no intersection"
			// even if the ray does intersect.
			IntersectionType = EIntersectionType::Empty;
			return false;
		}

		Real DdQxE2 = sign * Ray.Direction.Dot(diff.Cross(edge2));
		if (DdQxE2 >= (Real)0)
		{
			Real DdE1xQ = sign * Ray.Direction.Dot(edge1.Cross(diff));
			if (DdE1xQ >= (Real)0)
			{
				if (DdQxE2 + DdE1xQ <= DdN)
				{
					// Line intersects triangle, check if ray does.
					Real QdN = -sign * diff.Dot(normal);
					if (QdN >= (Real)0)
					{
						// Ray intersects triangle.
						IntersectionType = EIntersectionType::Point;
						return true;
					}
					// else: t < 0, no intersection
				}
				// else: b1+b2 > 1, no intersection
			}
			// else: b2 < 0, no intersection
		}
		// else: b1 < 0, no intersection

		IntersectionType = EIntersectionType::Empty;
		return false;
	}


	/**
	 * Find intersection point
	 * @return true if ray intersects triangle
	 */
	bool Find()
	{
		// Compute the offset origin, edges, and normal.
		TVector<Real> diff = Ray.Origin - Triangle.V[0];
		TVector<Real> edge1 = Triangle.V[1] - Triangle.V[0];
		TVector<Real> edge2 = Triangle.V[2] - Triangle.V[0];
		TVector<Real> normal = edge1.Cross(edge2);

		// Solve Q + t*D = b1*E1 + b2*E2 (Q = kDiff, D = ray direction,
		// E1 = kEdge1, E2 = kEdge2, N = Cross(E1,E2)) by
		//   |Dot(D,N)|*b1 = sign(Dot(D,N))*Dot(D,Cross(Q,E2))
		//   |Dot(D,N)|*b2 = sign(Dot(D,N))*Dot(D,Cross(E1,Q))
		//   |Dot(D,N)|*t = -sign(Dot(D,N))*Dot(Q,N)
		Real DdN = Ray.Direction.Dot(normal);
		Real sign;
		if (DdN > TMathUtil<Real>::ZeroTolerance)
		{
			sign = (Real)1;
		}
		else if (DdN < -TMathUtil<Real>::ZeroTolerance)
		{
			sign = (Real)-1;
			DdN = -DdN;
		}
		else
		{
			// Ray and triangle are parallel, call it a "no intersection"
			// even if the ray does intersect.
			IntersectionType = EIntersectionType::Empty;
			return false;
		}

		Real DdQxE2 = sign * Ray.Direction.Dot(diff.Cross(edge2));
		if (DdQxE2 >= (Real)0)
		{
			Real DdE1xQ = sign * Ray.Direction.Dot(edge1.Cross(diff));
			if (DdE1xQ >= (Real)0)
			{
				if (DdQxE2 + DdE1xQ <= DdN)
				{
					// Line intersects triangle, check if ray does.
					Real QdN = -sign * diff.Dot(normal);
					if (QdN >= (Real)0)
					{
						// Ray intersects triangle.
						Real inv = ((Real)1) / DdN;
						RayParameter = QdN * inv;
						TriangleBaryCoords.Y = DdQxE2 * inv;
						TriangleBaryCoords.Z = DdE1xQ * inv;
						TriangleBaryCoords.X = (Real)1 - TriangleBaryCoords.Y - TriangleBaryCoords.Z;
						IntersectionType = EIntersectionType::Point;
						return true;
					}
					// else: t < 0, no intersection
				}
				// else: b1+b2 > 1, no intersection
			}
			// else: b2 < 0, no intersection
		}
		// else: b1 < 0, no intersection

		IntersectionType = EIntersectionType::Empty;
		return false;
	}

};

typedef TIntrRay3Triangle3<float> FIntrRay3Triangle3f;
typedef TIntrRay3Triangle3<double> FIntrRay3Triangle3d;

} // end namespace UE::Geometry
} // end namespace UE
