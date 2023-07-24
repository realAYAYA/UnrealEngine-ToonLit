// Copyright Epic Games, Inc. All Rights Reserved.

#include "GlyphLoader.h"
#include "Arrangement.h"
#include "BoxTypes.h"
#include "Part.h"
#include "Polygon2.h"
#include "Curve/DynamicGraph2.h"

using namespace UE::Geometry;

FGlyphLoader::FGlyphLoader(const FT_GlyphSlot Glyph)
	: Root(MakeShared<FContourNode>(nullptr, false, true))
	, EndIndex(-1)
	, VertexID(0)
{
	check(Glyph);

	const FT_Outline Outline = Glyph->outline;
	const int32 ContourCount = Outline.n_contours;

	for (int32 Index = 0; Index < ContourCount; Index++)
	{
		const TSharedPtr<FPolygon2f> Contour = CreateContour(Outline, Index);

		if (Contour.IsValid())
		{
			Insert(MakeShared<FContourNode>(Contour, true, Contour->IsClockwise()), Root);
		}
	}

	if (Root->Children.Num() == 0)
	{
		return;
	}

	FixParityAndDivide(Root, false);
}

TSharedContourNode FGlyphLoader::GetContourList() const
{
	return Root;
}


FGlyphLoader::FLine::FLine(const FVector2D PositionIn)
	: Position(PositionIn)
{
}

void FGlyphLoader::FLine::Add(FGlyphLoader* const Loader)
{
	if (!Loader->ProcessedContour.Num() || !(Position - Loader->FirstPosition).IsNearlyZero())
	{
		Loader->ProcessedContour.AddHead(FVector2f(Position));
	}
}

FGlyphLoader::FCurve::FCurve(const bool bLineIn)
	: bLine(bLineIn)
	, StartT(0.f)
	, EndT(1.f)
{
	Loader = nullptr;

	Depth = 0.0f;
	MaxDepth = 0.0f;
	First = nullptr;
	Last = nullptr;
	bFirstSplit = false;
	bLastSplit = false;
}

FGlyphLoader::FCurve::~FCurve()
{
}

struct FGlyphLoader::FCurve::FPointData
{
	FPointData()
	{
		T = 0.0f;
		Point = nullptr;
	}

	float T;
	FVector2D Position;
	FVector2D Tangent;
	TDoubleLinkedList<FVector2f>::TDoubleLinkedListNode* Point;
};

void FGlyphLoader::FCurve::Add(FGlyphLoader* const LoaderIn)
{
	Loader = LoaderIn;

	if (bLine)
	{
		FLine(Position(StartT)).Add(Loader);
		return;
	}

	Depth = 0;

	FPointData Start;
	Start.T = StartT;
	Start.Position = Position(Start.T);
	Start.Tangent = Tangent(Start.T);
	Loader->ProcessedContour.AddHead(FVector2f(Start.Position));
	Start.Point = Loader->ProcessedContour.GetHead();
	First = Start.Point;
	bFirstSplit = false;

	FPointData End;
	End.T = EndT;
	End.Position = Position(End.T);
	End.Tangent = Tangent(End.T);
	End.Point = nullptr;
	Last = End.Point;
	bLastSplit = false;


	ComputeMaxDepth();
	Split(Start, End);
}

bool FGlyphLoader::FCurve::OnOneLine(const FVector2D A, const FVector2D B, const FVector2D C)
{
	return FMath::IsNearlyZero(FVector2D::CrossProduct((B - A).GetSafeNormal(), (C - A).GetSafeNormal()));
}

void FGlyphLoader::FCurve::ComputeMaxDepth()
{
	// Compute approximate curve length with 4 points
	const float MinStep = 30.f;
	const float StepT = 0.333f;
	float Length = 0.f;

	FVector2D Prev;
	FVector2D Current = Position(StartT);

	for (float T = StartT + StepT; T < EndT; T += StepT)
	{
		Prev = Current;
		Current = Position(T);

		Length += (Current - Prev).Size();
	}

	const float MaxStepCount = Length / MinStep;
	MaxDepth = static_cast<int32>(FMath::Log2(MaxStepCount)) + 1;
}

