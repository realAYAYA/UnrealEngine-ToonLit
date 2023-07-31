// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Math/Point.h"

#include "CADKernel/Utils/Util.h"
#include "CADKernel/Math/MatrixH.h"

namespace UE::CADKernel
{

const FPoint FPoint::ZeroPoint(0.,0.,0.);
const FPoint FPoint::UnitPoint(1., 1., 1.);
const FPoint FPoint::FarawayPoint(HUGE_VALUE, HUGE_VALUE, HUGE_VALUE);
const int32 FPoint::Dimension = 3;

const FFPoint FFPoint::ZeroPoint(0.f, 0.f, 0.f);
const FFPoint FFPoint::FarawayPoint(HUGE_VALUE, HUGE_VALUE, HUGE_VALUE);
const int32 FFPoint::Dimension = 3;

const FPoint2D FPoint2D::ZeroPoint(0., 0.);
const FPoint2D FPoint2D::FarawayPoint(HUGE_VALUE, HUGE_VALUE);
const int32 FPoint2D::Dimension = 2;

const FPointH FPointH::ZeroPoint(0., 0., 0., 1.);
const FPointH FPointH::FarawayPoint(HUGE_VALUE, HUGE_VALUE, HUGE_VALUE, 1.);
const int32 FPointH::Dimension = 4;

double FPoint::SignedAngle(const FPoint & Other, const FPoint & Normal) const
{
	FPoint Vector1 = *this; 
	FPoint Vector2 = Other; 
	FPoint Vector3 = Normal; 

	Vector1.Normalize();
	Vector2.Normalize();
	Vector3.Normalize();

	double ScalarProduct = Vector1 * Vector2;

	if (ScalarProduct >= 1 - DOUBLE_SMALL_NUMBER)
	{
		return 0.;
	}

	if (ScalarProduct <= -1 + DOUBLE_SMALL_NUMBER)
	{
		return DOUBLE_PI;
	}

	return MixedTripleProduct(Vector1, Vector2, Vector3) > 0 ? acos(ScalarProduct) : -acos(ScalarProduct);
}


}