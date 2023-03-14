// Copyright Epic Games, Inc. All Rights Reserved.

// Interface for exact predicates w/ Unreal Engine vector types

#pragma once

#include "CoreMinimal.h"
#include "Math/MathFwd.h"
#include "VectorTypes.h"

namespace UE {
namespace Math { template <typename T> struct TVector2; }
namespace Math { template <typename T> struct TVector; }

namespace Geometry {
namespace ExactPredicates {

using namespace UE::Math;

/**
 * Must be called once for exact predicates to work.
 * Will be called by GeometryAlgorithmsModule startup, so you don't need to manually call this.
 */
void GEOMETRYCORE_API GlobalInit();

double GEOMETRYCORE_API Orient2DInexact(double* PA, double* PB, double* PC);
double GEOMETRYCORE_API Orient2D(double* PA, double* PB, double* PC);

double GEOMETRYCORE_API Orient3DInexact(double* PA, double* PB, double* PC, double* PD);
double GEOMETRYCORE_API Orient3D(double* PA, double* PB, double* PC, double* PD);

double GEOMETRYCORE_API Facing3D(double* PA, double* PB, double* PC, double* Direction);
double GEOMETRYCORE_API Facing2D(double* PA, double* PB, double* Direction);

double GEOMETRYCORE_API InCircleInexact(double* PA, double* PB, double* PC, double* PD);
double GEOMETRYCORE_API InCircle(double* PA, double* PB, double* PC, double* PD);

// Note: The float versions of these functions can be marginally faster in some cases,
// but also are often orders of magnitude slower, and can fail to underflow at much larger values.
// Consider calling the double versions even for float inputs.
float GEOMETRYCORE_API Orient2DInexact(float* PA, float* PB, float* PC);
float GEOMETRYCORE_API Orient2D(float* PA, float* PB, float* PC);

float GEOMETRYCORE_API Orient3DInexact(float* PA, float* PB, float* PC, float* PD);
float GEOMETRYCORE_API Orient3D(float* PA, float* PB, float* PC, float* PD);

float GEOMETRYCORE_API Facing3D(float* PA, float* PB, float* PC, float* Direction);
float GEOMETRYCORE_API Facing2D(float* PA, float* PB, float* PC);

float GEOMETRYCORE_API InCircleInexact(float* PA, float* PB, float* PC, float* PD);
float GEOMETRYCORE_API InCircle(float* PA, float* PB, float* PC, float* PD);

/**
 * Fully generic version; always computes in double precision
 * @return value indicating which side of line AB point C is on, or 0 if ABC are collinear
 */
template<typename VectorType>
double Orient2D(const VectorType& A, const VectorType& B, const VectorType& C)
{
	double PA[2]{ A.X, A.Y };
	double PB[2]{ B.X, B.Y };
	double PC[2]{ C.X, C.Y };
	return Orient2D(PA, PB, PC);
}

/**
 * Fully generic version; always computes in double precision
 * @return value indicating which side of triangle ABC point D is on, or 0 if ABCD are coplanar
 */
template<typename VectorType>
double Orient3D(const VectorType& A, const VectorType& B, const VectorType& C, const VectorType& D)
{
	double PA[3]{ A.X, A.Y, A.Z };
	double PB[3]{ B.X, B.Y, B.Z };
	double PC[3]{ C.X, C.Y, C.Z };
	double PD[3]{ D.X, D.Y, D.Z };
	return Orient3D(PA, PB, PC, PD);
}

// Note: Fully generic version of InCircle not provided; favor InCircle2<RealType>
//template<typename VectorType>
//double InCircle(const VectorType& A, const VectorType& B, const VectorType& C, const VectorType& D)

/**
 * TVector2-only version that can run in float or double
 * @return value indicating which side of line AB point C is on, or 0 if ABC are collinear
 */
template<typename RealType>
RealType Orient2(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C)
{
	RealType PA[2]{ A.X, A.Y };
	RealType PB[2]{ B.X, B.Y };
	RealType PC[2]{ C.X, C.Y };
	return Orient2D(PA, PB, PC);
}

/**
 * TVector-only version that can run in float or double
 * @return value indicating which side of triangle ABC point D is on, or 0 if ABCD are coplanar
 */
template<typename RealType>
RealType Orient3(const TVector<RealType>& A, const TVector<RealType>& B, const TVector<RealType>& C, const TVector<RealType>& D)
{
	// Note: Though we support computing exact predicates in float precision directly, in practice
	// it is generally faster and more reliable to compute in double precision
	double PA[3]{ (double)A.X, (double)A.Y, (double)A.Z };
	double PB[3]{ (double)B.X, (double)B.Y, (double)B.Z };
	double PC[3]{ (double)C.X, (double)C.Y, (double)C.Z };
	double PD[3]{ (double)D.X, (double)D.Y, (double)D.Z };
	if constexpr (TIsSame<RealType, double>::Value)
	{
		return Orient3D(PA, PB, PC, PD);
	}
	else
	{
		// make sure the sign is not lost in casting
		return (RealType)FMath::Sign(Orient3D(PA, PB, PC, PD));
	}
}

/**
 * @return value w/ sign indicating whether the normal of line AB is facing Direction (and 0 if parallel)
 *  (i.e. the sign of -PerpCW(B-A) dot Dir)
 */
template<typename RealType>
RealType Facing2(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& Direction)
{
	RealType PA[2]{ A.X, A.Y };
	RealType PB[2]{ B.X, B.Y };
	RealType Dir[2]{ Direction.X, Direction.Y };
	return Facing2D(PA, PB, Dir);
}

/**
 * TVector-only version that can run in float or double
 * @return value w/ sign indicating whether triangle ABC is facing Direction, or 0 if it is parallel to Direction
 */
template<typename RealType>
RealType Facing3(const TVector<RealType>& A, const TVector<RealType>& B, const TVector<RealType>& C, const TVector<RealType>& Direction)
{
	// Note: Though we support computing exact predicates in float precision directly, in practice
	// it is generally faster and more reliable to compute in double precision
	double PA[3]{ (double)A.X, (double)A.Y, (double)A.Z };
	double PB[3]{ (double)B.X, (double)B.Y, (double)B.Z };
	double PC[3]{ (double)C.X, (double)C.Y, (double)C.Z };
	double Dir[3]{ (double)Direction.X, (double)Direction.Y, (double)Direction.Z };
	if constexpr (TIsSame<RealType, double>::Value)
	{
		return Facing3D(PA, PB, PC, Dir);
	}
	else
	{
		// make sure the sign is not lost in casting
		return (RealType)FMath::Sign(Facing3D(PA, PB, PC, Dir));
	}
}

/**
 * TVector2-only version that can run in float or double
 * @return value indicating whether point D is inside, outside, or exactly on the circle passing through ABC
 * Note: Sign of the result depends on the orientation of triangle ABC --
 *	Counterclockwise: Inside is positive
 *	Clockwise: Inside is negative
 */
template<typename RealType>
RealType InCircle2(const TVector2<RealType>& A, const TVector2<RealType>& B, const TVector2<RealType>& C, const TVector2<RealType>& D)
{
	// Note: Though we support computing exact predicates in float precision directly, in practice
	// it is generally faster and more reliable to compute in double precision
	double PA[2]{ (double)A.X, (double)A.Y };
	double PB[2]{ (double)B.X, (double)B.Y };
	double PC[2]{ (double)C.X, (double)C.Y };
	double PD[2]{ (double)D.X, (double)D.Y };
	if constexpr (TIsSame<RealType, double>::Value)
	{
		return InCircle(PA, PB, PC, PD);
	}
	else
	{
		// make sure the sign is not lost in casting
		return (RealType)FMath::Sign(InCircle(PA, PB, PC, PD));
	}
}


// TODO: insphere predicates (currently disabled because they have a huge stack allocation that scares static analysis)

}}} // namespace UE::Geometry::ExactPredicates