void FGlyphLoader::FCurve::Split(const FPointData& Start, const FPointData& End)
{
	Depth++;
	FPointData Middle;

	Middle.T = (Start.T + End.T) / 2.f;
	Middle.Position = Position(Middle.T);
	Middle.Tangent = Tangent(Middle.T);
	Loader->ProcessedContour.InsertNode(FVector2f(Middle.Position), Start.Point);
	Middle.Point = Start.Point->GetPrevNode();

	CheckPart(Start, Middle);
	UpdateTangent(&Middle);
	CheckPart(Middle, End);
	Depth--;
}

void FGlyphLoader::FCurve::CheckPart(const FPointData& Start, const FPointData& End)
{
	const FVector2D Side = (End.Position - Start.Position).GetSafeNormal();

	if ((FVector2D::DotProduct(Side, Start.Tangent) > FPart::CosMaxAngleSideTangent &&
		FVector2D::DotProduct(Side, End.Tangent) > FPart::CosMaxAngleSideTangent) || Depth >= MaxDepth)
	{
		if (!bFirstSplit && Start.Point == First)
		{
			bFirstSplit = true;
			Split(Start, End);
		}
		else if (!bLastSplit && End.Point == Last)
		{
			bLastSplit = true;
			Split(Start, End);
		}
	}
	else
	{
		Split(Start, End);
	}
}

void FGlyphLoader::FCurve::UpdateTangent(FPointData* const Middle)
{
}

FGlyphLoader::FQuadraticCurve::FQuadraticCurve(const FVector2D A, const FVector2D B, const FVector2D C)
	: FCurve(OnOneLine(A, B, C))

	, E(A - 2.f * B + C)
	, F(-A + B)
	, G(A)
{
}

FVector2D FGlyphLoader::FQuadraticCurve::Position(const float T) const
{
	return E * T * T + 2.f * F * T + G;
}

FVector2D QuadraticCurveTangent(const FVector2D E, const FVector2D F, const float T)
{
	const FVector2D Result = E * T + F;

	if (Result.IsNearlyZero())
	{
		// Just some vector with non-zero length
		return {1.f, 0.f};
	}

	return Result;
}

FVector2D FGlyphLoader::FQuadraticCurve::Tangent(const float T)
{
	return QuadraticCurveTangent(E, F, T).GetSafeNormal();
}

FGlyphLoader::FCubicCurve::FCubicCurve(const FVector2D A, const FVector2D B, const FVector2D C, const FVector2D D)
	: FCurve(OnOneLine(A, B, C) && OnOneLine(B, C, D))
	, E(-A + 3.f * B - 3.f * C + D)
	, F(A - 2.f * B + C)
	, G(-A + B)
	, H(A)
	, bSharpStart(false)
	, bSharpMiddle(false)
	, bSharpEnd(false)
{
	if (!bLine)
	{
		bSharpStart = G.IsNearlyZero();
		bSharpEnd = (C - D).IsNearlyZero();
	}
}

void FGlyphLoader::FCubicCurve::UpdateTangent(FPointData* const Middle)
{
	// In this point curve is not smooth, and  r'(t + 0) / |r'(t + 0)| = -r'(t - 0) / |r'(t - 0)|
	if (bSharpMiddle)
	{
		bSharpMiddle = false;
		Middle->Tangent *= -1.f;
	}
}

FVector2D FGlyphLoader::FCubicCurve::Position(const float T) const
{
	return E * T * T * T + 3.f * F * T * T + 3.f * G * T + H;
}

FVector2D FGlyphLoader::FCubicCurve::Tangent(const float T)
{
	FVector2D Result;

	// Using  r' / |r'|  for sharp start and end
	if (bSharpStart && FMath::IsNearlyEqual(T, StartT))
	{
		Result = F;
	}
	else if (bSharpEnd && FMath::IsNearlyEqual(T, EndT))
	{
		Result = -(E + F);
	}
	else
	{
		Result = E * T * T + 2.f * F * T + G;
		bSharpMiddle = Result.IsNearlyZero();

		if (bSharpMiddle)
		{
			// Using derivative of quadratic bezier curve (A, B, C) in this point
			Result = QuadraticCurveTangent(F, G, T);
		}
	}

	return Result.GetSafeNormal();
}

TSharedPtr<FPolygon2f> FGlyphLoader::CreateContour(const FT_Outline Outline, const int32 ContourIndex)
{
	const TSharedPtr<FPolygon2f> Contour = ProcessFreetypeOutline(Outline, ContourIndex);
	return Contour.IsValid() ? RemoveBadPoints(Contour) : Contour;
}

