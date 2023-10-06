// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/System.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/GeoEnum.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/UI/Visu.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/UI/DefineForDebug.h"
#endif

class FString;

namespace UE::CADKernel
{

class FAABB2D;
class FAABB;
class FBody;
class FCurve;
class FEdgeMesh;
class FEdgeMesh;
class FEntity;
class FFaceMesh;
class FFaceMesh;
class FGroup;
class FMesh;
class FModel;
class FModelMesh;
class FRestrictionCurve;
class FShell;
class FSurface;
class FTopologicalEdge;
class FTopologicalFace;
class FTopologicalLoop;
class FTopologicalVertex;
class FVertexMesh;
class FVertexMesh;
struct FLinearBoundary;

#ifdef CADKERNEL_DEBUG
void Wait(bool bMakeWait = true);
void Open3DDebugSession(FString name, const TArray<FIdent>& idList = TArray<FIdent>());
inline void Open3DDebugSession(bool bIsDisplayed, FString name, const TArray<FIdent>& idList = TArray<FIdent>())
{
	if (bIsDisplayed)
	{
		Open3DDebugSession(name, idList);
	}
};
void Close3DDebugSession(bool bIsDisplayed = true);
#else
inline void Wait(bool bMakeWait = true) {};
inline void Open3DDebugSession(bool bIsDisplayed, FString name, const TArray<FIdent>& idList = TArray<FIdent>()) {};
inline void Open3DDebugSession(FString name, const TArray<FIdent>& idList = TArray<FIdent>()) {};
inline void Close3DDebugSession(bool bIsDisplayed = true) {};
#endif

class CADKERNEL_API F3DDebugSession
{
private:
	bool bDisplay = true;

public:
	F3DDebugSession(FString Name, const TArray<FIdent>& Idents = TArray<FIdent>())
	{
#ifdef CADKERNEL_DEBUG
		Open3DDebugSession(Name, Idents);
#endif
	}

	F3DDebugSession(bool bInDisplay, FString Name, const TArray<FIdent>& Idents = TArray<FIdent>())
		: bDisplay(bInDisplay)
	{
#ifdef CADKERNEL_DEBUG
		if (bDisplay)
		{
			Open3DDebugSession(Name, Idents);
		}
#endif
	}

	~F3DDebugSession()
	{
#ifdef CADKERNEL_DEBUG
		if (bDisplay)
		{
			Close3DDebugSession();
		}
#endif
	}
};

CADKERNEL_API void Open3DDebugSegment(FIdent Ident);
CADKERNEL_API void Close3DDebugSegment();

class CADKERNEL_API F3DDebugSegment
{
public:
	F3DDebugSegment(FIdent Ident)
	{
#ifdef CADKERNEL_DEBUG
		Open3DDebugSegment(Ident);
#endif
	}

	~F3DDebugSegment()
	{
#ifdef CADKERNEL_DEBUG
		Close3DDebugSegment();
#endif
	}
};

CADKERNEL_API void FlushVisu();

template<typename TPoint>
void DrawPoint(const TPoint& InPoint, EVisuProperty Property = EVisuProperty::BluePoint)
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->DrawPoint(InPoint, Property);
#endif
}

/**
 * Draw a mesh element i.e. Element of dimension 1 is an edge, of dimension 2 is a triangle or quadrangle, of dimension 3 is tetrahedron, pyramid, hexahedron, ...
 */
CADKERNEL_API void DrawElement(int32 Dimension, TArray<FPoint>& Points, EVisuProperty Property = EVisuProperty::Element);

template<typename TPoint>
void Draw(const TArray<TPoint>& Points, EVisuProperty Property = EVisuProperty::BlueCurve)
{
#ifdef CADKERNEL_DEBUG
	FSystem::Get().GetVisu()->DrawPolyline(Points, Property);
#endif
}

CADKERNEL_API void DrawMesh(const TSharedPtr<FMesh>& mesh);

CADKERNEL_API void DisplayEdgeCriteriaGrid(int32 EdgeId, const TArray<FPoint>& Points3D);

