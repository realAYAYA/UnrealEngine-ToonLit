// Copyright Epic Games, Inc. All Rights Reserved.

#include "Arrangement.h"

using namespace UE::Geometry;

FArrangement::FArrangement(const FAxisAlignedBox2f& BoundsHint)
    : PointHash(static_cast<double>(BoundsHint.MaxDim()) / 64, -1)
{
}

void FArrangement::Insert(const FSegment2f& Segment)
{
    const FVector2f A = Segment.StartPoint();
    const FVector2f B = Segment.EndPoint();

    insert_segment({static_cast<double>(A.X), static_cast<double>(A.Y)}, {static_cast<double>(B.X), static_cast<double>(B.Y)}, VertexSnapTol);
}

int FArrangement::insert_point(const FVector2d &P, double Tol)
{
    int PIdx = find_existing_vertex(P);
    if (PIdx > -1)
    {
        return -1;
    }

    // TODO: currently this tries to add the vertex on the closest edge below tolerance; we should instead insert at *every* edge below tolerance!  ... but that is more inconvenient to write
    FVector2d x = FVector2d::Zero(), y = FVector2d::Zero();
    double ClosestDistSq = Tol*Tol;
    int FoundEdgeToSplit = -1;
    for (int EID = 0, ExistingEdgeMax = Graph.MaxEdgeID(); EID < ExistingEdgeMax; EID++)
    {
        if (!Graph.IsEdge(EID))
        {
            continue;
        }

        Graph.GetEdgeV(EID, x, y);
        FSegment2d Seg(x, y);
        double DistSq = Seg.DistanceSquared(P);
        if (DistSq < ClosestDistSq)
        {
            ClosestDistSq = DistSq;
            FoundEdgeToSplit = EID;
        }
    }
    if (FoundEdgeToSplit > -1)
    {
        FDynamicGraph2d::FEdgeSplitInfo splitInfo;
        EMeshResult result = Graph.SplitEdge(FoundEdgeToSplit, splitInfo);
        ensureMsgf(result == EMeshResult::Ok, TEXT("insert_into_segment: edge split failed?"));
        Graph.SetVertex(splitInfo.VNew, P);
        PointHash.InsertPointUnsafe(splitInfo.VNew, P);
        return splitInfo.VNew;
    }

    int VID = Graph.AppendVertex(P);
    PointHash.InsertPointUnsafe(VID, P);
    return VID;
}