bool FGlyphLoader::IsInside(const TSharedPtr<const FPolygon2f> ContourA,
							const TSharedPtr<const FPolygon2f> ContourB) const
{
	return ContourB->Contains(ContourA->GetSegmentPoint(0, 0));
}

void FGlyphLoader::Insert(const TSharedContourNode NodeA, const TSharedContourNode NodeB) const
{
	TArray<TSharedContourNode>& ChildrenB = NodeB->Children;
	const TSharedPtr<const FPolygon2f> ContourA = NodeA->Contour;

	for (int32 ChildBIndex = 0; ChildBIndex < ChildrenB.Num(); ChildBIndex++)
	{
		TSharedContourNode& ChildB = ChildrenB[ChildBIndex];
		const TSharedPtr<const FPolygon2f> ChildBContour = ChildB->Contour;

		if (IsInside(ContourA, ChildBContour))
		{
			Insert(NodeA, ChildB);
			return;
		}

		if (IsInside(ChildBContour, ContourA))
		{
			// add ChildBContour to list of contours that are inside ContourA
			TArray<TSharedContourNode>& ChildrenA = NodeA->Children;
			ChildrenA.Add(ChildB);
			// replace ChildBContour with ContourA in list it was before
			ChildB = NodeA;

			// check if other contours in that list are inside ContourA
			for (int32 ChildBSiblingIndex = ChildrenB.Num() - 1; ChildBSiblingIndex > ChildBIndex; ChildBSiblingIndex--)
			{
				TSharedContourNode ChildBSibling = ChildrenB[ChildBSiblingIndex];

				if (IsInside(ChildBSibling->Contour, ContourA))
				{
					ChildrenA.Add(ChildBSibling);
					ChildrenB.RemoveAt(ChildBSiblingIndex);
				}
			}

			return;
		}
	}

	ChildrenB.Add(NodeA);
}

void FGlyphLoader::FixParityAndDivide(const TSharedContourNode Node, const bool bClockwiseIn)
{
	TArray<TSharedContourNode>& Children = Node->Children;

	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ChildIndex++)
	{
		TSharedContourNode Child = Children[ChildIndex];
		const TSharedPtr<FPolygon2f> ChildContour = Child->Contour;

		// bCanHaveIntersections has default value "true", if it's false then contour was checked for having self-intersections and it's parity was fixed
		if (!Child->bCanHaveIntersections)
		{
			FixParityAndDivide(Child, !bClockwiseIn);
			continue;
		}

		// Fix parity
		if (Child->bClockwise != bClockwiseIn)
		{
			ChildContour->Reverse();
			Child->bClockwise = bClockwiseIn;
		}

		MakeArrangement(ChildContour);
		Junctions.Reset();

		// Find first junction
		if (!FindJunction())
		{
			// If no junction was found, contour doesn't have self-intersections
			FixParityAndDivide(Child, !bClockwiseIn);
			continue;
		}

		// Remove this contour from parent's child list
		Children.RemoveAt(ChildIndex--);
		// Reset path
		DividedContourIDs.Reset();
		// Parts are stored as children of separate root node
		TSharedContourNode RootForDetaching = MakeShared<FContourNode>(nullptr, false, Node->bClockwise);
		// Processed edges are removed from graph to simplify dividing, vertices that don't belong to any edge are removed too, so a copy is needed
		const TArray<FVector2f> InitialVertices = CopyVertices();

		bool bAddPoint = true;
		while (bAddPoint)
		{
			AddPoint();

			if (VertexID == -1)
			{
				// This is bad contour (not a closed loop)
				VertexID = Junctions.Last();
				DetachBadContour();
			}
			else
			{
				const int32 RepeatedJunctionIndex = Junctions.Find(VertexID);
				if (RepeatedJunctionIndex == INDEX_NONE)
				{
					continue;
				}

				// If it is, we made a loop that should be separated
				DetachFinishedContour(RepeatedJunctionIndex, InitialVertices, RootForDetaching);
			}

			// If path contains no junctions and current vertex is not a junction
			if (Junctions.Num() == 0 && !Arrangement->Graph.IsJunctionVertex(VertexID))
			{
				if (!FindJunction())
				{
					// If there are no junctions in graph, it contains last contour that should be separated
					if (!FindRegular())
					{
						// Graph is empty, done
						bAddPoint = false;
					}
					else
					{
						// Add non-junction vertex as if it was a junction
						AddPoint(true);
					}
				}
			}
		}

		RemoveUnneededNodes(RootForDetaching);
		MergeRootForDetaching(Child, RootForDetaching, Node);
	}
}

