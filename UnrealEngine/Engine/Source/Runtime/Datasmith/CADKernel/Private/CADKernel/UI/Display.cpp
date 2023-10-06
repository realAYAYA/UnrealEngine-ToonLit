// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/UI/Display.h"

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/Group.h"
#include "CADKernel/Core/System.h"
#include "CADKernel/Core/OrientedEntity.h"
#include "CADKernel/Geo/Curves/RestrictionCurve.h"
#include "CADKernel/Geo/Curves/BezierCurve.h"
#include "CADKernel/Geo/Curves/NURBSCurve.h"
#include "CADKernel/Geo/Curves/RestrictionCurve.h"
#include "CADKernel/Geo/Curves/SplineCurve.h"
#include "CADKernel/Geo/Sampler/SamplerOnChord.h"
#include "CADKernel/Geo/Sampler/SamplerOnParam.h"
#include "CADKernel/Geo/Surfaces/BezierSurface.h"
#include "CADKernel/Geo/Surfaces/NURBSSurface.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/Math/Aabb.h"
#include "CADKernel/Math/Boundary.h"
#include "CADKernel/Mesh/Structure/EdgeMesh.h"
#include "CADKernel/Mesh/Structure/ModelMesh.h"
#include "CADKernel/Mesh/Structure/FaceMesh.h"
#include "CADKernel/Mesh/Structure/VertexMesh.h"
#include "CADKernel/Topo/Body.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/Model.h"
#include "CADKernel/Topo/Shell.h"
#include "CADKernel/Topo/TopologicalFace.h"

#include "Math/Plane.h"

namespace UE::CADKernel
{

#ifdef CADKERNEL_DEBUG
void Open3DDebugSession(FString name, const TArray<FIdent>& IdArray)
{
	FSystem::Get().GetVisu()->Open3DDebugSession(*name, IdArray);
}

void Close3DDebugSession(bool bIsDisplayed)
{
	if (bIsDisplayed)
	{
		FSystem::Get().GetVisu()->Close3DDebugSession();
	}
}

void Wait(bool bMakeWait)
{
	if (bMakeWait)
	{
		system("PAUSE");
		printf("Hello\n");
	}
}
#endif

void Open3DDebugSegment(FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->Open3DDebugSegment(Ident);
#endif
}

void Close3DDebugSegment()
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->Close3DDebugSegment();
#endif
}

void FlushVisu()
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->UpdateViewer();
#endif
}

void DrawElement(int32 Dimension, TArray<FPoint>& Points, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->DrawElement(Dimension, Points, Property);
#endif
}

void DrawMesh(const FMesh& Mesh)
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->DrawMesh(Mesh.GetId());
#endif
}

void Draw(const FLinearBoundary& Boundary, const FRestrictionCurve& Curve, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	if (Boundary.IsDegenerated())
	{
		return;
	}

	bool bShowOrientation = FSystem::Get().GetVisu()->GetParameters()->bDisplayCADOrient;

	TArray<FPoint> Polyline;
	Curve.GetDiscretizationPoints<FPoint>(Boundary, EOrientation::Front, Polyline);
	Draw(Polyline, Property);

	if (bShowOrientation)
	{
		double Length = 0;
		for (int32 Index = 1; Index < Polyline.Num(); ++Index)
		{
			Length += Polyline[Index].Distance(Polyline[Index - 1]);
		}

		double Coordinate = (Boundary.Max + Boundary.Min) / 2.;
		FCurvePoint Point;
		Curve.EvaluatePoint(Coordinate, Point, 1);

		double Height = Length / 20.0;
		double Base = Height / 2;

		DrawQuadripode(Height, Base, Point.Point, Point.Gradient, Property);
	}
#endif
}

void Draw2D(const FCurve& Curve, const FLinearBoundary& Boundary, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	if (Boundary.IsDegenerated())
	{
		return;
	}

	double DiscVisu = ((double)(FSystem::Get().GetVisu()->GetParameters()->ChordError)) / 10.;
	bool bVoirOrientation = FSystem::Get().GetVisu()->GetParameters()->bDisplayCADOrient;

	FPolyline2D Polyline;
	FCurve2DSamplerOnChord Sampler(Curve, Boundary, DiscVisu, Polyline);
	Sampler.Sample();

	Draw(Polyline.GetPoints(), Property);

	if (bVoirOrientation)
	{
		double Length = Polyline.GetLength(Boundary);

		double Coordinate = (Boundary.Max + Boundary.Min) / 2.;
		FCurvePoint Point;
		Curve.EvaluatePoint(Coordinate, Point, 1);

		double Height = Length / 20.0;
		double Base = Height / 2;

		DrawQuadripode(Height, Base, Point.Point, Point.Gradient, Property);
	}
#endif
}

void Draw3D(const FCurve& Curve, const FLinearBoundary& Boundary, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	if (Boundary.IsDegenerated())
	{
		return;
	}

	double DiscVisu = FSystem::Get().GetVisu()->GetParameters()->ChordError;
	bool bVoirOrientation = FSystem::Get().GetVisu()->GetParameters()->bDisplayCADOrient;

	FPolyline3D Polyline;
	FCurveSamplerOnChord Sampler(Curve, Boundary, DiscVisu, Polyline);
	Sampler.Sample();

	Draw(Polyline.GetPoints(), Property);

	if (bVoirOrientation)
	{
		double Length = Polyline.GetLength(Boundary);

		double Coordinate = (Boundary.Max + Boundary.Min) / 2.;
		FCurvePoint Point;
		Curve.EvaluatePoint(Coordinate, Point, 1);

		double Height = Length / 20.0;
		double Base = Height / 2;

		DrawQuadripode(Height, Base, Point.Point, Point.Gradient, Property);
	}
#endif
}