bool FArrangement::insert_segment(FVector2d A, FVector2d B, double Tol)
{
    // handle degenerate edges
    int a_idx = find_existing_vertex(A);
    int b_idx = find_existing_vertex(B);
    if (a_idx == b_idx && a_idx >= 0)
    {
        return false;
    }
    // snap input vertices
    if (a_idx >= 0)
    {
        A = Graph.GetVertex(a_idx);
    }
    if (b_idx >= 0)
    {
        B = Graph.GetVertex(b_idx);
    }

    // handle tiny-segment case
    double SegLenSq = DistanceSquared(A, B);
    if (SegLenSq <= VertexSnapTol*VertexSnapTol)
    {
        // seg is too short and was already on an existing vertex; just consider that vertex to be the inserted segment
        if (a_idx >= 0 || b_idx >= 0)
        {
            return false;
        }
        // seg is too short and wasn't on an existing vertex; add it as an isolated vertex
        return insert_point(A, Tol) != -1;
    }

    // ok find all intersections
    TArray<FIntersection> Hits;
    find_intersecting_edges(A, B, Hits, Tol);

    // we are going to construct a list of <T,vertex_id> values along segment AB
    TArray<FSegmentPoint> points;
    FSegment2d segAB = FSegment2d(A, B);

    find_intersecting_floating_vertices(segAB, a_idx, b_idx, points, Tol);

    // insert intersections into existing segments
    for (int i = 0, N = Hits.Num(); i < N; ++i)
    {
        FIntersection Intr = Hits[i];
        int EID = Intr.EID;
        double t0 = Intr.Intr.Parameter0, t1 = Intr.Intr.Parameter1;

        // insert first point at t0
        int new_eid = -1;
        if (Intr.Intr.Type == EIntersectionType::Point || Intr.Intr.Type == EIntersectionType::Segment)
        {
            FIndex2i new_info = split_segment_at_t(EID, t0, VertexSnapTol);
            new_eid = new_info.B;
            FVector2d v = Graph.GetVertex(new_info.A);
            points.Add(FSegmentPoint{segAB.Project(v), new_info.A});
        }

        // if intersection was on-segment, then we have a second point at t1
        if (Intr.Intr.Type == EIntersectionType::Segment)
        {
            if (new_eid == -1)
            {
                // did not actually split edge for t0, so we can still use EID
                FIndex2i new_info = split_segment_at_t(EID, t1, VertexSnapTol);
                FVector2d v = Graph.GetVertex(new_info.A);
                points.Add(FSegmentPoint{segAB.Project(v), new_info.A});
            }
            else
            {
                // find t1 was in EID, rebuild in new_eid
                FSegment2d new_seg = Graph.GetEdgeSegment(new_eid);
                FVector2d p1 = Intr.Intr.GetSegment1().PointAt(t1);
                double new_t1 = new_seg.Project(p1);
                // note: new_t1 may be outside of new_seg due to snapping; in this case the segment will just not be split

                FIndex2i new_info = split_segment_at_t(new_eid, new_t1, VertexSnapTol);
                FVector2d v = Graph.GetVertex(new_info.A);
                points.Add(FSegmentPoint{segAB.Project(v), new_info.A});
            }
        }
    }

    // find or create start and end points
    if (a_idx == -1)
    {
        a_idx = find_existing_vertex(A);
    }
    if (a_idx == -1)
    {
        a_idx = Graph.AppendVertex(A);
        PointHash.InsertPointUnsafe(a_idx, A);
    }
    if (b_idx == -1)
    {
        b_idx = find_existing_vertex(B);
    }
    if (b_idx == -1)
    {
        b_idx = Graph.AppendVertex(B);
        PointHash.InsertPointUnsafe(b_idx, B);
    }

    // add start/end to points list. These may be duplicates but we will sort that out after
    points.Add(FSegmentPoint{-segAB.Extent, a_idx});
    points.Add(FSegmentPoint{segAB.Extent, b_idx});
    // sort by T
    points.Sort([](const FSegmentPoint& pa, const FSegmentPoint& pb) { return pa.T < pb.T; });

    // connect sequential points, as long as they aren't the same point,
    // and the segment doesn't already exist
    for (int k = 0; k < points.Num() - 1; ++k)
    {
        int v0 = points[k].VID;
        int v1 = points[k + 1].VID;
        if (v0 == v1)
        {
            continue;
        }

        if (Graph.FindEdge(v0, v1) == FDynamicGraph2d::InvalidID)
        {
            // sanity check; technically this can happen and still be correct but it's more likely an error case
            ensureMsgf(FMath::Abs(points[k].T - points[k + 1].T) >= std::numeric_limits<float>::epsilon(), TEXT("insert_segment: different points have same T??"));

            const int EID = Graph.AppendEdge(v0, v1);
            Directions.Add(EID, v0 < v1);
        }
    }

    return true;
}