TSharedPtr<FPolygon2f> FGlyphLoader::ProcessFreetypeOutline(const FT_Outline Outline, const int32 ContourIndex)
{
	TSharedPtr<FPolygon2f> Contour;

	const int32 StartIndex = EndIndex + 1;
	EndIndex = Outline.contours[ContourIndex];
	const int32 ContourLength = EndIndex - StartIndex + 1;

	if (ContourLength < 3)
	{
		return Contour;
	}

	ProcessedContour.Empty();

	const FT_Vector* const Points = Outline.points + StartIndex;
	auto ToFVector2D = [Points](const int32 Index)
	{
		const FT_Vector Point = Points[Index];
		return FVector2D(Point.x, Point.y);
	};

	FVector2D Prev;
	FVector2D Current = ToFVector2D(ContourLength - 1);
	FVector2D Next = ToFVector2D(0);
	FVector2D NextNext = ToFVector2D(1);


	const char* const Tags = Outline.tags + StartIndex;
	auto Tag = [Tags](int32 Index)
	{
		return FT_CURVE_TAG(Tags[Index]);
	};

	int32 TagPrev = 0;
	int32 TagCurrent = Tag(ContourLength - 1);
	int32 TagNext = Tag(0);


	FVector2D& FirstPositionLocal = FirstPosition;
	TDoubleLinkedList<FVector2f>& ProcessedContourLocal = ProcessedContour;
	auto ContourIsBad = [&FirstPositionLocal, &ProcessedContourLocal](const FVector2D Point)
	{
		if (ProcessedContourLocal.Num() == 0)
		{
			FirstPositionLocal = Point;
			return false;
		}

		return (Point - FVector2D(ProcessedContourLocal.GetHead()->GetValue())).IsNearlyZero();
	};

	for (int32 Index = 0; Index < ContourLength; Index++)
	{
		const int32 NextIndex = (Index + 1) % ContourLength;

		Prev = Current;
		Current = Next;
		Next = NextNext;
		NextNext = ToFVector2D((NextIndex + 1) % ContourLength);

		TagPrev = TagCurrent;
		TagCurrent = TagNext;
		TagNext = Tag(NextIndex);

		if (TagCurrent == FT_Curve_Tag_On)
		{
			if (TagNext == FT_Curve_Tag_Cubic || TagNext == FT_Curve_Tag_Conic)
			{
				continue;
			}

			if (ContourIsBad(Current))
			{
				return Contour;
			}

			if (TagNext == FT_Curve_Tag_On && (Current - Next).IsNearlyZero())
			{
				continue;
			}

			FLine(Current).Add(this);
		}
		else if (TagCurrent == FT_Curve_Tag_Conic)
		{
			FVector2D A;

			if (TagPrev == FT_Curve_Tag_On)
			{
				if (ContourIsBad(Prev))
				{
					return Contour;
				}

				A = Prev;
			}
			else
			{
				A = (Prev + Current) / 2.f;
			}

			FQuadraticCurve(A, Current, TagNext == FT_Curve_Tag_Conic ? (Current + Next) / 2.f : Next).Add(this);
		}
		else if (TagCurrent == FT_Curve_Tag_Cubic && TagNext == FT_Curve_Tag_Cubic)
		{
			if (ContourIsBad(Prev))
			{
				return Contour;
			}

			FCubicCurve(Prev, Current, Next, NextNext).Add(this);
		}
	}

	if (ProcessedContour.Num() < 3)
	{
		return Contour;
	}


	Contour = MakeShared<FPolygon2f>();

	for (const TDoubleLinkedList<FVector2f>::TDoubleLinkedListNode* Point = ProcessedContour.GetTail(); Point; Point =
		 Point->GetPrevNode())
	{
		Contour->AppendVertex(Point->GetValue());
	}

	return Contour;
}

bool FGlyphLoader::IsBadNormal(const TSharedPtr<FPolygon2f> Contour, const int32 Point) const
{
	FVector2f ToPrev;
	FVector2f ToNext;

	Contour->NeighbourVectors(Point, ToPrev, ToNext, true);
	return FMath::IsNearlyZero(1.0f - FVector2D::DotProduct(FVector2D(ToPrev), FVector2D(ToNext)));
}