template<typename TPoint>
void DisplayPoint(const TPoint& Point, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	DrawPoint(Point);
#endif
}

template<typename TPoint>
void DisplayPoint(const TPoint& Point, EVisuProperty Property = EVisuProperty::BluePoint)
{
#ifdef CADKERNEL_DEBUG
	DrawPoint(Point, Property);
#endif
}

template<typename TPoint>
void DisplayPoint(const TPoint& Point, EVisuProperty Property, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	DrawPoint(Point, Property);
#endif
}

static void DisplayPoint2DWithScale(const FPoint2D& Point, EVisuProperty Property = EVisuProperty::BluePoint)
{
#ifdef CADKERNEL_DEBUG
	DrawPoint(Point * DisplayScale, Property);
#endif
}

static void DisplayPoint2DWithScale(const FPoint2D& Point, EVisuProperty Property, FIdent Ident)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	DrawPoint(Point * DisplayScale, Property);
#endif
}

template<typename TPoint>
void DisplayPoints(FString Message, const TArray<TPoint>& Points, EVisuProperty Property = EVisuProperty::BluePoint, bool bDisplay = true)
{
#ifdef CADKERNEL_DEBUG
	if (!bDisplay)
	{
		return;
	}

	Open3DDebugSession(Message);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		DisplayPoint(Points[Index], Property);
	}
	Close3DDebugSession();
#endif
}

CADKERNEL_API void DisplayProductTree(const FEntity& RootId);
CADKERNEL_API void DisplayProductTree(const FModel& Model);
CADKERNEL_API void DisplayProductTree(const FBody& Body);
CADKERNEL_API void DisplayProductTree(const FShell& Shell);

CADKERNEL_API void DisplayAABB(const FAABB& aabb, FIdent Ident = 0);
CADKERNEL_API void DisplayAABB2D(const FAABB2D& aabb, FIdent Ident = 0);

CADKERNEL_API void DisplayEntity(const FEntity& Entity);
CADKERNEL_API void DisplayEntity2D(const FEntity& Entity);

CADKERNEL_API void DisplayLoop(const FTopologicalFace& Entity);
CADKERNEL_API void DisplayIsoCurve(const FSurface& CarrierSurface, double Coordinate, EIso Type);

CADKERNEL_API void Display(const FPlane& plane, FIdent Ident = 0);

CADKERNEL_API void Display(const FCurve& Curve);
CADKERNEL_API void Display(const FSurface& CarrierSurface);