void Draw(const FCurve& Curve, const FLinearBoundary& Boundary, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	if (Curve.GetDimension() == 3)
	{
		Draw3D(Curve, Boundary, Property);
	}
	else
	{
		Draw2D(Curve, Boundary, Property);
	}
#endif
}


void Draw(const FCurve& Curve, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	const FLinearBoundary& Bounds = Curve.GetBoundary();
	Draw(Curve, Bounds, Property);
#endif
}

void DrawQuadripode(double Height, double Base, FPoint& Center, FPoint& InDirection, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	FPoint Direction = InDirection;
	Direction.Normalize();

	FPoint Normal(-Direction[1], Direction[0], 0.0);

	double UVNorm = Normal.Length();
	Normal /= UVNorm;

	FPoint BiNormal(-Direction[2] * Normal[1], Direction[2] * Normal[0], UVNorm);

	FPoint Point0 = Center;
	FPoint PointBase = Point0 - Direction * Height;

	Normal *= Base;
	BiNormal *= Base;

	FPoint Point1 = PointBase + Normal;
	FPoint Point2 = PointBase + BiNormal;
	FPoint Point3 = PointBase - Normal;
	FPoint Point4 = PointBase - BiNormal;

	TArray<FPoint> Polygone;
	Polygone.Reserve(7);

	Polygone.Add(Point1);
	Polygone.Add(Point0);
	Polygone.Add(Point2);
	Polygone.Add(Point0);
	Polygone.Add(Point3);
	Polygone.Add(Point0);
	Polygone.Add(Point4);

	Draw(Polygone, Property);
#endif
}

void DisplayEntity(const FEntity& Entity)
{
#ifdef CADKERNEL_DEBUG
	if (Entity.IsDeleted())
	{
		return;
	}

	FTimePoint StartTime = FChrono::Now();
	FProgress Progress;

	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %d"), Entity.GetTypeName(), Entity.GetId()), { Entity.GetId() });

	switch (Entity.GetEntityType())
	{
	case EEntity::TopologicalVertex:
		Display((const FTopologicalVertex&)Entity);
		break;
	case EEntity::Curve:
		Display((const FCurve&)Entity);
		break;
	case EEntity::Surface:
		Display((const FSurface&)Entity);
		break;
	case EEntity::TopologicalFace:
		Display((const FTopologicalFace&)Entity);
		break;
	case EEntity::TopologicalLoop:
		Display((const FTopologicalLoop&)Entity);
		break;
	case EEntity::Shell:
		Display((const FShell&)Entity);
		break;
	case EEntity::Body:
		Display((const FBody&)Entity);
		break;
	case EEntity::TopologicalEdge:
		Display((const FTopologicalEdge&)Entity);
		break;
	case EEntity::Model:
		Display((const FModel&)Entity);
		break;
	case EEntity::MeshModel:
		Display((const FModelMesh&)Entity);
		break;
	default:
		FMessage::Printf(Log, TEXT("Unable to display Entity of type %s"), FEntity::GetTypeName(Entity.GetEntityType()));
	}

	FDuration DisplayDuration = FChrono::Elapse(StartTime);
	FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Display "), DisplayDuration);

#endif
}

void DisplayEntity2D(const FEntity& Entity)
{
#ifdef CADKERNEL_DEBUG
	if (Entity.IsDeleted())
	{
		return;
	}

	FTimePoint StartTime = FChrono::Now();

	FProgress Progress;

	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %d"), Entity.GetTypeName(), Entity.GetId()), { Entity.GetId() });

	switch (Entity.GetEntityType())
	{
	case EEntity::Surface:
		Display2D((const FSurface&)Entity);
		break;
	case EEntity::TopologicalFace:
		Display2D((const FTopologicalFace&)Entity);
		break;
	case EEntity::TopologicalLoop:
		Display2D((const FTopologicalLoop&)Entity);
		break;
	case EEntity::TopologicalEdge:
		Display2D((const FTopologicalEdge&)Entity);
		break;
	default:
		FMessage::Printf(Log, TEXT("Unable to display Entity of type %s"), FEntity::GetTypeName(Entity.GetEntityType()));
	}

	FDuration DisplayDuration = FChrono::Elapse(StartTime);
	FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Display "), DisplayDuration);

#endif
}

void Display(const FPlane& Plane, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	FVector Normal = Plane.GetNormal();
	if (Normal.Size() < DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	FVector UAxis;
	// Find the first frame axis non collinear to the plane normal
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FVector Axis = FVector::ZeroVector;
		Axis[Index] = 1;
		UAxis = Normal ^ Axis;
		if (UAxis.Size() > DOUBLE_SMALL_NUMBER)
		{
			break;
		}
	}
	FVector VAxis = UAxis ^ Normal;

	UAxis.Normalize();
	UAxis *= 10;
	VAxis.Normalize();
	VAxis *= 10;

	TArray<FPoint> Points;

	const FVector Point = Plane.GetOrigin();
	
	FVector VTemp;

	VTemp = Point + UAxis + VAxis;
	Points.Emplace(VTemp[0], VTemp[1], VTemp[2]);

	VTemp = Point + UAxis - VAxis;
	Points.Emplace(VTemp[0], VTemp[1], VTemp[2]);

	VTemp = Point - UAxis - VAxis;
	Points.Emplace(VTemp[0], VTemp[1], VTemp[2]);

	VTemp = Point - UAxis + VAxis;
	Points.Emplace(VTemp[0], VTemp[1], VTemp[2]);

	VTemp = Point + UAxis + VAxis;
	Points.Emplace(VTemp[0], VTemp[1], VTemp[2]);

	F3DDebugSegment G(Ident);
	DrawElement(2, Points);
#endif
}