bool FGlyphLoader::MergedWithNext(const TSharedPtr<FPolygon2f> Contour, const int32 Point) const
{
	const int32 Current = Point;
	const int32 Next = (Point + 1) % Contour->VertexCount();

	return FVector2D((*Contour)[Next] - (*Contour)[Current]).IsNearlyZero();
}

TSharedPtr<FPolygon2f> FGlyphLoader::RemoveBadPoints(const TSharedPtr<FPolygon2f> Contour) const
{
	int32 First = 0;

	for (int32 Point = First;;)
	{
		bool bPointRemoved = false;

		while (IsBadNormal(Contour, Point) || MergedWithNext(Contour, Point))
		{
			bPointRemoved = true;

			if (Contour->VertexCount() < 4)
			{
				return TSharedPtr<FPolygon2f>();
			}

			Contour->RemoveVertex(Point);
			const int32 Current = (Point - 1 + Contour->VertexCount()) % Contour->VertexCount();

			if (Point == 0)
			{
				First = Current;
			}

			Point = Current;
		}

		if (!bPointRemoved)
		{
			Point = (Point + 1) % Contour->VertexCount();

			if (Point == First)
			{
				break;
			}
		}
	}

	return Contour;
}

void FGlyphLoader::MakeArrangement(const TSharedPtr<const FPolygon2f> Contour)
{
	FAxisAlignedBox2f Box = FAxisAlignedBox2f::Empty();

	for (const FVector2f& Vertex : Contour->GetVertices())
	{
		Box.Contain(Vertex);
	}

	Arrangement = MakeShared<FArrangement>(Box);

	for (FSegment2f Edge : Contour->Segments())
	{
		Arrangement->Insert(Edge);
	}
}

bool FGlyphLoader::FindJunction()
{
	FDynamicGraph2d& Graph = Arrangement->Graph;

	for (int32 VID = 0; VID < Graph.MaxVertexID(); VID++)
	{
		if (Graph.IsJunctionVertex(VID))
		{
			VertexID = VID;
			return true;
		}
	}

	VertexID = -1;
	return false;
}

bool FGlyphLoader::FindRegular()
{
	FDynamicGraph2d& Graph = Arrangement->Graph;

	for (int32 VID = 0; VID < Graph.MaxVertexID(); VID++)
	{
		if (Graph.IsRegularVertex(VID))
		{
			VertexID = VID;
			return true;
		}
	}

	VertexID = -1;
	return false;
}

TArray<FVector2f> FGlyphLoader::CopyVertices() const
{
	FDynamicGraph2d& Graph = Arrangement->Graph;
	TArray<FVector2f> Vertices;
	const int32 MaxVID = Graph.MaxVertexID();
	Vertices.Reserve(MaxVID);

	for (int32 VID = 0; VID < MaxVID; VID++)
	{
		const FVector2d Vertex = Graph.GetVertex(VID);
		Vertices.Add({static_cast<float>(Vertex.X), static_cast<float>(Vertex.Y)});
	}

	return Vertices;
}

void FGlyphLoader::DetachBadContour()
{
	const int32 JunctionIndexInContour = FindStartOfDetachedContour(Junctions.Num() - 1);
	RemoveDetachedContourFromPath(JunctionIndexInContour);
}

void FGlyphLoader::DetachFinishedContour(const int32 RepeatedJunctionIndex, const TArray<FVector2f>& InitialVertices,
										 const TSharedContourNode RootForDetaching)
{
	const int32 JunctionIndexInContour = FindStartOfDetachedContour(RepeatedJunctionIndex);

	// Create contour from copied vertices
	TSharedPtr<FPolygon2f> FinishedContour = MakeShared<FPolygon2f>();

	for (int32 ID = JunctionIndexInContour; ID < DividedContourIDs.Num(); ID++)
	{
		FinishedContour->AppendVertex(InitialVertices[DividedContourIDs[ID]]);
	}

	RemoveDetachedContourFromPath(JunctionIndexInContour);

	const TSharedContourNode FinishedContourNode = MakeShared<FContourNode>(FinishedContour,
																			false,
																			FinishedContour->IsClockwise());
	Insert(FinishedContourNode, RootForDetaching);
}

