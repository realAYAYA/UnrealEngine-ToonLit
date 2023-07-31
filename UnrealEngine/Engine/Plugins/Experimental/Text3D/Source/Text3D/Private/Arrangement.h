// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Curve/DynamicGraph2.h"
#include "Spatial/PointHashGrid2.h"
#include "Intersection/IntrSegment2Segment2.h"

/**
 * @brief Modified copy of FArrangement2d, has edge directions
 */
struct FArrangement final
{
	using FIndex2i = UE::Geometry::FIndex2i;
	using FSegment2f = UE::Geometry::FSegment2f;
	using FSegment2d = UE::Geometry::FSegment2d;
	using FAxisAlignedBox2f = UE::Geometry::FAxisAlignedBox2f;
	
    UE::Geometry::FDynamicGraph2d Graph;
    UE::Geometry::TPointHashGrid2d<int> PointHash;
    TMap<int, bool> Directions; //bool - from A to B
	const double VertexSnapTol = 0.001;


    FArrangement(const FAxisAlignedBox2f& BoundsHint);
    void Insert(const FSegment2f& Segment);

protected:
    struct FSegmentPoint
    {
        double T;
        int VID;
    };

    struct FIntersection
    {
        int EID;
        int SideX;
        int SideY;
        UE::Geometry::FIntrSegment2Segment2d Intr;
    };


    int insert_point(const FVector2d &P, double Tol = 0);
    bool insert_segment(FVector2d A, FVector2d B, double Tol = 0);
    UE::Geometry::FIndex2i split_segment_at_t(int EID, double T, double Tol);
    int find_existing_vertex(FVector2d Pt);
    int find_nearest_vertex(FVector2d Pt, double SearchRadius, int IgnoreVID = -1);
    bool find_intersecting_edges(FVector2d A, FVector2d B, TArray<FIntersection>& Hits, double Tol = 0);
    bool find_intersecting_floating_vertices(const FSegment2d &SegAB, int32 AID, int32 BID, TArray<FSegmentPoint>& Hits, double Tol = 0);
};