FIndex2i FArrangement::split_segment_at_t(int EID, double T, double Tol)
{
    FVector2d V1, V2;
    FIndex2i ev = Graph.GetEdgeV(EID);
    FSegment2d seg = FSegment2d(Graph.GetVertex(ev.A), Graph.GetVertex(ev.B));

    int use_vid = -1;
    int new_eid = -1;
    if (T < -(seg.Extent - Tol))
    {
        use_vid = ev.A;
    }
    else if (T > (seg.Extent - Tol))
    {
        use_vid = ev.B;
    }
    else
    {
        FVector2d Pt = seg.PointAt(T);
        FDynamicGraph2d::FEdgeSplitInfo splitInfo;
        EMeshResult result;
        int CrossingVert = find_existing_vertex(Pt);
        if (CrossingVert == -1)
        {
            result = Graph.SplitEdge(EID, splitInfo);
        }
        else
        {
            result = Graph.SplitEdgeWithExistingVertex(EID, CrossingVert, splitInfo);
        }
        ensureMsgf(result == EMeshResult::Ok, TEXT("insert_into_segment: edge split failed?"));
        use_vid = splitInfo.VNew;
        new_eid = splitInfo.ENewBN;

        Directions.Add(new_eid, !Directions[EID]);

        if (CrossingVert == -1)
        {	// position + track added vertex
            Graph.SetVertex(use_vid, Pt);
            PointHash.InsertPointUnsafe(splitInfo.VNew, Pt);
        }
    }
    return FIndex2i(use_vid, new_eid);
}

int FArrangement::find_existing_vertex(FVector2d Pt)
{
    return find_nearest_vertex(Pt, VertexSnapTol);
}

int FArrangement::find_nearest_vertex(FVector2d Pt, double SearchRadius, int IgnoreVID)
{
    auto FuncDistSq = [&](int B) { return DistanceSquared(Pt, Graph.GetVertex(B)); };
    auto FuncIgnore = [&](int VID) { return VID == IgnoreVID; };
    TPair<int, double> found = (IgnoreVID == -1) ? PointHash.FindNearestInRadius(Pt, SearchRadius, FuncDistSq)
                                                 : PointHash.FindNearestInRadius(Pt, SearchRadius, FuncDistSq, FuncIgnore);
    if (found.Key == PointHash.GetInvalidValue())
    {
        return -1;
    }
    return found.Key;
}

bool FArrangement::find_intersecting_edges(FVector2d A, FVector2d B, TArray<FIntersection>& Hits, double Tol)
{
    int num_hits = 0;
    FVector2d x = FVector2d::Zero(), y = FVector2d::Zero();
    FVector2d EPerp = UE::Geometry::PerpCW(B - A);
    UE::Geometry::Normalize(EPerp);
    for (int EID : Graph.EdgeIndices())
    {
        Graph.GetEdgeV(EID, x, y);
        // inlined version of WhichSide with pre-normalized EPerp, to ensure Tolerance is consistent for different edge lengths
        double SignX = EPerp.Dot(x - A);
        double SignY = EPerp.Dot(y - A);
        int SideX = (SignX > Tol ? +1 : (SignX < -Tol ? -1 : 0));
        int SideY = (SignY > Tol ? +1 : (SignY < -Tol ? -1 : 0));
        if (SideX == SideY && SideX != 0)
        {
            continue; // both pts on same side
        }

        FIntrSegment2Segment2d Intr(FSegment2d(x, y), FSegment2d(A, B));
        Intr.SetIntervalThreshold(Tol);
        // set a loose DotThreshold as well so almost-parallel segments are treated as parallel;
        //  otherwise we're more likely to hit later problems when an edge intersects near-overlapping edges at almost the same point
        // (TODO: detect + handle that case!)
        Intr.SetDotThreshold(1e-4);
        if (Intr.Find())
        {
            Hits.Add(FIntersection{EID, SideX, SideY, Intr});
            num_hits++;
        }
    }

    return (num_hits > 0);
}

bool FArrangement::find_intersecting_floating_vertices(const FSegment2d &SegAB, int32 AID, int32 BID, TArray<FSegmentPoint>& Hits, double Tol)
{
    int num_hits = 0;

    for (int VID : Graph.VertexIndices())
    {
        if (Graph.GetVtxEdgeCount(VID) > 0 || VID == AID || VID == BID) // if it's an existing edge or on the currently added edge, it's not floating so skip it
        {
            continue;
        }

        FVector2d V = Graph.GetVertex(VID);
        double T;
        double DSQ = SegAB.DistanceSquared(V, T);
        if (DSQ < Tol*Tol)
        {
            Hits.Add(FSegmentPoint{ T, VID });
            num_hits++;
        }
    }

    return num_hits > 0;
}