void DisplayEdgeCriteriaGrid(int32 EdgeId, const TArray<FPoint>& Points3D)
{
#ifdef CADKERNEL_DEBUG
	FString Name = FString::Printf(TEXT("Edge Grid %d"), EdgeId);
	F3DDebugSession G(Name);
	{
		F3DDebugSession _(TEXT("CriteriaGrid Point 3d"));
		for (int32 Index = 0; Index < Points3D.Num(); Index += 2)
		{
			DisplayPoint(Points3D[Index]);
		}
	}
	{
		F3DDebugSession _(TEXT("CriteriaGrid IntermediateU"));
		for (int32 Index = 1; Index < Points3D.Num(); Index += 2)
		{
			DisplayPoint(Points3D[Index], EVisuProperty::ControlPoint);
		}
	}
#endif
}

void DisplayAABB(const FAABB& Aabb, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	TFunction<void(int32, FPoint&, FPoint&)> GetWire = [&](int32 Segment, FPoint& A, FPoint& B)
	{
		int32 Corner1;
		int32 Corner2;

		if (Segment < 4)
		{
			Corner1 = Segment * 2;
			Corner2 = Corner1 + 1;
		}
		else if (Segment < 8)
		{
			if (Segment < 6)
			{
				Corner1 = Segment - 4;
			}
			else
			{
				Corner1 = Segment - 2;
			}
			Corner2 = Corner1 + 2;
		}
		else if (Segment < 12)
		{
			Corner1 = Segment - 8;
			Corner2 = Corner1 + 4;
		}
		else
		{
			Corner1 = 0;
			Corner2 = 1;
			ERROR_NOT_EXPECTED;
		}
		A = Aabb.GetCorner(Corner1);
		B = Aabb.GetCorner(Corner2);
	};

	F3DDebugSegment G(Ident);
	for (int32 SegmentIndex = 0; SegmentIndex < 12; SegmentIndex++)
	{
		TArray<FPoint> Points;
		Points.SetNum(2);
		GetWire(SegmentIndex, Points[0], Points[1]);
		Draw(Points);
	}
#endif
}

void DisplayAABB2D(const FAABB2D& aabb, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	FPoint A(aabb.GetMin().U, aabb.GetMin().V, 0.);
	FPoint B(aabb.GetMin().U, aabb.GetMax().V, 0.);
	FPoint C(aabb.GetMax().U, aabb.GetMax().V, 0.);
	FPoint D(aabb.GetMax().U, aabb.GetMin().V, 0.);

	TArray<FPoint> Points;
	Points.Add(A);
	Points.Add(B);
	Points.Add(C);
	Points.Add(D);
	Points.Add(A);
	Draw(Points);
#endif
}

void Display(const FTopologicalVertex& Vertex, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Vertex.GetId());
	DrawPoint(Vertex.GetCoordinates(), Property);
#endif
}

void Display(const FCurve& Curve)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Curve.GetId());
	Draw(Curve, Curve.GetBoundary());
#endif
}

void Display(const FShell& Shell)
{
#ifdef CADKERNEL_DEBUG
	FProgress Progress(Shell.GetFaces().Num(), TEXT("Display Shell"));
	for (const FOrientedFace& Face : Shell.GetFaces())
	{
		if (Face.Entity->IsDeleted())
		{
			continue;
		}
		Display(*Face.Entity);
	}
#endif
}

void Draw(const FShell& Shell)
{
#ifdef CADKERNEL_DEBUG
	FProgress Progress(Shell.GetFaces().Num(), TEXT("Display Shell"));
	for (const FOrientedFace& Face : Shell.GetFaces())
	{
		if (Face.Entity->IsDeleted())
		{
			continue;
		}
		F3DDebugSegment _(Face.Entity->GetId());
		Draw(*Face.Entity);
	}
#endif
}

void Display(const FBody& Body)
{
#ifdef CADKERNEL_DEBUG
	FProgress Progress(Body.GetShells().Num(), TEXT("Display Body"));

	F3DDebugSegment GraphicSegment(Body.GetId());
	for (const TSharedPtr<FShell>& Shell : Body.GetShells())
	{
		if (!Shell.IsValid() || Shell->IsDeleted())
		{
			continue;
		}
		Draw(*Shell);
	}
#endif
}

