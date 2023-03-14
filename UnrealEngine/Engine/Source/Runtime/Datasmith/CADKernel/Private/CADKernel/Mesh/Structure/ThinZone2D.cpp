// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Structure/ThinZone2D.h"

#include "CADKernel/Math/Point.h"
#include "CADKernel/Mesh/Structure/Grid.h"
#include "CADKernel/Mesh/Structure/EdgeSegment.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/Topo/TopologicalLoop.h"
#include "CADKernel/UI/Display.h"

namespace UE::CADKernel
{

FIdent FEdgeSegment::LastId = 0;

void FThinZone2DFinder::FindClosedSegments()
{
	double MaxSegmentLength = 0.0;
	for (const FEdgeSegment* Segment : LoopSegments)
	{
		double Length = Segment->GetLength();
		if (Length > MaxSegmentLength) 
		{
			MaxSegmentLength = Length;
		}
	}
	MaxSegmentLength *= 1.01;

	double MaxSpace = FMath::Max(MaxSegmentLength, Tolerance * 1.01);
	double MaxSquareSpace = 4 * FMath::Square(MaxSpace);

	// Copy of loop segments to generate a sorted array of segments
	SortedLoopSegments = LoopSegments;
	Algo::Sort(SortedLoopSegments, [](FEdgeSegment* SegmentA, FEdgeSegment* SegmentB)
		{
			return (SegmentA->GetAxeMin() <SegmentB->GetAxeMin());
		});


#ifdef DEBUG_THIN_ZONES
	Open3DDebugSession(TEXT("Closed Segment");
#endif

	// For each segment, the nearest segment is search
	// If segment is from an inner loop, the nearest could not be from the same loop
	int32 SegmentToBeginIndex = 0;
	for (FEdgeSegment* Segment : SortedLoopSegments)
	{
		FTopologicalLoop* SegmentLoop = nullptr;
		if (Segment->IsInner())
		{
			SegmentLoop = Segment->GetEdge()->GetLoop();
		}
		else
		{
			SegmentLoop = nullptr;
		}

		FPoint2D SegmentMiddle = Segment->GetCenter();

		FPoint2D ClosedPoint, ClosedPoint2;
		FEdgeSegment* ClosedSegment = nullptr;

		double SegmentMin = Segment->GetAxeMin();
		double MinSquareThickness = HUGE_VALUE;

		for (int32 CandidateIndex = SegmentToBeginIndex; CandidateIndex <SortedLoopSegments.Num(); ++CandidateIndex)
		{
			FEdgeSegment* Candidate = SortedLoopSegments[CandidateIndex];
			if (Candidate == Segment) 
			{
				continue;
			}

			// Inner boundary are not check for thin inner boundary
			if (SegmentLoop == Candidate->GetEdge()->GetLoop()) 
			{
				continue;
			}

			double CandidateSegmentMin = Candidate->GetAxeMin();

			// If min point of candiate segment + maxSpace (= max length of the segments + Tolerance) is smaller than Min point of current segment then the distance between both segments cannot be smaller the Tolerance
			if ((CandidateSegmentMin + MaxSpace) < SegmentMin)
			{
				SegmentToBeginIndex = CandidateIndex;
				continue;
			}

			// If Min point of current segment + maxSpace is smaller than Min point of candidate segment then the distance between both segments cannot be smaller the Tolerance.
			// As segments are sorted, next segments are not close to the current segment
			if (CandidateSegmentMin > (SegmentMin + MaxSpace))
			{
				break;
			}

			const FPoint2D& FirstPointCandidate = Candidate->GetExtemity(ELimit::Start);

			// if the distance of FirstPointCandidate with the middle of current segment is biggest than MaxSpace, then the projection of the point cannot be smaller than the tolerance, 
			double SquareDistance = SegmentMiddle.SquareDistance(FirstPointCandidate);
			if (SquareDistance > MaxSquareSpace)
			{
				continue;
			}

			const FPoint2D& SecondPointCandidate = Candidate->GetExtemity(ELimit::End);
			double Coordinate;
			FPoint2D Projection = ProjectPointOnSegment(SegmentMiddle, FirstPointCandidate, SecondPointCandidate, Coordinate, true);

			SquareDistance = SegmentMiddle.SquareDistance(Projection);
			if (SquareDistance > SquareTolerance)
			{
				continue;
			}

			if (MinSquareThickness > SquareDistance)
			{
				// check the angle between segments. As they are opposite, the cosAngle as to be close to -1
				double Slop = Segment->ComputeUnorientedSlopeOf(Candidate);
				if (Slop < 3.33) // Angle <5Pi/6 (150 deg)
				{
					continue;
				}

				// check the angle between segment and Middle-Projection. As they are opposite, the cosAngle as to be close to 0
				Slop = Segment->ComputeUnorientedSlopeOf(SegmentMiddle, Projection);
				if (Slop < 1.33) // Angle <Pi/3 (60 deg)
				{
					continue;
				}

				MinSquareThickness = SquareDistance;
				ClosedPoint = Projection;
				ClosedSegment = Candidate;
			}
		}

		if (ClosedSegment)
		{
			Segment->SetClosedSegment(ClosedSegment, nullptr, MinSquareThickness, true);
#ifdef DEBUG_THIN_ZONES
			DisplaySegment(Segment->GetFirstPoint(), Segment->GetSecondPoint());
			DisplaySegment(ClosedSegment->GetFirstPoint(), ClosedSegment->GetSecondPoint());
			Wait();
#endif
		}
	}
#ifdef DEBUG_THIN_ZONES
	Close3DDebugSession();
#endif
}

void FThinZone2DFinder::DisplayClosedSegments()
{
#ifdef DEBUG_THIN_ZONES
	F3DDebugSession _(TEXT("Closed Segment"));
	for (const FEdgeSegment* Segment : LoopSegments)
	{
		if (Segment->GetClosedSegment())
		{
			DisplaySegment(Segment->GetFirstPoint(), Segment->GetSecondPoint(), Segment->GetId(), PropertyElement);
			FEdgeSegment* Parallel = Segment->GetClosedSegment();
			DisplaySegment(Parallel->GetFirstPoint(), Parallel->GetSecondPoint(), Segment->GetId(), PropertyElement);

			double Coordinate;
			FPoint Projection = ProjectPointOnSegment(Segment->GetCenter(), Parallel->GetFirstPoint(), Parallel->GetSecondPoint(), &Coordinate, true);

			DisplaySegment(Projection, Segment->GetCenter(), Segment->GetId(), EVisuProperty::Iso);
		}
	}
#endif
}

void FThinZone2DFinder::CheckClosedSegments()
{
	for (FEdgeSegment* Segment : LoopSegments)
	{
		if (Segment->GetClosedSegment())
		{
			double NextSegmentThickness = FMath::Min(Segment->GetPrevious()->GetClosedSquareDistance(), Segment->GetNext()->GetClosedSquareDistance());
			double SegmentThickness = Segment->GetClosedSquareDistance();
			GetMinMax(NextSegmentThickness, SegmentThickness);
			SegmentThickness = sqrt(SegmentThickness);
			NextSegmentThickness = sqrt(NextSegmentThickness);
			double Delta = SegmentThickness - NextSegmentThickness;
			if (Delta > (Segment->GetLength()))
			{
				Segment->ResetClosedData();
			}
		}
	}
}

void FThinZone2DFinder::LinkClosedSegments()
{
	SortedLoopSegments.Empty();
	SortedLoopSegments.Reserve(LoopSegments.Num());

	FEdgeSegment* StartSegment = nullptr;
	FEdgeSegment* Segment = nullptr;
	FIdent Index = 0;
	FIdent StartZoneIndex = 0;
	FTopologicalLoop* LastLoop = nullptr;
	bool bThinZone = false;

	for (FEdgeSegment* EdgeSegment : LoopSegments)
	{
		Segment = EdgeSegment;
		if (LastLoop != EdgeSegment->GetEdge()->GetLoop())
		{
			LastLoop = EdgeSegment->GetEdge()->GetLoop();
			Index++;
			StartSegment = EdgeSegment;
			StartZoneIndex = Index;
			bThinZone = false;
		}

		if (Segment->GetClosedSegment() != nullptr)
		{
			Segment->SetChainIndex(Index);
			SortedLoopSegments.Add(Segment);
			bThinZone = true;

			if (Segment->GetNext() == StartSegment)
			{
				FIdent currentZoneIndex = Segment->GetChainIndex();
				if (StartZoneIndex != 0 && currentZoneIndex != StartZoneIndex)
				{
					do {
						Segment->SetChainIndex(StartZoneIndex);
						Segment = Segment->GetPrevious();
					} while (Segment->GetChainIndex() == currentZoneIndex);
				}
			}
		}
		else
		{
			if (bThinZone)
			{
				Index++;
				bThinZone = false;
			}
		}
	}

	// the number of ThinZone should be less than 2x the number of Chain, 
	ThinZones.Reserve(Index * 2);
#ifdef DEBUG_THIN_ZONES
	{
		F3DDebugSession _(TEXT("Linking"));
		FIdent LastIndex = 0;
		int32 Color = 1;
		for (FEdgeSegment* EdgeSegment : SortedLoopSegments)
		{
			if (LastIndex != EdgeSegment->GetChainIndex())
			{
				if (Color > 1) Close3DDebugSession();
				Color++;
				LastIndex = EdgeSegment->GetChainIndex();
				Open3DDebugSession(FString::Printf(TEXT("Chain %d\n"), LastIndex));
				if (Color == 7) Color++;
				if (Color == 11) Color = 2;
			}
			DisplaySegment(EdgeSegment->GetExtemity(ELimit::Start), EdgeSegment->GetExtemity(ELimit::End), EdgeSegment->GetId(), (EVisuProperty)Color);
		}
		if (SortedLoopSegments.Num() > 0)
		{
			Close3DDebugSession();
		}
	}
#endif

}


static void GetThinZoneSideConnectionsLength(TArray<FEdgeSegment*>& FirstSide, TArray<FEdgeSegment*>& SecondSide, double MaxLength, double* LengthBetweenExtremity, TArray<TSharedPtr<FTopologicalEdge>>* PeakEdges)
{
	LengthBetweenExtremity[0] = 0.0;
	TSharedPtr<FTopologicalEdge> Edge = nullptr;
	FEdgeSegment* Segment = FirstSide[0]->GetPrevious();
	while (Segment != SecondSide[0])
	{
		if (Edge != Segment->GetEdge())
		{
			Edge = Segment->GetEdge();
			PeakEdges[0].Add(Edge);
		}
		LengthBetweenExtremity[0] += Segment->GetLength();
		Segment = Segment->GetPrevious();

		if (LengthBetweenExtremity[0] > MaxLength)
		{
			LengthBetweenExtremity[0] = HUGE_VALUE;
			PeakEdges[0].Empty(0);
			break;
		}
	}

	LengthBetweenExtremity[1] = 0.0;
	Edge = nullptr;
	Segment = FirstSide.Last()->GetNext();
	while (Segment != SecondSide.Last())
	{
		if (Edge != Segment->GetEdge())
		{
			Edge = Segment->GetEdge();
			PeakEdges[1].Add(Edge);
		}

		LengthBetweenExtremity[1] += Segment->GetLength();
		Segment = Segment->GetNext();

		if (LengthBetweenExtremity[1] > MaxLength)
		{
			LengthBetweenExtremity[1] = HUGE_VALUE;
			PeakEdges[1].Empty(0);
			break;
		}
	}
}

void FThinZone2DFinder::BuildThinZone()
{
	for (FEdgeSegment* EdgeSegment : SortedLoopSegments)
	{
		EdgeSegment->ResetMarker1();
	}

	TArray<FEdgeSegment*> FirstSide;
	TArray<FEdgeSegment*> SecondSide;
	FirstSide.Reserve(SortedLoopSegments.Num());
	SecondSide.Reserve(SortedLoopSegments.Num());

	FEdgeSegment* FirstSideSegment = nullptr;
	FEdgeSegment* SecondSideSegment = nullptr;

	for (FEdgeSegment* EdgeSegment : SortedLoopSegments)
	{
		bool bFirstSideIsClosed = false;
		bool bSecondSideIsClosed = false;

		FirstSide.Empty();
		SecondSide.Empty();

		if (EdgeSegment->HasMarker1()) 
		{
			continue;
		}

		FirstSideSegment = EdgeSegment;

		// Find the first segment
		FIdent FirstChainIndex = FirstSideSegment->GetChainIndex();
		FEdgeSegment* Segment1 = FirstSideSegment;
		while (Segment1->GetChainIndex() == FirstChainIndex && !Segment1->HasMarker1())
		{
			Segment1 = Segment1->GetPrevious();
			if (Segment1 == FirstSideSegment)
			{
				Segment1 = Segment1->GetPrevious();
				bFirstSideIsClosed = true;
				break;
			}
		}
		FirstSideSegment = Segment1->GetNext();

		// Find the parallel chain
		SecondSideSegment = FirstSideSegment->GetClosedSegment();
		if (SecondSideSegment == nullptr)
		{
			SecondSideSegment = FirstSideSegment->GetNext()->GetClosedSegment();
			if (SecondSideSegment == nullptr)
			{
				FirstSideSegment->HasMarker1();
				continue;
			}
		}
		FIdent SecondChainIndex = SecondSideSegment->GetChainIndex();
		if (SecondChainIndex == 0) continue;

		int32 SegmentNum = 0;
		FEdgeSegment* Segment2 = SecondSideSegment;
		if (!(bFirstSideIsClosed && (FirstChainIndex == SecondChainIndex)))
		{
			while (Segment2->GetChainIndex() == SecondChainIndex && !Segment2->HasMarker1() &&
				((Segment2->GetClosedSegment() == FirstSideSegment) || (Segment2->GetClosedSegment() == FirstSideSegment->GetNext())
					|| (FirstSideSegment->GetClosedSegment() == Segment2) || (FirstSideSegment->GetClosedSegment() == Segment2->GetPrevious())))
			{
				if (Segment2->HasMarker1())
				{
					break;
				}

				Segment2 = Segment2->GetNext();
				if (SegmentNum > 0 && Segment2->GetPrevious() == SecondSideSegment)
				{
					bSecondSideIsClosed = true;
					break;
				}
				SegmentNum++;
			}
			SecondSideSegment = Segment2->GetPrevious();
		}

		Segment1 = FirstSideSegment;
		Segment2 = SecondSideSegment;

		// both chains are browsed as long as they stay in front of each other
		double MaxSquareThickness = 0.0;
		do {
			if (!FirstSideSegment->HasMarker1())
			{
				FirstSide.Add(FirstSideSegment);
				FirstSideSegment->SetMarker1();
				if (MaxSquareThickness <FirstSideSegment->GetClosedSquareDistance()) 
				{
					MaxSquareThickness = FirstSideSegment->GetClosedSquareDistance();
				}
			}

			if (!SecondSideSegment->HasMarker1())
			{
				SecondSide.Add(SecondSideSegment);
				SecondSideSegment->SetMarker1();
				if (MaxSquareThickness <SecondSideSegment->GetClosedSquareDistance()) 
				{
					MaxSquareThickness = SecondSideSegment->GetClosedSquareDistance();
				}
			}

			if (Segment1 == FirstSideSegment) 
			{
				Segment1 = FirstSideSegment->GetNext();
			}
			if (Segment2 == SecondSideSegment) 
			{
				Segment2 = SecondSideSegment->GetPrevious();
			}

			if ((!Segment1->HasMarker1()) && (Segment1->GetChainIndex() == FirstChainIndex) && ((Segment1->GetClosedSegment() == SecondSideSegment->GetNext()) || (Segment1->GetClosedSegment() == SecondSideSegment)))
			{
				FirstSideSegment = Segment1;
			}
			else if ((!Segment2->HasMarker1()) && (Segment2->GetChainIndex() == SecondChainIndex) && ((Segment2->GetClosedSegment() == FirstSideSegment->GetPrevious()) || (Segment2->GetClosedSegment() == FirstSideSegment)))
			{
				SecondSideSegment = Segment2;
			}
			else if ((!Segment1->HasMarker1()) && (Segment1->GetChainIndex() == FirstChainIndex) && (Segment1->GetClosedSegment() == SecondSideSegment->GetPrevious()))
			{
				FirstSideSegment = Segment1;
			}
			else if ((!Segment2->HasMarker1()) && (Segment2->GetChainIndex() == SecondChainIndex) && (Segment2->GetClosedSegment() == FirstSideSegment->GetNext()))
			{
				SecondSideSegment = Segment2;
			}
		} 
		while ((FirstSideSegment == Segment1) || (SecondSideSegment == Segment2));

		if ((FirstSide.Num() == 0) || (SecondSide.Num() == 0))
		{
			continue;
		}

		// Length of each side
		double LengthSide1 = 0;
		double MediumThickness1 = 0;
		for (FEdgeSegment* Segment : FirstSide)
		{
			LengthSide1 += Segment->GetLength();
			MediumThickness1 += Segment->GetClosedSquareDistance();
		}
		MediumThickness1 /= FirstSide.Num();
		MediumThickness1 = sqrt(MediumThickness1);

		double LengthSide2 = 0;
		double MediumThickness2 = 0;
		for (FEdgeSegment* Segment : SecondSide)
		{
			LengthSide2 += Segment->GetLength();
			MediumThickness2 += Segment->GetClosedSquareDistance();
		}
		MediumThickness2 /= FirstSide.Num();
		MediumThickness2 = sqrt(MediumThickness2);


		if ((LengthSide1 <MediumThickness2 * 3) || (LengthSide2 <MediumThickness2 * 3))
		{
			continue;
		}

		const double MaxThickness = sqrt(MaxSquareThickness);
		const double MaxLengthBetweendSideToBePeak = 3 * MaxThickness;

		// make the thin zone
		FThinZone2D& Zone = ThinZones.Emplace_GetRef(FirstSide, bFirstSideIsClosed, SecondSide, bSecondSideIsClosed, MaxThickness);

		TFunction<void(TArray<TSharedPtr<FTopologicalEdge>>&)> DeleteCuttingPointOfPeak = [](const TArray<TSharedPtr<FTopologicalEdge>>& PeakEdges)
		{
			for (TSharedPtr<FTopologicalEdge> Edge : PeakEdges)
			{
				if (Edge->IsMeshed())
				{
					continue;
				}
				Edge->GetLinkActiveEdge()->SetThinPeak();
			}
		};

		if (!Zone.GetFirstSide().IsInner() && !Zone.GetSecondSide().IsInner())
		{
			double LengthBetweenExtremity[2] = { HUGE_VALUE, HUGE_VALUE };
			TArray<TSharedPtr<FTopologicalEdge>> PeakEdges[2];
			GetThinZoneSideConnectionsLength(FirstSide, SecondSide, 10 * MaxThickness, LengthBetweenExtremity, PeakEdges);

			if ((Zone.GetMaxLength() * 2 > (FirstLoopLength - MaxLengthBetweendSideToBePeak)) || (LengthBetweenExtremity[0] <MaxLengthBetweendSideToBePeak && LengthBetweenExtremity[1] <MaxLengthBetweendSideToBePeak))
			{
				Zone.SetCategory(EThinZone2DType::Global);
				DeleteCuttingPointOfPeak(PeakEdges[0]);
				DeleteCuttingPointOfPeak(PeakEdges[1]);
			}
			else if (LengthBetweenExtremity[0] <MaxLengthBetweendSideToBePeak)
			{
				Zone.SetCategory(EThinZone2DType::PeakStart);
				if (Zone.GetFirstSide().GetLength() <MaxThickness * 10)
				{
					ThinZones.Pop();
					continue;
				}
				DeleteCuttingPointOfPeak(PeakEdges[0]);
			}
			else if (LengthBetweenExtremity[1] <MaxLengthBetweendSideToBePeak)
			{
				Zone.SetCategory(EThinZone2DType::PeakEnd);
				if (Zone.GetFirstSide().GetLength() <MaxThickness * 10)
				{
					ThinZones.Pop();
					continue;
				}
				DeleteCuttingPointOfPeak(PeakEdges[1]);
			}
			else {
				Zone.SetCategory(EThinZone2DType::Butterfly);
			}
		}
		else
		{
			Zone.SetCategory(EThinZone2DType::BetweenLoops);
		}

		Zone.SetEdgesAsThinZone();
	}
}

void FThinZone2DFinder::DisplayLoopSegments()
{
#ifdef DEBUG_THIN_ZONES
	TSharedPtr<FTopologicalEdge> currentEdge = nullptr;
	currentEdge = LoopSegments[0]->GetEdge();
	F3DDebugSession _(TEXT("BoundarySegment"));

	Open3DDebugSession(FString::Printf(TEXT("Edge %d\n"), currentEdge->GetId()));
	for (FEdgeSegment* EdgeSegment : LoopSegments)
	{
		if (currentEdge != EdgeSegment->GetEdge())
		{
			currentEdge = EdgeSegment->GetEdge();
			Close3DDebugSession();
			Open3DDebugSession(FString::Printf(TEXT("Edge %d\n"), currentEdge->GetId()));
		}
		DisplaySegment(EdgeSegment->GetExtemity(ELimit::Start), EdgeSegment->GetExtemity(ELimit::End), EdgeSegment->GetId(), EVisuProperty::GreenCurve);
	}
	Close3DDebugSession();
	//Wait();
#endif
}

void FThinZone2DFinder::SearchThinZones()
{
#ifdef DEBUG_THIN_ZONES
	{
		F3DDebugSession _(TEXT("Thin Surface"));
		Grid.DisplayPoints("Scaled Grid", Grid.GetInner2DPoints(EGridSpace::Scaled));
	}
#endif

	//FMessage::Printf(DBG, TEXT("Searching thin zones on Surface %d\n", Surface->GetId());
	FTimePoint StartTime = FChrono::Now();
	BuildLoopSegments();
	Chronos.BuildLoopSegmentsTime = FChrono::Elapse(StartTime);

	DisplayLoopSegments();

	StartTime = FChrono::Now();
	FindClosedSegments();
	Chronos.FindClosedSegmentTime = FChrono::Elapse(StartTime);
	DisplayClosedSegments();

	StartTime = FChrono::Now();
	CheckClosedSegments();
	Chronos.CheckClosedSegmentTime = FChrono::Elapse(StartTime);
	DisplayClosedSegments();

	StartTime = FChrono::Now();
	LinkClosedSegments();
	Chronos.LinkClosedSegmentTime = FChrono::Elapse(StartTime);

	StartTime = FChrono::Now();
	BuildThinZone();
	Chronos.BuildThinZoneTime = FChrono::Elapse(StartTime);

	if (ThinZones.Num() > 0)
	{
		Grid.GetFace().SetHasThinZone();
	}

#ifdef DISPLAY_THIN_ZONES
	if (ThinZones.Num() > 0)
	{
		F3DDebugSession _(TEXT("Zones fines"));

		int32 index = 0;
		for (const FThinZone2D& Zone : ThinZones)
		{
			index++;
			EVisuProperty prop = PropertyElement;
			switch (Zone.GetCategory())
			{
			default:
			case FThinZone2D::UNDEFINED:
				prop = EVisuProperty::Iso;
				Open3DDebugSession(TEXT("Zone UNDEFINED"));
				break;
			case FThinZone2D::GLOBAL:
				prop = PROPERTY_BORDER_GEO;
				Open3DDebugSession(TEXT("Zone GLOBAL"));
				break;
			case FThinZone2D::PEAK_START:
				prop = EVisuProperty::BorderEdge;
				Open3DDebugSession(TEXT("Zone PEAK start"));
				break;
			case FThinZone2D::PEAK_END:
				prop = EVisuProperty::BorderEdge;
				Open3DDebugSession(TEXT("Zone PEAK end"));
				break;
			case FThinZone2D::BUTTERFLY:
				prop = PropertyElement;
				Open3DDebugSession(TEXT("Zone BUTTERFLY"));
				break;
			case FThinZone2D::BETWEEN_CONTOUR:
				prop = EVisuProperty::NonManifoldEdge;
				Open3DDebugSession(TEXT("Zone BETWEEN_CONTOUR"));
				break;
			default :
				Open3DDebugSession(TEXT("Zone Unknown"));
			}

			{
				F3DDebugSession _(TEXT("Side1"));
				int32 ind = 0;
				for (FEdgeSegment* EdgeSegment : Zone.GetFirstSide().GetSegments())
				{
					DisplaySegment(EdgeSegment->GetExtemity(ELimit::Start)(), EdgeSegment->GetExtemity(ELimit::End)(), ++ind, prop);
				}
			}
			{
				F3DDebugSession _(TEXT("Side2"));
				int32 ind = 0;
				for (FEdgeSegment* EdgeSegment : Zone.GetSecondSide().GetSegments())
				{
					DisplaySegment(EdgeSegment->GetExtemity(ELimit::Start)(), EdgeSegment->GetExtemity(ELimit::End)(), ++ind, prop);
				}
			}

			Close3DDebugSession();
		}
	}
#endif

}
void FThinZone2DFinder::BuildLoopSegments()
{
	double Length = 0;
	double SegmentLength = Tolerance / 5.;

	const TArray<TSharedPtr<FTopologicalLoop>>& Loops = Grid.GetFace().GetLoops();

	FirstLoopLength = -1.;
	for (const TSharedPtr<FTopologicalLoop>& Loop : Loops)
	{
		for (const FOrientedEdge& Edge : Loop->GetEdges())
		{
			Length += Edge.Entity->Length();
		}

		if (FirstLoopLength < 0)
		{
			FirstLoopLength = Length;
		}
	}

	double SegmentNum = 1.2 * Length / SegmentLength;
	LoopSegments.Empty((int32)SegmentNum);

	TSharedPtr<FTopologicalLoop> OuterLoop = Loops[0];
	for (TSharedPtr<FTopologicalLoop> Loop : Loops)
	{
		const TArray<FOrientedEdge>& Edges = Loop->GetEdges();

		FEdgeSegment* FirstSegment = nullptr;
		FEdgeSegment* PrecedingSegment = nullptr;

		for (const FOrientedEdge& Edge : Edges)
		{
			TArray<double> Coordinates;
			Edge.Entity->Sample(SegmentLength, Coordinates);

			TArray<FPoint2D> Points;
			Edge.Entity->Approximate2DPoints(Coordinates, Points);

			TArray<FPoint2D> ScaledPoints;
			Grid.TransformPoints(EGridSpace::Scaled, Points, ScaledPoints);

			int32 PointNum = Points.Num();
			for (int32 ISegment = 0; ISegment < PointNum - 1; ISegment++)
			{
				int32 Index1 = (Edge.Direction == EOrientation::Front) ? ISegment : PointNum - ISegment - 1l;
				int32 Index2 = (Edge.Direction == EOrientation::Front) ? ISegment + 1l : PointNum - ISegment - 2l;
				FEdgeSegment& CurrentSeg = SegmentFatory.New();
				CurrentSeg.SetBoundarySegment(OuterLoop != Loop, Edge.Entity, Coordinates[Index1], Coordinates[Index2], ScaledPoints[Index1], ScaledPoints[Index2]);

				LoopSegments.Add(&CurrentSeg);
				if (!FirstSegment)
				{
					FirstSegment = PrecedingSegment = &CurrentSeg;
				}
				else
				{
					PrecedingSegment->SetNext(&CurrentSeg);
				}
				PrecedingSegment = &CurrentSeg;
			}
		}
		PrecedingSegment->SetNext(FirstSegment);
	}
	SortedLoopSegments = LoopSegments;
}

FThinZoneSide::FThinZoneSide(FThinZoneSide* InFrontSide, const TArray<FEdgeSegment*>& InSegments, bool bInIsFirstSide)
	: FrontSide(*InFrontSide)
{
	Segments = InSegments;
	Length = 0;
	for (const FEdgeSegment* Segment : Segments)
	{
		Length += Segment->GetLength();
	}
}

void FThinZoneSide::SetEdgesAsThinZone()
{
	for (FEdgeSegment* Segment : Segments)
	{
		Segment->GetEdge()->SetThinZone();
	}
}

bool FThinZoneSide::IsPartiallyMeshed() const
{
	TSharedPtr<FTopologicalEdge> Edge = nullptr;
	for (FEdgeSegment* EdgeSegment : Segments)
	{
		if (Edge != EdgeSegment->GetEdge())
		{
			Edge = EdgeSegment->GetEdge();
			if (Edge->GetLinkActiveEdge()->IsMeshed())
			{
				return true;
			}
		}
	}
	return false;
}

double FThinZoneSide::GetMeshedLength() const
{
	double LocalLength = 0;
	TSharedPtr<FTopologicalEdge> Edge = nullptr;
	for (FEdgeSegment* EdgeSegment : Segments)
	{
		if (Edge != EdgeSegment->GetEdge())
		{
			Edge = EdgeSegment->GetEdge();
			if (Edge->GetLinkActiveEdge()->IsMeshed())
			{
				LocalLength += Edge->Length();
			}
		}
	}
	return LocalLength;
}

void FThinZone2D::SetEdgesAsThinZone()
{
	FirstSide.SetEdgesAsThinZone();
	SecondSide.SetEdgesAsThinZone();
}

}

