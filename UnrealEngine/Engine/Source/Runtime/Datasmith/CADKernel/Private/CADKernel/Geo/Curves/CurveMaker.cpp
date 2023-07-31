// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Curves/BoundedCurve.h"
#include "CADKernel/Geo/Curves/BezierCurve.h"
#include "CADKernel/Geo/Curves/Curve.h"
#include "CADKernel/Geo/Curves/NURBSCurve.h"
#include "CADKernel/Geo/Curves/SegmentCurve.h"
#include "CADKernel/Geo/Curves/EllipseCurve.h"
#include "CADKernel/Geo/Curves/HyperbolaCurve.h"
#include "CADKernel/Geo/Curves/ParabolaCurve.h"
#include "CADKernel/Geo/Curves/CompositeCurve.h"
#include "CADKernel/Geo/Curves/PolylineCurve.h"
#include "CADKernel/Geo/Curves/RestrictionCurve.h"
#include "CADKernel/Geo/Curves/SurfacicCurve.h"

namespace UE::CADKernel
{

TSharedPtr<FCurve> FCurve::MakeNurbsCurve(FNurbsCurveData& InNurbsData)
{
	return FEntity::MakeShared<UE::CADKernel::FNURBSCurve>(InNurbsData);
}

}