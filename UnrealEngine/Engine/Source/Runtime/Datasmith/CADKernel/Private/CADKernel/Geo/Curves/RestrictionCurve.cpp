// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Curves/RestrictionCurve.h"

namespace UE::CADKernel
{

void FRestrictionCurve::ExtendTo(const FPoint2D& Point)
{
	Curve2D->ExtendTo(Point);
	EvaluateSurfacicPolyline(Polyline);
}

void FRestrictionCurve::Offset2D(const FPoint2D& Offset)
{
	Curve2D->Offset(Offset);
	EvaluateSurfacicPolyline(Polyline);
}


#ifdef CADKERNEL_DEV
FInfoEntity& FRestrictionCurve::GetInfo(FInfoEntity& Info) const
{
	return FSurfacicCurve::GetInfo(Info)
		.Add(TEXT("2D polyline"), Polyline.Points2D)
		.Add(TEXT("3D polyline"), Polyline.Points3D);
}
#endif

}