void Display(const FSurface& Surface)
{
#ifdef CADKERNEL_DEBUG
	FProgress Progress(TEXT("Display Surface"));

	F3DDebugSegment GraphicSegment(Surface.GetId());

	int32 IsoUNum = FSystem::Get().GetVisu()->GetParameters()->IsoUNumber;
	int32 IsoVNum = FSystem::Get().GetVisu()->GetParameters()->IsoVNumber;

	const double VisuSag = FSystem::Get().GetVisu()->GetParameters()->ChordError;

	FPolyline3D Polyline;
	FIsoCurve3DSamplerOnChord Sampler(Surface, VisuSag, Polyline);

	TFunction <void(const int32, const EIso)> DrawIsos = [&](int32 IsoCount, const EIso IsoType)
	{
		const FLinearBoundary& CurveBounds = Surface.GetBoundary().Get(IsoType == EIso::IsoU ? EIso::IsoV : EIso::IsoU);
		const FLinearBoundary& Bounds = Surface.GetBoundary().Get(IsoType);

		double Coordinate = Bounds.Min;
		IsoCount++;
		const double Step = (Bounds.Max - Bounds.Min) / IsoCount;

		for (int32 Index = 0; Index <= IsoCount; Index++)
		{
			Polyline.Empty();
			Sampler.Set(IsoType, Coordinate, CurveBounds);
			Sampler.Sample();

			Draw(Polyline.GetPoints(), EVisuProperty::Iso);
			Coordinate += Step;
		}
	};

	DrawIsos(IsoVNum, EIso::IsoV);
	DrawIsos(IsoUNum, EIso::IsoU);
#endif
}

void Display2D(const FSurface& Surface)
{
#ifdef CADKERNEL_DEBUG
	double StepU, StepV;

	F3DDebugSegment G(Surface.GetId());

	int32 IsoUCount = FSystem::Get().GetVisu()->GetParameters()->IsoUNumber;
	int32 IsoVCount = FSystem::Get().GetVisu()->GetParameters()->IsoVNumber;
	FSurfacicBoundary Bounds = Surface.GetBoundary();

	StepU = (Bounds[EIso::IsoU].Max - Bounds[EIso::IsoU].Min) / (IsoUCount + 1);
	StepV = (Bounds[EIso::IsoV].Max - Bounds[EIso::IsoV].Min) / (IsoVCount + 1);

	for (int32 iIso = 0; iIso <= (IsoUCount + 1); iIso++)
	{
		FPoint2D StartPoint(Bounds[EIso::IsoU].Min + (iIso)*StepU, Bounds[EIso::IsoV].Min);
		FPoint2D EndPoint(Bounds[EIso::IsoU].Min + (iIso)*StepU, Bounds[EIso::IsoV].Max);
		DrawSegment(StartPoint, EndPoint, EVisuProperty::Iso);
	}

	for (int32 iIso = 0; iIso <= (IsoVCount + 1); iIso++)
	{
		FPoint2D StartPoint(Bounds[EIso::IsoU].Min, Bounds[EIso::IsoV].Min + (iIso)*StepV);
		FPoint2D EndPoint(Bounds[EIso::IsoU].Max, Bounds[EIso::IsoV].Min + (iIso)*StepV);
		DrawSegment(StartPoint, EndPoint, EVisuProperty::Iso);
	}
#endif
}

void DisplayIsoCurve(const FSurface& Surface, double Coordinate, EIso IsoType)
{
#ifdef CADKERNEL_DEBUG
	const double VisuSag = FSystem::Get().GetVisu()->GetParameters()->ChordError;

	F3DDebugSegment G(Surface.GetId());

	const FLinearBoundary& CurveBounds = Surface.GetBoundary().Get(IsoType == EIso::IsoU ? EIso::IsoV : EIso::IsoU);

	FPolyline3D Polyline;
	FIsoCurve3DSamplerOnChord Sampler(Surface, VisuSag, Polyline);
	Sampler.Set(IsoType, Coordinate, CurveBounds);
	Draw(Polyline.GetPoints(), EVisuProperty::Iso);
#endif
}



void DisplayControlPolygon(const FCurve& Curve)
{
#ifdef CADKERNEL_DEBUG
	TFunction<void(const TArray<FPoint>&)> DisplayHull = [](const TArray<FPoint>& Poles)
	{
		for (const FPoint& Pole : Poles)
		{
			DisplayPoint(Pole, EVisuProperty::GreenPoint);
		}

		for (int32 Index = 1; Index < Poles.Num(); Index++)
		{
			FPoint Segment = Poles[Index] - Poles[Index - 1];
			DisplaySegment(Poles[Index - 1] + Segment * 0.1, Poles[Index] - Segment * 0.1, 0, EVisuProperty::GreenCurve);
		}
	};

	F3DDebugSegment GraphicSegment(Curve.GetId());
	switch (Curve.GetCurveType())
	{
	case  ECurve::Bezier:
	{
		const FBezierCurve& Bezier = (const FBezierCurve&)Curve;
		DisplayHull(Bezier.GetPoles());
		return;
	}
	case ECurve::Nurbs:
	{
		const FNURBSCurve& Nurbs = (const FNURBSCurve&)Curve;
		const TArray<FPoint>& Poles = Nurbs.GetPoles();
		DisplayHull(Poles);
		return;
	}
	case ECurve::Spline:
	{
		const FSplineCurve& Spline = (const FSplineCurve&)Curve;
		const FInterpCurveFPoint& Poles = Spline.GetSplinePointsPosition();

		for (const auto& Pole : Poles.Points)
		{
			DisplayPoint(Pole.OutVal, EVisuProperty::BluePoint);
		}

		for (const auto& Pole : Poles.Points)
		{
			DisplaySegment(Pole.OutVal - Pole.ArriveTangent / 2., Pole.OutVal, 0, EVisuProperty::GreenCurve);
			DisplaySegment(Pole.OutVal, Pole.OutVal + Pole.LeaveTangent / 2., 0, EVisuProperty::GreenCurve);
		}
		return;
	}

	}

#endif
}