CADKERNEL_API void Display(const FGroup& Group);
CADKERNEL_API void Display(const FModel& Model);
CADKERNEL_API void Display(const FBody& Body);
CADKERNEL_API void Display(const FShell& Shell);
CADKERNEL_API void Display(const FTopologicalEdge& Edge, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Display(const FTopologicalFace& Face);
CADKERNEL_API void Display(const FTopologicalLoop& Loop);
CADKERNEL_API void Display(const FTopologicalVertex& Vertex, EVisuProperty Property = EVisuProperty::BluePoint);

CADKERNEL_API void Display2D(const FTopologicalEdge& Edge, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Display2D(const FTopologicalFace& Face);
CADKERNEL_API void Display2D(const FTopologicalLoop& Loop);
CADKERNEL_API void Display2D(const FSurface& CarrierSurface);

CADKERNEL_API void Display2DWithScale(const FTopologicalEdge& Edge, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Display2DWithScale(const FTopologicalFace& Face);
CADKERNEL_API void Display2DWithScale(const FTopologicalLoop& Loop);
CADKERNEL_API void Display2DWithScale(const FSurface& CarrierSurface);

CADKERNEL_API void DisplayMesh(const FFaceMesh& Mesh);
CADKERNEL_API void DisplayMesh(const FEdgeMesh& Mesh);
CADKERNEL_API void DisplayMesh(const FVertexMesh& Mesh);

CADKERNEL_API void DisplayMesh(const FModel& Model);
CADKERNEL_API void DisplayMesh(const FBody& Body);
CADKERNEL_API void DisplayMesh(const FShell &Shel);

CADKERNEL_API void Display(const FModelMesh& MeshModel);

CADKERNEL_API void DisplayControlPolygon(const FCurve& Entity);
CADKERNEL_API void DisplayControlPolygon(const FSurface& Entity);

template<typename TPoint>
void DisplaySegment(const TPoint& Point1, const TPoint& Point2, FIdent Ident = 0, EVisuProperty Property = EVisuProperty::BlueCurve, bool bWithOrientation = false)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	if (bWithOrientation)
	{
		DrawSegmentOrientation(Point1, Point2, Property);
	}
	DrawSegment(Point1, Point2, Property);
 #endif
};

template<typename TPoint>
void DisplaySegmentWithScale(const TPoint& Point1, const TPoint& Point2, FIdent Ident = 0, EVisuProperty Property = EVisuProperty::BlueCurve, bool bWithOrientation = false)
{
#ifdef CADKERNEL_DEBUG
	F3DDebugSegment G(Ident);
	if (bWithOrientation)
	{
		DrawSegmentOrientation(Point1 * DisplayScale, Point2 * DisplayScale, Property);
	}
	DrawSegment(Point1 * DisplayScale, Point2 * DisplayScale, Property);
#endif
};

template<typename TPoint>
void DisplayPolyline(const TArray<TPoint>& Points, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	Open3DDebugSegment(0);
	Draw(Points, Property);
	Close3DDebugSegment();
#endif
}

static void DisplayPolylineWithScale(const TArray<FPoint2D>& Points, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	TArray<FPoint2D> PointsWithScale = Points;
	for (FPoint2D& Point : PointsWithScale)
	{
		Point *= DisplayScale;
	}

	Open3DDebugSegment(0);
	Draw(PointsWithScale, Property);
	Close3DDebugSegment();
#endif
}

template<typename TPoint>
void DisplayOrientedPolyline(const TArray<TPoint>& Points, EVisuProperty Property)
{
#ifdef CADKERNEL_DEBUG
	Open3DDebugSegment(0);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		DisplaySegment(Points[Index - 1], Points[Index], Index, Property, true);
	}
	Close3DDebugSegment();
#endif
}

CADKERNEL_API void DrawQuadripode(double Height, double Base, FPoint& Centre, FPoint& Direction, EVisuProperty Property);

CADKERNEL_API void Draw(const FTopologicalEdge& Edge, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Draw(const FTopologicalFace& Face);
CADKERNEL_API void Draw2D(const FTopologicalFace& Face);
CADKERNEL_API void Draw2DWithScale(const FTopologicalFace& Face);
CADKERNEL_API void Draw(const FShell& Shell);

CADKERNEL_API void Draw(const FCurve& Curve, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Draw(const FCurve& Curve, const FLinearBoundary& Boundary, EVisuProperty Property = EVisuProperty::BlueCurve);
CADKERNEL_API void Draw(const FLinearBoundary& Boundary, const FRestrictionCurve& Curve, EVisuProperty Property = EVisuProperty::BlueCurve);

template<typename TPoint>
void DrawSegment(const TPoint& Point1, const TPoint& Point2, EVisuProperty Property = EVisuProperty::Element)
{
#ifdef CADKERNEL_DEBUG
	TArray<FPoint> Points;
	Points.Add(Point1);
	Points.Add(Point2);
	Draw(Points, Property);
#endif
}

template<typename TPoint>
void DrawSegmentOrientation(const TPoint& Point1, const TPoint& Point2, EVisuProperty Property = EVisuProperty::Element)
{
#ifdef CADKERNEL_DEBUG
	double Length = Point1.Distance(Point2);
	double Height = Length / 10.0;
	double Base = Height / 2;

	FPoint Middle = (Point1 + Point2) / 2.;
	FPoint Tangent = Point2 - Point1;
	DrawQuadripode(Height, Base, Middle, Tangent, Property);
#endif
}

CADKERNEL_API void DrawIsoCurves(const FTopologicalFace& Face);


} // namespace UE::CADKernel

