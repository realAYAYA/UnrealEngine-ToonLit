// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Topo/FaceAnalyzer.h"

#include "CADKernel/Math/Point.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/Topo/TopologicalLoop.h"
#include "CADKernel/UI/Display.h"

namespace UE::CADKernel
{

#ifdef CADKERNEL_DEV
FIdent Topo::FEdgeSegment::LastId = 0;
#endif

void FFaceAnalyzer::FindClosedSegments(Topo::FThinFaceContext& Context)
{
#ifdef DEBUG_FIND_THIN_FACE
	F3DDebugSession _(TEXT("Closed Segment"));
#endif

	// For each segment, the nearest segment is search
	// If segment is from an inner loop, the nearest could not be from the same loop
	for (Topo::FEdgeSegment* Segment : Context.LoopSegments)
	{
		const FPoint& Point = Segment->GetMiddle();
		const FTopologicalEdge* Edge = Segment->GetEdge();

		//FPoint ClosedPoint;
		Topo::FEdgeSegment* ClosedSegment = nullptr;

		double MinSquareDistance = HUGE_VALUE;

		for (Topo::FEdgeSegment* Candidate : Context.LoopSegments)
		{
			// to avoid to define cylinder or cone as a thin surface
			if (Candidate->GetEdge()->IsLinkedTo(*Edge))
			{
				continue;
			}

			const FPoint& CenterPointCandidate = Candidate->GetMiddle();
			const double SquareDistanceToCenter = CenterPointCandidate.SquareDistance(Point);

			// if the distance of Point with the middle of candidate segment is biggest than MaxSpace, then the projection of the point cannot be smaller than the tolerance, 
			if (SquareDistanceToCenter > MinSquareDistance)
			{
				continue;
			}

			const FPoint& FirstPointCandidate = Candidate->GetExtemity(ELimit::Start);
			const FPoint& SecondPointCandidate = Candidate->GetExtemity(ELimit::End);

			double Coordinate;
			FPoint Projection = ProjectPointOnSegment(Point, FirstPointCandidate, SecondPointCandidate, Coordinate, true);

			double SquareDistance = Point.SquareDistance(Projection);
			if (SquareDistance > MinSquareDistance)
			{
				continue;
			}

			// check the angle between segments. As they are opposite, the cosAngle as to be close to -1
			double CosAngle = Segment->ComputeCosAngleOf(Candidate);
			if (CosAngle > -0.5) // Angle < 3Pi/4 (135 deg)
			{
				continue;
			}

			MinSquareDistance = SquareDistance;
			ClosedSegment = Candidate;
		}

		if (ClosedSegment)
		{
			Segment->SetClosedSegment(ClosedSegment, MinSquareDistance);

#ifdef DEBUG_FIND_THIN_FACE
			//F3DDebugSession _(TEXT("Segment"));
			DisplaySegment(Segment->GetExtemity(ELimit::Start), Segment->GetExtemity(ELimit::End), 0, EVisuProperty::BlueCurve);
			DisplaySegment(ClosedSegment->GetExtemity(ELimit::Start), ClosedSegment->GetExtemity(ELimit::End), 0, EVisuProperty::RedCurve);
			//Wait();
#endif
		}
	}
}


void FFaceAnalyzer::Analyze(Topo::FThinFaceContext& Context)
{
	const FTopologicalEdge* Edge = Context.LoopSegments[0]->GetEdge();
	double MaxSquareDistance = 0;
	double MedSquareDistance = 0;
	double EdgeLength = 0;

	TFunction<void()> SetEdgeMaxGap = [&]()
	{
		if (EdgeLength * 1.2 > Edge->Length())
		{
			MedSquareDistance /= EdgeLength;
		}
		else
		{
			MedSquareDistance = DOUBLE_BIG_NUMBER;
			MaxSquareDistance = DOUBLE_BIG_NUMBER;
		}

		if (MaxSquareDistance < 2 * SquareTolerance)
		{
			Context.EdgeSquareDistance.Add(MedSquareDistance);
			Context.EdgeMaxSquareDistance.Add(MaxSquareDistance);

			Context.MaxSquareDistance = FMath::Max(Context.MaxSquareDistance, MaxSquareDistance);
			Context.ThinSideEdgeLength += Edge->Length();
		}
		else
		{
			Context.EdgeSquareDistance.Add(-1.);
			Context.EdgeMaxSquareDistance.Add(-1.);
			Context.OppositSideEdgeLength += Edge->Length();
		}
		Context.ExternalLoopLength += Edge->Length();
	};

	Context.EdgeSquareDistance.Reserve(Context.Loop.EdgeCount());
	Context.EdgeMaxSquareDistance.Reserve(Context.Loop.EdgeCount());

	for (Topo::FEdgeSegment* Segment : Context.LoopSegments)
	{
		if (Segment->GetEdge() != Edge)
		{
			SetEdgeMaxGap();

			Edge = Segment->GetEdge();
			MaxSquareDistance = 0;
			MedSquareDistance = 0;
			EdgeLength = 0;
		}

		if (Segment->GetClosedSquareDistance() > 0)
		{
			MedSquareDistance += Segment->GetLength() * Segment->GetClosedSquareDistance();
			EdgeLength += Segment->GetLength();
			if (MaxSquareDistance < Segment->GetClosedSquareDistance())
			{
				MaxSquareDistance = Segment->GetClosedSquareDistance();
			}
		}
	}

	SetEdgeMaxGap();
}


void FFaceAnalyzer::BuildLoopSegments(Topo::FThinFaceContext& Context)
{
	double Length = 0;

	const TArray<FOrientedEdge>& Edges = Context.Loop.GetEdges();

	Context.LoopSegments.Empty();

	for (const FOrientedEdge& OrientedEdge : Edges)
	{
		const FTopologicalEdge* Edge = OrientedEdge.Entity.Get();
		TArray<double> Coordinates;
		TArray<FPoint> Points;
		Edge->GetCurve()->GetDiscretizationPoints(Edge->GetBoundary(), Coordinates, Points);

		if (OrientedEdge.Direction == EOrientation::Back)
		{
			Algo::Reverse(Coordinates);
			Algo::Reverse(Points);
		}
			
		int32 PointCount = Points.Num();

		Context.LoopSegments.Reserve(Context.LoopSegments.Num() + PointCount);

		for (int32 ISegment = 0; ISegment < PointCount - 1; ISegment++)
		{
			const int32 Index1 = ISegment;	
			const int32 Index2 = ISegment + 1;
			Topo::FEdgeSegment& CurrentSeg = Context.SegmentFatory.New();
			CurrentSeg.SetBoundarySegment(Edge, Coordinates[Index1], Coordinates[Index2], Points[Index1], Points[Index2]);
			Context.LoopSegments.Add(&CurrentSeg);
		}
	}
}

bool FFaceAnalyzer::IsThinFace(double& OutGapSize)
{
#ifdef DEBUG_THIN_FACE
	F3DDebugSession _(TEXT("Thin Surface"));
#endif

	Topo::FThinFaceContext Context(*Face.GetExternalLoop());

	FTimePoint StartTime = FChrono::Now();
	BuildLoopSegments(Context);
	Chronos.BuildLoopSegmentsTime = FChrono::Elapse(StartTime);

#ifdef CADKERNEL_DEV
	DisplayLoopSegments(Context);
#endif

	StartTime = FChrono::Now();
	FindClosedSegments(Context);
	Chronos.FindClosedSegmentTime = FChrono::Elapse(StartTime);
#ifdef CADKERNEL_DEV
	DisplayClosedSegments(Context);
#endif

	StartTime = FChrono::Now();
	Analyze(Context);
	Chronos.AnalyzeClosedSegmentTime = FChrono::Elapse(StartTime);

	if (Context.ThinSideEdgeLength > Context.OppositSideEdgeLength && Context.MaxSquareDistance < SquareTolerance )
	{
		OutGapSize = sqrt(Context.MaxSquareDistance);
		return true;
	}

	OutGapSize = DOUBLE_BIG_NUMBER;
	return false;
}

#ifdef CADKERNEL_DEV
void FFaceAnalyzer::DisplayLoopSegments(Topo::FThinFaceContext& Context)
{
#ifdef DEBUG_THIN_FACE
	const FTopologicalEdge* Edge = nullptr;
	Edge = Context.LoopSegments[0]->GetEdge();

	F3DDebugSession _(TEXT("BoundarySegment"));

#ifdef DEBUG_THIN_FACE_EDGE
	Open3DDebugSession(FString::Printf(TEXT("Edge %d\n"), Edge->GetId()));
#endif

	for (Topo::FEdgeSegment* EdgeSegment : Context.LoopSegments)
	{
		if (Edge != EdgeSegment->GetEdge())
		{
			Edge = EdgeSegment->GetEdge();
#ifdef DEBUG_THIN_FACE_EDGE
			Close3DDebugSession();
			Open3DDebugSession(FString::Printf(TEXT("Edge %d\n"), Edge->GetId()));
#endif
		}
		DisplaySegment(EdgeSegment->GetExtemity(ELimit::Start), EdgeSegment->GetExtemity(ELimit::End), EdgeSegment->GetId(), EVisuProperty::GreenCurve, true);
		DisplayPoint(EdgeSegment->GetExtemity(ELimit::Start), EVisuProperty::BluePoint);
	}
#ifdef DEBUG_THIN_FACE_EDGE
	Close3DDebugSession();
#endif
	//Wait();
#endif
}

void FFaceAnalyzer::DisplayClosedSegments(Topo::FThinFaceContext& Context)
{
#ifdef DEBUG_THIN_FACE
	F3DDebugSession _(TEXT("Closed Segment"));
	for (const Topo::FEdgeSegment* Segment : Context.LoopSegments)
	{
		F3DDebugSession _(TEXT("Closed"));
		if (Segment->GetClosedSegment())
		{
			DisplaySegment(Segment->GetExtemity(ELimit::Start), Segment->GetExtemity(ELimit::End), Segment->GetId(), EVisuProperty::BlueCurve);
			Topo::FEdgeSegment* Parallel = Segment->GetClosedSegment();
			DisplaySegment(Parallel->GetExtemity(ELimit::Start), Parallel->GetExtemity(ELimit::End), Parallel->GetId(), EVisuProperty::BlueCurve);

			double Coordinate;
			FPoint Projection = ProjectPointOnSegment(Segment->GetMiddle(), Parallel->GetExtemity(ELimit::Start), Parallel->GetExtemity(ELimit::End), Coordinate, true);

			DisplaySegment(Projection, Segment->GetMiddle(), Segment->GetId(), EVisuProperty::RedCurve);
		}
	}
	//Wait();
#endif
}
#endif
}