void DisplayControlPolygon(const FSurface& Surface)
{
#ifdef CADKERNEL_DEBUG
	bool bShowOrientation = FSystem::Get().GetVisu()->GetParameters()->bDisplayCADOrient;

	TFunction<void(const TArray<FPoint>&, int32, int32)> DisplayHull = [&bShowOrientation](const TArray<FPoint>& Poles, int32 PoleUNum, int32 PoleVNum)
	{
		for (int32 Index = 0; Index < Poles.Num(); Index++)
		{
			DisplayPoint(Poles[Index], EVisuProperty::GreenPoint);
		}

		for (int32 IndexV = 0, Index = 0; IndexV < PoleVNum; IndexV++)
		{
			Index = Index + 1;
			for (int32 IndexU = 1; IndexU < PoleUNum; IndexU++, Index++)
			{
				FPoint Segment = Poles[Index] - Poles[Index - 1];
				DisplaySegment(Poles[Index - 1] + Segment * 0.1, Poles[Index] - Segment * 0.1, 0, EVisuProperty::YellowCurve);
				if (bShowOrientation)
				{
					DrawSegmentOrientation(Poles[Index - 1], Poles[Index], EVisuProperty::YellowCurve);
				}
			}
		}

		for (int32 IndexU = 0; IndexU < PoleUNum; IndexU++)
		{
			for (int32 IndexV = 1, Index = PoleUNum + IndexU; IndexV < PoleVNum; IndexV++, Index += PoleUNum)
			{
				FPoint Segment = Poles[Index] - Poles[Index - PoleUNum];
				DisplaySegment(Poles[Index - PoleUNum] + Segment * 0.1, Poles[Index] - Segment * 0.1, 0, EVisuProperty::GreenCurve);
				if (bShowOrientation)
				{
					DrawSegmentOrientation(Poles[Index - PoleUNum], Poles[Index], EVisuProperty::GreenCurve);
				}
			}
		}
	};

	F3DDebugSegment GraphicSegment(Surface.GetId());
	if (Surface.GetSurfaceType() == ESurface::Bezier)
	{
		const FBezierSurface& Bezier = (const FBezierSurface&)Surface;
		DisplayHull(Bezier.GetPoles(), Bezier.GetUDegre() + 1, Bezier.GetVDegre() + 1);
		return;
	}
	if (Surface.GetSurfaceType() == ESurface::Nurbs)
	{
		const FNURBSSurface& Nurbs = (const FNURBSSurface&)Surface;
		DisplayHull(Nurbs.GetPoles(), Nurbs.GetPoleCount(EIso::IsoU), Nurbs.GetPoleCount(EIso::IsoV));
		return;
	}
#endif
}

void Display(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Face.GetId());
	Draw(Face);
#endif
}

void Display2D(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Face.GetId());
	Draw2D(Face);
#endif
}

void Display2DWithScale(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Face.GetId());
	Draw2D(Face);
#endif
}

void Draw(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	{
		for (const TSharedPtr<FTopologicalLoop>& Loop : Face.GetLoops())
		{
			for (const FOrientedEdge& Edge : Loop->GetEdges())
			{
				EVisuProperty Property = EVisuProperty::BlueCurve;
				switch (Edge.Entity->GetTwinEntityCount())
				{
				case 1:
					Property =  Edge.Entity->IsDegenerated() ? EVisuProperty::OrangeCurve : EVisuProperty::BorderEdge;
					break;
				case 2:
					Property = EVisuProperty::BlueCurve;
					break;
				default:
					Property = EVisuProperty::NonManifoldEdge;
				}
				Draw(*Edge.Entity, Property);
			}
		}
	}

	DrawIsoCurves(Face);
#endif
}

void Draw2D(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	TArray<TArray<FPoint2D>> BoundaryApproximation;
	Face.Get2DLoopSampling(BoundaryApproximation);

	for (const TArray<FPoint2D>& Boudary : BoundaryApproximation)
	{
		Draw(Boudary, EVisuProperty::BlueCurve);
	}

	TFunction <void(const int32, const EIso)> DrawIsos = [&](int32 IsoCount, const EIso IsoType)
	{
		EIso OtherIso = IsoType == EIso::IsoU ? EIso::IsoV : EIso::IsoU;
		const FLinearBoundary& Bounds = Face.GetBoundary().Get(IsoType);
		const FLinearBoundary& CurveBounds = Face.GetBoundary().Get(OtherIso);


		double Coordinate = Bounds.Min;
		IsoCount++;
		const double Step = (Bounds.Max - Bounds.Min) / IsoCount;

		for (int32 iIso = 1; iIso < IsoCount; iIso++)
		{
			Coordinate += Step;

			TArray<double> Intersections;
			FindLoopIntersectionsWithIso(IsoType, Coordinate, BoundaryApproximation, Intersections);

			FPoint2D Start;
			FPoint2D End;

			Start[IsoType] = Coordinate;
			End[IsoType] = Coordinate;

			if (Intersections.Num() % 2 != 0)
			{
				Start[OtherIso] = Intersections[Intersections.Num() - 1];
				End[OtherIso] = CurveBounds.GetMax();
				DrawSegment(Start, End, EVisuProperty::YellowCurve);
				Intersections.Pop();
			}

			for (int32 ISection = 0; ISection < Intersections.Num(); ISection += 2)
			{
				Start[OtherIso] = Intersections[ISection];
				End[OtherIso] = Intersections[ISection + 1];
				DrawSegment(Start, End, EVisuProperty::Iso);
			}
		}
	};

	int32 IsoUCount = FSystem::Get().GetVisu()->GetParameters()->IsoUNumber;
	int32 IsoVCount = FSystem::Get().GetVisu()->GetParameters()->IsoVNumber;

	DrawIsos(IsoUCount, EIso::IsoU);
	DrawIsos(IsoVCount, EIso::IsoV);
#endif
}