int32 FGlyphLoader::FindStartOfDetachedContour(const int32 RepeatedJunctionIndex)
{
	// Remove from list of junctions all junctions that belong to detached loop
	Junctions.SetNum(RepeatedJunctionIndex);
	// Find index in path of first (and last) vertex of loop
	const int32 JunctionIndexInContour = DividedContourIDs.Find(VertexID);
	return JunctionIndexInContour;
}

void FGlyphLoader::RemoveDetachedContourFromPath(const int32 JunctionIndexInContour)
{
	DividedContourIDs.SetNum(JunctionIndexInContour);
}

void FGlyphLoader::AddPoint(const bool bForceJunction)
{
	DividedContourIDs.Add(VertexID);
	FDynamicGraph2d& Graph = Arrangement->Graph;
	int32 OutgoingEdge = 0;

	if (Graph.IsJunctionVertex(VertexID) || bForceJunction)
	{
		Junctions.Add(VertexID);
		// Select specific path direction
		OutgoingEdge = FindOutgoing(Junctions.Last());
	}
	else if (Graph.IsVertex(VertexID))
	{
		// This should be the only possible direction
		OutgoingEdge = *Graph.VtxEdgesItr(VertexID).begin();
	}
	else
	{
		// We have no possible directions, this contour is bad
		VertexID = -1;
		return;
	}

	// Move to next graph node
	VertexID = edge_other_v(OutgoingEdge, VertexID);
	Graph.RemoveEdge(OutgoingEdge, true);
}

void FGlyphLoader::RemoveUnneededNodes(const TSharedContourNode Node) const
{
	TArray<TSharedContourNode>& Children = Node->Children;

	for (int32 ChildIndex = 0; ChildIndex < Children.Num();)
	{
		const TSharedContourNode Child = Children[ChildIndex];

		if (Child->bClockwise == Node->bClockwise)
		{
			Children.RemoveAt(ChildIndex);
		}
		else
		{
			ChildIndex++;
			RemoveUnneededNodes(Child);
		}
	}
}

void FGlyphLoader::MergeRootForDetaching(const TSharedContourNode RemovedNode,
										 const TSharedContourNode RootForDetaching,
										 const TSharedContourNode RemovedNodeParent) const
{
	for (const TSharedContourNode& Child : RemovedNode->Children)
	{
		Insert(Child, RootForDetaching);
	}

	for (const TSharedContourNode& Child : RootForDetaching->Children)
	{
		RemovedNodeParent->Children.Add(Child);
	}
}

bool FGlyphLoader::IsOutgoing(const int32 Junction, const int32 EID) const
{
	return (Arrangement->Graph.GetEdgeV(EID).A == Junction) == Arrangement->Directions[EID];
}

int32 FGlyphLoader::FindOutgoing(const int32 Junction) const
{
	// Get list of all edges that have this vertex
	TArray<int32> Edges;
	SortedVtxEdges(Junction, Edges);

	int32 Current = 0;
	int32 Next = Edges[0];

	for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); EdgeIndex++)
	{
		Current = Next;
		Next = Edges[(EdgeIndex + 1) % Edges.Num()];

		// Needed direction is the one that is outgoing and it's clockwise neighbour is not
		if (!IsOutgoing(Junction, Current) && IsOutgoing(Junction, Next))
		{
			return Next;
		}
	}

	return Edges[0];
}

int32 FGlyphLoader::edge_other_v(const int32 EID, const int32 VID) const
{
	const FIndex2i Edge = Arrangement->Graph.GetEdgeV(EID);
	return (Edge.A == VID) ? Edge.B : ((Edge.B == VID) ? Edge.A : FDynamicGraph::InvalidID);
}

void FGlyphLoader::SortedVtxEdges(const int32 VID, TArray<int32>& Sorted) const
{
	FDynamicGraph2d& Graph = Arrangement->Graph;
	Sorted.Reserve(Graph.GetVtxEdgeCount(VID));

	for (int32 EID : Graph.VtxEdgesItr(VID))
	{
		Sorted.Add(EID);
	}

	const FVector2d V = Graph.GetVertex(VID);
	Algo::SortBy(Sorted,
				 [&](int32 EID)
				 {
					 const int32 NbrVID = edge_other_v(EID, VID);
					 const FVector2d D = Graph.GetVertex(NbrVID) - V;
					 return TMathUtil<double>::Atan2Positive(D.Y, D.X);
				 });
}
