// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Geo/Surfaces/Surface.h"

#include "CADKernel/Core/CADKernelArchive.h"

#include "CADKernel/Geo/Surfaces/PlaneSurface.h"
#include "CADKernel/Geo/Surfaces/BezierSurface.h"
#include "CADKernel/Geo/Surfaces/NURBSSurface.h"
#include "CADKernel/Geo/Surfaces/RuledSurface.h"
#include "CADKernel/Geo/Surfaces/RevolutionSurface.h"
#include "CADKernel/Geo/Surfaces/TabulatedCylinderSurface.h"
#include "CADKernel/Geo/Surfaces/CylinderSurface.h"
#include "CADKernel/Geo/Surfaces/OffsetSurface.h"
#include "CADKernel/Geo/Surfaces/CompositeSurface.h"
#include "CADKernel/Geo/Surfaces/CoonsSurface.h"
#include "CADKernel/Geo/Surfaces/SphericalSurface.h"
#include "CADKernel/Geo/Surfaces/TorusSurface.h"
#include "CADKernel/Geo/Surfaces/ConeSurface.h"


namespace UE::CADKernel
{
TSharedPtr<FSurface> FSurface::Deserialize(FCADKernelArchive& Archive)
{
	ESurface SurfaceType = ESurface::None;
	Archive << SurfaceType;

	switch (SurfaceType)
	{
	case ESurface::Bezier:
		return FEntity::MakeShared<FBezierSurface>(Archive);
	case ESurface::Cone:
		return FEntity::MakeShared<FConeSurface>(Archive);
	case ESurface::Composite:
		return FEntity::MakeShared<FCompositeSurface>(Archive);
	case ESurface::Coons:
		return FEntity::MakeShared<FCoonsSurface>(Archive);
	case ESurface::Cylinder:
		return FEntity::MakeShared<FCylinderSurface>(Archive);
	case ESurface::Nurbs:
		return FEntity::MakeShared<FNURBSSurface>(Archive);
	case ESurface::Offset:
		return FEntity::MakeShared<FOffsetSurface>(Archive);
	case ESurface::Plane:
		return FEntity::MakeShared<FPlaneSurface>(Archive);
	case ESurface::Revolution:
		return FEntity::MakeShared<FRevolutionSurface>(Archive);
	case ESurface::Ruled:
		return FEntity::MakeShared<FRuledSurface>(Archive);
	case ESurface::Sphere:
		return FEntity::MakeShared<FSphericalSurface>(Archive);
	case ESurface::TabulatedCylinder:
		return FEntity::MakeShared<FTabulatedCylinderSurface>(Archive);
	case ESurface::Torus:
		return FEntity::MakeShared<FTorusSurface>(Archive);
	default:
		return TSharedPtr<FSurface>();
	}
	return TSharedPtr<FSurface>();
}

} // namespace UE::CADKernel