void DrawIsoCurves(const FTopologicalFace& Face)
{
#ifdef CADKERNEL_DEBUG
	TArray<TArray<FPoint2D>> BoundaryApproximation;
	Face.Get2DLoopSampling(BoundaryApproximation);

	const double VisuSag = FSystem::Get().GetVisu()->GetParameters()->ChordError;

	FPolyline3D Polyline;
	const FSurface& Surface = Face.GetCarrierSurface().Get();
	FIsoCurve3DSamplerOnChord Sampler(Surface, VisuSag, Polyline);

	TFunction <void(const int32, const EIso)> DrawIsos = [&](int32 IsoCount, const EIso IsoType)
	{
		const FLinearBoundary& Bounds = Face.GetBoundary().Get(IsoType);

		EIso Other = IsoType == EIso::IsoU ? EIso::IsoV : EIso::IsoU;

		double Coordinate = Bounds.Min;
		IsoCount++;
		const double Step = (Bounds.Max - Bounds.Min) / IsoCount;

		for (int32 iIso = 1; iIso < IsoCount; iIso++)
		{
			Coordinate += Step;

			TArray<double> Intersections;
			FindLoopIntersectionsWithIso(IsoType, Coordinate, BoundaryApproximation, Intersections);
			int32 IntersectionCount = Intersections.Num();
			if (IntersectionCount == 0)
			{
				return;
			}

			FLinearBoundary CurveBounds(Intersections[0], Intersections.Last());

			Polyline.Empty();
			Sampler.Set(IsoType, Coordinate, CurveBounds);
			Sampler.Sample();

			if (IntersectionCount % 2 != 0)
			{
				TArray<FPoint> SubPolyline;
				FLinearBoundary Boundary(Intersections[IntersectionCount - 1], CurveBounds.GetMax());
				Polyline.GetSubPolyline(Boundary, EOrientation::Front, SubPolyline);
				Draw(SubPolyline, EVisuProperty::YellowCurve);
				Intersections.Pop();
				IntersectionCount--;
			}

			for (int32 ISection = 0; ISection < IntersectionCount; ISection += 2)
			{
				TArray<FPoint> SubPolyline;
				FLinearBoundary Boundary(Intersections[ISection], Intersections[ISection + 1]);
				Polyline.GetSubPolyline(Boundary, EOrientation::Front, SubPolyline);
				Draw(SubPolyline, EVisuProperty::Iso);
			}
		}
	};

	int32 IsoUCount = FSystem::Get().GetVisu()->GetParameters()->IsoUNumber;
	int32 IsoVCount = FSystem::Get().GetVisu()->GetParameters()->IsoVNumber;

	DrawIsos(IsoUCount, EIso::IsoU);
	DrawIsos(IsoVCount, EIso::IsoV);
#endif
}

void Display(const FTopologicalEdge& Edge, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Edge.GetId());
	Draw(Edge, Property);
#endif
}

void Display2D(const FTopologicalEdge& Edge, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Edge.GetId());
	TArray<FPoint2D> Polyline;
	Edge.GetCurve()->GetDiscretizationPoints(Edge.GetBoundary(), EOrientation::Front, Polyline);
	DisplayPolyline(Polyline, Property);
#endif
}

void Display2DWithScale(const FTopologicalEdge& Edge, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment GraphicSegment(Edge.GetId());
	TArray<FPoint2D> Polyline;
	Edge.GetCurve()->GetDiscretizationPoints(Edge.GetBoundary(), EOrientation::Front, Polyline);
	DisplayPolylineWithScale(Polyline, Property);
#endif
}

void Draw(const FTopologicalEdge& Edge, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	const FLinearBoundary& Boundary = Edge.GetBoundary();
	Draw(Boundary, *Edge.GetCurve(), Property);
#endif
}

void Display(const FModel& Model)
{
#ifdef CADKERNEL_DEBUG
	FProgress MainProgress(2, TEXT("Display model"));
	{
		const TArray<TSharedPtr<FBody>>& Bodies = Model.GetBodies();
		FProgress BodyProgress((int32)Bodies.Num(), TEXT("Bodies"));
		for (const TSharedPtr<FBody>& Body : Bodies)
		{
			Display(*Body);
		}
	}

#endif
}

void DisplayProductTree(const FModel& Model)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %d"), Model.GetTypeName(), Model.GetId()), { Model.GetId() });

	FProgress MainProgress(2, TEXT("Display Model"));
	{
		const TArray<TSharedPtr<FBody>>& Bodies = Model.GetBodies();
		FProgress BodyProgress((int32)Bodies.Num(), TEXT("Bodies"));
		for (const TSharedPtr<FBody>& Body : Bodies)
		{
			DisplayProductTree(*Body);
		}
	}

#endif
}

void DisplayProductTree(const FBody& Body)
{
#ifdef CADKERNEL_DEBUG
#ifdef CORETECHBRIDGE_DEBUG
	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %s KioId: %d Id: %d"), Body.GetTypeName(), Body.GetName(), Body.GetKioId(), Body.GetId()), { Body.GetId() });
#else
	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %s Id: %d"), Body.GetTypeName(), Body.GetName(), Body.GetId()), { Body.GetId() });
#endif
	FProgress Progress(Body.GetShells().Num(), TEXT("Display Body"));

	for (TSharedPtr<FShell> Shell : Body.GetShells())
	{
		if (!Shell.IsValid())
		{
			continue;
		}
		DisplayProductTree(*Shell);
	}
#endif
}

void DisplayProductTree(const FShell& Shell)
{
#ifdef CADKERNEL_DEBUG
#ifdef CORETECHBRIDGE_DEBUG
	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %s KioId: %d Id: %d"), Shell.GetTypeName(), Shell.GetName(), Shell.GetKioId(), Shell.GetId()), { Shell.GetId() });
#else
	F3DDebugSession GraphicSession(FString::Printf(TEXT("%s %s Id: %d"), Shell.GetTypeName(), Shell.GetName(), Shell.GetId()), { Shell.GetId() });
#endif
	Draw(Shell);
#endif
}

void DisplayProductTree(const FEntity& Entity)
{
#ifdef CADKERNEL_DEBUG
	FTimePoint StartTime = FChrono::Now();

	switch (Entity.GetEntityType())
	{
	case EEntity::TopologicalFace:
		Display((const FTopologicalFace&)Entity);
		break;
	case EEntity::Shell:
		DisplayProductTree((const FShell&)Entity);
		break;
	case EEntity::Body:
		DisplayProductTree((const FBody&)Entity);
		break;
	case EEntity::Model:
		DisplayProductTree((const FModel&)Entity);
		break;
	default:
		FMessage::Printf(Log, TEXT("Unable to display Entity of type %s"), FEntity::GetTypeName(Entity.GetEntityType()));
	}

	FDuration DisplayDuration = FChrono::Elapse(StartTime);
	FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Display "), DisplayDuration);
#endif
}

void Display(const FGroup& Group)
{
#ifdef CADKERNEL_DEBUG
	TArray<TSharedPtr<FEntity>> Entities;

	Group.GetValidEntities(Entities);

	FProgress Progress(Entities.Num());
	for (const TSharedPtr<FEntity>& Entity : Entities)
	{
		DisplayEntity(*Entity);
	}
#endif
}

void DisplayMesh(const FModel& Model)
{
	for (const TSharedPtr<FBody>& Body : Model.GetBodies())
	{
		if(Body)
		{
			DisplayMesh(*Body);
		}
	}
}

void DisplayMesh(const FBody& Body)
{
	for (const TSharedPtr<FShell>& Shell : Body.GetShells())
	{
		if (Shell)
		{
			DisplayMesh(*Shell);
		}
	}
}

void DisplayMesh(const FShell& Shell)
{
	for (const FOrientedFace& OrientedFace : Shell.GetFaces())
	{
		if (OrientedFace.Entity)
		{
			FTopologicalFace& Face = *OrientedFace.Entity;
			if (!Face.IsDeletedOrDegenerated() && Face.IsMeshed())
			{
				const FFaceMesh* Mesh = Face.GetMesh();
				if(Mesh)
				{
					DisplayMesh(*Mesh);
				}
			}
		}
	}
}

void DisplayMesh(const FFaceMesh& Mesh)
{
#ifdef CADKERNEL_DEBUG
	TMap<int32, const FPoint*> NodeIdToCoordinates;
	Mesh.GetNodeIdToCoordinates(NodeIdToCoordinates);

	TArray<FPoint> Points;
	Points.SetNum(3);
	double EdgeMeanLength = 0;
	const TArray<int32>& TriangleIndices = Mesh.TrianglesVerticesIndex;
	const TArray<int32>& VertexIndices = Mesh.VerticesGlobalIndex;
	for (int32 Index = 0; Index < TriangleIndices.Num();)
	{
		const FPoint** P0 = NodeIdToCoordinates.Find(VertexIndices[TriangleIndices[Index++]]);
		const FPoint** P1 = NodeIdToCoordinates.Find(VertexIndices[TriangleIndices[Index++]]);
		const FPoint** P2 = NodeIdToCoordinates.Find(VertexIndices[TriangleIndices[Index++]]);

		if (!P0 || !P1 || !P2)
		{
			continue;
		}

		Points[0] = **P0;
		Points[1] = **P1;
		Points[2] = **P2;
		DrawElement(2, Points, EVisuProperty::Element);
		DrawSegment(Points[0], Points[1], EVisuProperty::EdgeMesh);
		DrawSegment(Points[1], Points[2], EVisuProperty::EdgeMesh);
		DrawSegment(Points[2], Points[0], EVisuProperty::EdgeMesh);
		EdgeMeanLength += Points[0].Distance(Points[1]);
		EdgeMeanLength += Points[1].Distance(Points[2]);
		EdgeMeanLength += Points[2].Distance(Points[0]);
	}
	EdgeMeanLength /= 10 * TriangleIndices.Num();

	for (const int32& Index : VertexIndices)
	{
		const FPoint** PointPtr = NodeIdToCoordinates.Find(Index);
		if (!PointPtr)
		{
			continue;
		}
		F3DDebugSegment GraphicSegment(Index);
		DrawPoint(**PointPtr, EVisuProperty::NodeMesh);
	}

	bool test = FSystem::Get().GetVisu()->GetParameters()->bDisplayNormals;
	if (FSystem::Get().GetVisu()->GetParameters()->bDisplayNormals)
	{
		double NormalLength = FSystem::Get().GetVisu()->GetParameters()->NormalLength;
		const TArray<FVector3f>& Normals = Mesh.Normals;
		for (int32 Index = 0; Index < VertexIndices.Num(); ++Index)
		{
			const FPoint** PointPtr = NodeIdToCoordinates.Find(VertexIndices[Index]);
			if (!PointPtr)
			{
				continue;
			}

			F3DDebugSegment GraphicSegment(Index);
			FVector3f Normal = Normals[Index];
			Normal.Normalize();
			Normal *= NormalLength;
			DrawSegment(**PointPtr, **PointPtr + Normal, EVisuProperty::EdgeMesh);
		}
	}
#endif
}

void DisplayMesh(const FEdgeMesh& Mesh)
{
#ifdef CADKERNEL_DEBUG
	const FModelMesh& MeshModel = Mesh.GetMeshModel();

	const TArray<int32>& NodeIds = Mesh.EdgeVerticesIndex;
	const TArray<FPoint>& NodeCoordinates = Mesh.GetNodeCoordinates();

	int32 StartNodeId = NodeIds[0];
	int32 LastNodeId = NodeIds.Last();

	{
		F3DDebugSegment GraphicSegment(Mesh.GetGeometricEntity().GetId());
		if (NodeCoordinates.Num() == 0)
		{
			DrawSegment(MeshModel.GetMeshOfVertexNodeId(StartNodeId)->GetNodeCoordinates()[0], MeshModel.GetMeshOfVertexNodeId(LastNodeId)->GetNodeCoordinates()[0], EVisuProperty::EdgeMesh);
		}
		else
		{
			DrawSegment(MeshModel.GetMeshOfVertexNodeId(StartNodeId)->GetNodeCoordinates()[0], NodeCoordinates[0], EVisuProperty::EdgeMesh);
			if (NodeCoordinates.Num() > 1)
			{
				for (int32 Index = 0; Index < NodeCoordinates.Num() - 1; Index++)
				{
					DrawSegment(NodeCoordinates[Index], NodeCoordinates[Index + 1], EVisuProperty::EdgeMesh);
				}
			}
			DrawSegment(NodeCoordinates.Last(), MeshModel.GetMeshOfVertexNodeId(LastNodeId)->GetNodeCoordinates()[0], EVisuProperty::EdgeMesh);
		}
	}

	{
		F3DDebugSegment GraphicSegment(StartNodeId);
		DrawPoint(MeshModel.GetMeshOfVertexNodeId(StartNodeId)->GetNodeCoordinates()[0], EVisuProperty::NodeMesh);
	}

	{
		F3DDebugSegment GraphicSegment(LastNodeId);
		DrawPoint(MeshModel.GetMeshOfVertexNodeId(LastNodeId)->GetNodeCoordinates()[0], EVisuProperty::NodeMesh);
	}

	if (NodeCoordinates.Num() > 1)
	{
		for (int32 Index = 0; Index < NodeCoordinates.Num(); Index++)
		{
			F3DDebugSegment GraphicSegment(NodeIds[Index]);
			DrawPoint(NodeCoordinates[Index], EVisuProperty::NodeMesh);
		}
	}
#endif
}

void DisplayMesh(const FVertexMesh& Mesh)
{
#ifdef CADKERNEL_DEBUG
	DisplayPoint(Mesh.GetNodeCoordinates()[0], EVisuProperty::NodeMesh, Mesh.GetId());
#endif
}

void Display(const FModelMesh& MeshModel)
{
#ifdef CADKERNEL_DEBUG
	for (const FFaceMesh* Mesh : MeshModel.GetFaceMeshes())
	{
		if (Mesh)
		{
			DisplayMesh(*Mesh);
		}
	}
#endif
}

void DisplaySegment(const FPoint& Point1, const FPoint& Point2, FIdent Ident, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	DrawSegment(Point1, Point2, Property);
#endif
}

void DisplaySegment(const FPoint2D& Point1, const FPoint2D& Point2, FIdent Ident, EVisuProperty Property, bool bWithOrientation)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	if (bWithOrientation)
	{
		DrawSegmentOrientation(Point1, Point2, Property);
	}
	DrawSegment(Point1, Point2, Property);
#endif
}

void DisplaySegment(const FPointH& Point1, const FPointH& Point2, FIdent Ident, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	DrawSegment(Point1, Point2, Property);
#endif
}

void DisplayLoop(const FTopologicalFace& Surface)
{
#ifdef CADKERNEL_DEBUG
	for (const TSharedPtr<FTopologicalLoop>& Loop : Surface.GetLoops())
	{
		Display(*Loop);
	}
#endif
}


void Display(const FTopologicalLoop& Loop)
{
#ifdef CADKERNEL_DEBUG
	for (const FOrientedEdge& Edge : Loop.GetEdges())
	{
		Display(*Edge.Entity);
	}
#endif
}

void Display2D(const FTopologicalLoop& Loop)
{
#ifdef CADKERNEL_DEBUG
	for (const FOrientedEdge& Edge : Loop.GetEdges())
	{
		Display2D(*Edge.Entity);
	}
#endif
}

void Display2DWithScale(const FTopologicalLoop& Loop)
{
#ifdef CADKERNEL_DEBUG
	for (const FOrientedEdge& Edge : Loop.GetEdges())
	{
		Display2DWithScale(*Edge.Entity);
	}
#endif
}

} // namespace UE::CADKernel
