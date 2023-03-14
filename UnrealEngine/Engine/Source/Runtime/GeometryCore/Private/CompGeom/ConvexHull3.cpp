// Copyright Epic Games, Inc. All Rights Reserved.

// Adaptation/Port of GTEngine's ConvexHull3 algorithm;
// ref: Engine\Plugins\Runtime\GeometryProcessing\Source\GeometryAlgorithms\Private\ThirdParty\GTEngine\Mathematics\GteConvexHull3.h
// ExtremePoints::Init adapted from GteVector3.h's IntrinsicsVector3
// ref: Engine\Plugins\Runtime\GeometryProcessing\Source\GeometryAlgorithms\Private\ThirdParty\GTEngine\Mathematics\GteVector3.h

#include "CompGeom/ConvexHull3.h"
#include "CompGeom/ExactPredicates.h"

#include "Async/ParallelFor.h"

namespace UE
{
namespace Geometry
{


template<typename RealType>
void TExtremePoints3<RealType>::Init(int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc, RealType Epsilon)
{
	TVector<RealType> FirstPoint;
	int FirstPtIdx = -1;
	for (int FirstPtIdxTest = 0; FirstPtIdxTest < NumPoints; FirstPtIdxTest++)
	{
		if (FilterFunc(FirstPtIdxTest))
		{
			FirstPoint = GetPointFunc(FirstPtIdxTest);
			FirstPtIdx = FirstPtIdxTest;
			break;
		}
	}
	if (FirstPtIdx == -1)
	{
		// no points passed filter
		Dimension = 0;
		return;
	}

	TVector<RealType> Min = GetPointFunc(FirstPtIdx), Max = GetPointFunc(FirstPtIdx);
	FIndex3i IndexMin(FirstPtIdx, FirstPtIdx, FirstPtIdx), IndexMax(FirstPtIdx, FirstPtIdx, FirstPtIdx);
	for (int Idx = FirstPtIdx + 1; Idx < NumPoints; Idx++)
	{
		if (!FilterFunc(Idx))
		{
			continue;
		}
		for (int Dim = 0; Dim < 3; Dim++)
		{
			RealType Val = GetPointFunc(Idx)[Dim];
			if (Val < Min[Dim])
			{
				Min[Dim] = Val;
				IndexMin[Dim] = Idx;
			}
			else if (Val > Max[Dim])
			{
				Max[Dim] = Val;
				IndexMax[Dim] = Idx;
			}
		}
	}

	RealType MaxRange = Max[0] - Min[0];
	int MaxRangeDim = 0;
	for (int Dim = 1; Dim < 3; Dim++)
	{
		RealType Range = Max[Dim] - Min[Dim];
		if (Range > MaxRange)
		{
			MaxRange = Range;
			MaxRangeDim = Dim;
		}
	}
	Extreme[0] = IndexMin[MaxRangeDim];
	Extreme[1] = IndexMax[MaxRangeDim];

	// all points within box of Epsilon extent; Dimension must be 0
	if (MaxRange <= Epsilon)
	{
		Dimension = 0;
		Extreme[3] = Extreme[2] = Extreme[1] = Extreme[0];
		return;
	}

	Origin = GetPointFunc(Extreme[0]);
	Basis[0] = GetPointFunc(Extreme[1]) - Origin;
	Normalize(Basis[0]);

	// find point furthest from the line formed by the first two extreme points
	{
		TLine3<RealType> Basis0Line(Origin, Basis[0]);
		RealType MaxDistSq = 0;
		for (int Idx = FirstPtIdx; Idx < NumPoints; Idx++)
		{
			if (!FilterFunc(Idx))
			{
				continue;
			}
			RealType DistSq = Basis0Line.DistanceSquared(GetPointFunc(Idx));
			if (DistSq > MaxDistSq)
			{
				MaxDistSq = DistSq;
				Extreme[2] = Idx;
			}
		}

		// Nearly collinear points
		if (TMathUtil<RealType>::Sqrt(MaxDistSq) <= Epsilon * MaxRange)
		{
			Dimension = 1;
			Extreme[3] = Extreme[2] = Extreme[1];
			return;
		}
	}


	Basis[1] = GetPointFunc(Extreme[2]) - Origin;
	// project Basis[1] to be orthogonal to Basis[0]
	Basis[1] -= (Basis[0].Dot(Basis[1])) * Basis[0];
	if (!Basis[1].Normalize(Epsilon)) // points too collinear to form a valid basis
	{
		Dimension = 1;
		Extreme[3] = Extreme[2] = Extreme[1];
		return;
	}
	Basis[2] = Basis[0].Cross(Basis[1]);
	if (!Basis[2].Normalize(Epsilon)) // points too collinear to form a valid basis
	{
		Dimension = 1;
		Extreme[3] = Extreme[2] = Extreme[1];
		return;
	}


	{
		TPlane3<RealType> Plane(Basis[2], Origin);
		RealType MaxDist = 0, MaxSign = 0;
		for (int Idx = FirstPtIdx; Idx < NumPoints; Idx++)
		{
			if (!FilterFunc(Idx))
			{
				continue;
			}
			RealType DistSigned = (RealType)Plane.DistanceTo(GetPointFunc(Idx));
			RealType Dist = TMathUtil<RealType>::Abs(DistSigned);
			if (Dist > MaxDist)
			{
				MaxDist = Dist;
				MaxSign = TMathUtil<RealType>::Sign(DistSigned);
				Extreme[3] = Idx;
			}
		}

		// Nearly coplanar points
		if (MaxDist <= Epsilon * MaxRange)
		{
			Dimension = 2;
			Extreme[3] = Extreme[2];
			return;
		}

		// make sure the tetrahedron is CW-oriented
		if (MaxSign > 0)
		{
			Swap(Extreme[3], Extreme[2]);
		}
	}

	Dimension = 3;
}


template<typename RealType>
struct FHullConnectivity
{

	struct FVisiblePoints
	{
		TArray<int32> Indices;
		int32 MaxIdx = -1; // Note: an index into the source point array, not into the above Indices array
		// Max value of the Orient3D predicate, indicating the farthest point outside; values less than zero are inside the hull so not considered
		double MaxValue = 0;

		void AddPt(int32 Idx, double Value)
		{
			if (Value > MaxValue)
			{
				MaxValue = Value;
				MaxIdx = Idx;
			}
			Indices.Add(Idx);
		}

		int32 Num()
		{
			return Indices.Num();
		}

		int32 MaxPt()
		{
			return MaxIdx;
		}

		void Reset()
		{
			Indices.Reset();
			MaxIdx = -1;
			MaxValue = 0;
		}
	};

	TArray<FIndex3i> TriNeighbors;
	TArray<FVisiblePoints> VisiblePoints;
	TSet<int32> TrisWithPoints;

	/**
	 * Fully build neighbor connectivity; only on the initial tet, 
	 * as after that we can incrementally update connectivity w/ mesh changes
	 */
	void BuildNeighbors(const TArray<FIndex3i>& Triangles)
	{
		TMap<FIndex2i, int32> EdgeToTri;
		for (int32 TriIdx = 0; TriIdx < Triangles.Num(); TriIdx++)
		{
			const FIndex3i& Tri = Triangles[TriIdx];
			for (int32 LastIdx = 2, Idx = 0; Idx < 3; LastIdx = Idx++)
			{
				EdgeToTri.Add(FIndex2i(Tri[LastIdx], Tri[Idx]), TriIdx);
			}
		}
		TriNeighbors.SetNum(Triangles.Num());
		for (int32 TriIdx = 0; TriIdx < Triangles.Num(); TriIdx++)
		{
			const FIndex3i& Tri = Triangles[TriIdx];
			for (int32 LastIdx = 2, Idx = 0; Idx < 3; LastIdx = Idx++)
			{
				FIndex2i BackEdge(Tri[Idx], Tri[LastIdx]);
				int32* Val = EdgeToTri.Find(BackEdge);
				TriNeighbors[TriIdx][LastIdx] = Val ? *Val : -1;
			}
		}
	}

	/**
	 * @param ValueOut The volume of the tetrahedron formed by TriPts and Pt
	 * @return true if Pt is on the 'positive' side of the triangle
	 */
	bool IsVisible(const TVector<RealType> TriPts[3], const TVector<RealType>& Pt, double& ValueOut)
	{
		ValueOut = ExactPredicates::Orient3<RealType>(TriPts[0], TriPts[1], TriPts[2], Pt);
		return ValueOut > 0;
	}

	/**
	 * Set triangle positions to prep for IsVisible calls
	 */
	void SetTriPts(const FIndex3i& Tri, TFunctionRef<TVector<RealType>(int32)> GetPointFunc,  TVector<RealType> TriPtsOut[3])
	{
		TriPtsOut[0] = GetPointFunc(Tri[0]);
		TriPtsOut[1] = GetPointFunc(Tri[1]);
		TriPtsOut[2] = GetPointFunc(Tri[2]);
	}

	/**
	 * Bucket all points based on which triangles can see them
	 */
	void InitVisibility(const TArray<FIndex3i>& Triangles, int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc)
	{
		VisiblePoints.SetNum(Triangles.Num());

		TVector<RealType> TriPts[3];
		TVector<RealType> Pt;
		for (int32 PtIdx = 0; PtIdx < NumPoints; PtIdx++)
		{
			if (!FilterFunc(PtIdx))
			{
				continue;
			}
			Pt = GetPointFunc(PtIdx);
			for (int32 TriIdx = 0; TriIdx < Triangles.Num(); TriIdx++)
			{
				SetTriPts(Triangles[TriIdx], GetPointFunc, TriPts);
				double Value;
				if (IsVisible(TriPts, Pt, Value))
				{
					if (!VisiblePoints[TriIdx].Num())
					{
						TrisWithPoints.Add(TriIdx);
					}
					VisiblePoints[TriIdx].AddPt(PtIdx, Value);
				}
			}
		}
	}

	/**
	 * Update any neighbor "back pointers" to TriIdx to instead point to NewIdx
	 * Used during triangle deletion when we swap a triangle to a new index
	 */
	void UpdateNeighbors(int32 TriIdx, int32 NewIdx)
	{
		FIndex3i Neighbors = TriNeighbors[TriIdx];
		for (int32 Idx = 0; Idx < 3; Idx++)
		{
			int32 NbrIdx = Neighbors[Idx];
			if (NbrIdx > -1 && NbrIdx < TriNeighbors.Num())
			{
				FIndex3i& NbrNbrs = TriNeighbors[NbrIdx];
				for (int SubIdx = 0; SubIdx < 3; SubIdx++)
				{
					if (NbrNbrs[SubIdx] == TriIdx)
					{
						NbrNbrs[SubIdx] = NewIdx;
						break; // should only be one back-pointer per neighbor (or sometimes zero if it was already partially unhooked)
					}
				}
			}
		}
	}

	/**
	 * Rewrite one neighbor connection (and NOT the backwards connection)
	 *
	 * @param Triangles All triangles
	 * @param TriIdx The triangle to update
	 * @param EdgeFirstVertex The first vertex of the edge that should be connected to the new neighbor
	 * @param NewNbrIdx The new neighbor to connect to
	 */
	void UpdateNeighbor(TArray<FIndex3i>& Triangles, int32 TriIdx, int32 EdgeFirstVertex, int32 NewNbrIdx)
	{
		const FIndex3i& Triangle = Triangles[TriIdx];
		for (int32 Idx = 0; Idx < 3; Idx++)
		{
			if (Triangle[Idx] == EdgeFirstVertex)
			{
				TriNeighbors[TriIdx][Idx] = NewNbrIdx;
				return;
			}
		}
		check(false); // EdgeFirstVertex wasn't found in triangle; invalid UpdateNeighbor call
	}

	/**
	 * Debugging aid to validate the neighbor connectivity
	 */
	bool ValidateConnectivity(TArray<FIndex3i>& Triangles, TSet<int32> Skip = TSet<int32>())
	{
		TMap<FIndex2i, int32> EdgeToTri;
		for (int32 TriIdx = 0; TriIdx < Triangles.Num(); TriIdx++)
		{
			if (Skip.Contains(TriIdx))
			{
				continue;
			}
			const FIndex3i& Tri = Triangles[TriIdx];
			for (int32 LastIdx = 2, Idx = 0; Idx < 3; LastIdx = Idx++)
			{
				EdgeToTri.Add(FIndex2i(Tri[LastIdx], Tri[Idx]), TriIdx);
			}
		}
		for (int32 TriIdx = 0; TriIdx < Triangles.Num(); TriIdx++)
		{
			if (Skip.Contains(TriIdx))
			{
				continue;
			}
			const FIndex3i& Tri = Triangles[TriIdx];
			const FIndex3i& Nbrs = TriNeighbors[TriIdx];
			for (int32 LastIdx = 2, Idx = 0; Idx < 3; LastIdx = Idx++)
			{
				FIndex2i BackEdge = FIndex2i(Tri[Idx], Tri[LastIdx]);
				int32* FoundNbr = EdgeToTri.Find(BackEdge);
				if (!FoundNbr || Nbrs[LastIdx] != *FoundNbr)
				{
					ensure(false);
					return false;
				}
			}
		}

		return true;
	}

	/** 
	 * Remove a triangle and update connectivity accordingly.  Removes by swapping, and updates references as needed.
	 * Note: removes a whole set at a time, because indices change during deletion
	 *
	 * @param Triangles All triangles
	 * @param ToDelete All triangles to be deleted
	 */
	void DeleteTriangles(TArray<FIndex3i>& Triangles, const TSet<int32>& ToDelete)
	{
		TSet<int32> Moved; // indices of deleted tris that non-delete triangles have been moved into, must not be deleted
		for (int32 TriIdx : ToDelete)
		{
			checkSlow(!Moved.Contains(TriIdx));

			// skip triangles we already greedy-deleted while looking for the LastIdx in a previous iter
			if (TriIdx >= Triangles.Num())
			{
				continue;
			}
			checkSlow(Triangles.Num() == TriNeighbors.Num());
			checkSlow(Triangles.Num() == VisiblePoints.Num());

			// remove back-refs
			UpdateNeighbors(TriIdx, -1);

			int32 LastIdx = Triangles.Num() - 1;
			// greedily remove any triangles from the end rather than swapping them back
			while (TriIdx < LastIdx && (ToDelete.Contains(LastIdx) && !Moved.Contains(LastIdx)))
			{
				TrisWithPoints.Remove(LastIdx);
				UpdateNeighbors(LastIdx, -1);
				LastIdx--;
			}

			// found a non-delete tri at end to swap in
			if (TriIdx < LastIdx)
			{
				Moved.Add(TriIdx); // track so we don't later "greedy delete" the now non-delete tri
				UpdateNeighbors(LastIdx, TriIdx);
				TriNeighbors[TriIdx] = TriNeighbors[LastIdx];
				Triangles[TriIdx] = Triangles[LastIdx];
				VisiblePoints[TriIdx] = MoveTemp(VisiblePoints[LastIdx]);
				if (TrisWithPoints.Remove(LastIdx))
				{
					TrisWithPoints.Add(TriIdx);
				}
			}
			else // this was the last tri, just let it pop off
			{
				TrisWithPoints.Remove(TriIdx);
			}
			Triangles.SetNum(LastIdx, false);
			TriNeighbors.SetNum(LastIdx, false);
			VisiblePoints.SetNum(LastIdx, false);
		}
	}

	/**
	 * @return A triangle index and visible-from-that-triangle point index that can be added to the hull next
	 */
	FIndex2i ChooseVisiblePoint()
	{
		FIndex2i FoundTriPointPair(-1, -1);
		for (int32 TriIdx : TrisWithPoints)
		{
			ensure(VisiblePoints[TriIdx].Num() > 0);

			FoundTriPointPair[0] = TriIdx;
			// choose the "max point" -- the point with the largest volume when it makes a tetrahedron w/ the triangle
			FoundTriPointPair[1] = VisiblePoints[TriIdx].MaxPt();
			break;
		}
		return FoundTriPointPair;
	}

	/**
	 * Helper struct carrying enough information to describe triangles that will be added when we UpdateHullWithNewPoint
	 */
	struct FNewTriangle
	{
		FNewTriangle()
		{}
		FNewTriangle(int32 V0, int32 V1, int32 ConnectedTri) : EdgeVertices(V0, V1), ConnectedTri(ConnectedTri)
		{}

		// The edge the new point will connect to
		FIndex2i EdgeVertices;
		// The neighboring triangle across that edge
		int32 ConnectedTri;
	};

	/**
	 * Recursive traversal strategy that starts from a 'visible' triangle and walks to the connected set of all visible triangles, s.t. it visits the boundary in order
	 * See explanation here: http://algolist.ru/maths/geom/convhull/qhull3d.php
	 *
	 * @param Triangles All triangles
	 * @param GetPointFunc Point access function
	 * @param Pt Current 'eye point'; triangles will be deleted if this point is on the 'positive' side of the triangle
	 * @param NewlyUnclaimed (Output) As we mark triangles for deletion, we move the visible points stored on the triangles to this giant list of points that need to be re-assigned (or culled) wrt the new triangles
	 * @param ToDelete (Output) Set of all triangles that we will delete
	 * @param ToAdd (Output) Info required to add and connect up new triangles to the eye point
	 * @param TriIdx Index of triangle to currently process
	 * @param CrossedEdgeFirstVertex The first vertex of the edge that was 'crossed over' to traverse to this TriIdx, or -1 for the initial call
	 * @return false if triangle is beyond horizon (so we don't need to search through it / remove it), true otherwise
	 */
	bool HorizonHelper(const TArray<FIndex3i>& Triangles, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, const TVector<RealType>& Pt, TArray<int32>& NewlyUnclaimed, TSet<int32>& ToDelete, TArray<FNewTriangle>& ToAdd, int32 TriIdx, int32 CrossedEdgeFirstVertex)
	{
		// if it's not the first triangle, crossed edge should be set and we should check if the triangle is visible / actually needs to be replaced
		if (CrossedEdgeFirstVertex != -1)
		{
			TVector<RealType> TriPts[3];
			SetTriPts(Triangles[TriIdx], GetPointFunc, TriPts);
			double UnusedValue;
			if (!IsVisible(TriPts, Pt, UnusedValue))
			{
				return false;
			}
		}

		// track the triangle as needing deletion; for simplicity wait until after traversal to actually delete
		ToDelete.Add(TriIdx); // TODO: could we delete as we go rather than keep this set?  seems tricky

		// clean out the visible points right away
		if (VisiblePoints[TriIdx].Num() > 0)
		{
			TrisWithPoints.Remove(TriIdx);
			NewlyUnclaimed.Append(VisiblePoints[TriIdx].Indices);
			VisiblePoints[TriIdx].Reset();
		}
		

		FIndex3i Tri = Triangles[TriIdx];
		int32 FirstOff = 0, OffMax = 3; // Cross all three edges for first triangle
		if (CrossedEdgeFirstVertex > -1) // Cross the two edges we didn't 'come from' for all other triangles
		{
			FirstOff = Tri.IndexOf(CrossedEdgeFirstVertex) + 1; // index of the crossed edge wrt this triangle
			OffMax = FirstOff + 3;
		}
		for (int32 EdgeIdxOff = FirstOff; EdgeIdxOff < OffMax; EdgeIdxOff++)
		{
			int32 AcrossTri = TriNeighbors[TriIdx][EdgeIdxOff % 3];
			if (ToDelete.Contains(AcrossTri))
			{
				continue;
			}
			// second vertex of the edge (should be first seen when looking for the edge on opposite side)
			int32 EdgeSecondVertex = Tri[(EdgeIdxOff + 1) % 3];
			if (!HorizonHelper(Triangles, GetPointFunc, Pt, NewlyUnclaimed, ToDelete, ToAdd, AcrossTri, EdgeSecondVertex))
			{
				int32 EdgeFirstVertex = Tri[EdgeIdxOff % 3];
				ToAdd.Add(FNewTriangle(EdgeFirstVertex, EdgeSecondVertex, AcrossTri));
			}
		}

		return true;
	}

	void UpdateHullWithNewPoint(TArray<FIndex3i>& Triangles, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, int32 StartTriIdx, int32 PtIdx)
	{
		//ValidateConnectivity(Triangles);

		TVector<RealType> Pt = GetPointFunc(PtIdx);

		// Call the recursive helper to find all the tris we need to delete + the 'horizon' of edges that will form new triangles
		TArray<int32> NewlyUnclaimed;
		TSet<int32> ToDelete;
		TArray<FNewTriangle> ToAdd;
		HorizonHelper(Triangles, GetPointFunc, Pt, NewlyUnclaimed, ToDelete, ToAdd, StartTriIdx, -1);

		// Connect up all the horizon triangles
		int32 NewTriStart = Triangles.Num();
		int32 NumAdd = ToAdd.Num();
		TVector<RealType> TriPts[3];
		for (int32 AddIdx = 0; AddIdx < NumAdd; AddIdx++)
		{
			const FNewTriangle& TriData = ToAdd[AddIdx];
			int32 NewTriIdx = Triangles.Add(FIndex3i(PtIdx, TriData.EdgeVertices.A, TriData.EdgeVertices.B));
			int32 PrevTriIdx = NewTriStart + ((AddIdx - 1 + NumAdd) % NumAdd);
			int32 NextTriIdx = NewTriStart + ((AddIdx + 1) % NumAdd);

			int32 AcrossTriIdx = TriData.ConnectedTri;
			TriNeighbors.Add(FIndex3i(PrevTriIdx, AcrossTriIdx, NextTriIdx)); // TODO: is this the right order? need to validate!
			UpdateNeighbor(Triangles, AcrossTriIdx, TriData.EdgeVertices.B, NewTriIdx);
			FVisiblePoints& Visible = VisiblePoints.Emplace_GetRef();
			SetTriPts(Triangles[NewTriIdx], GetPointFunc, TriPts);
			// claim any claim-able points
			for (int32 UnclaimedIdx = 0; UnclaimedIdx < NewlyUnclaimed.Num(); UnclaimedIdx++)
			{
				int32 UnPtIdx = NewlyUnclaimed[UnclaimedIdx];
				double Value;
				if (IsVisible(TriPts, GetPointFunc(UnPtIdx), Value))
				{
					Visible.AddPt(UnPtIdx, Value);
					NewlyUnclaimed.RemoveAtSwap(UnclaimedIdx, 1, false);
					UnclaimedIdx--;
					continue;
				}
			}
			if (Visible.Num() > 0)
			{
				TrisWithPoints.Add(NewTriIdx);
			}
		}

		//ValidateConnectivity(Triangles, ToDelete);


		// do the deletions *last* so we don't invalidate tri indices in ToAdd
		DeleteTriangles(Triangles, ToDelete);

		//ValidateConnectivity(Triangles);
	}
};



template<class RealType>
bool TConvexHull3<RealType>::Solve(int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc)
{
	Hull.Reset();
	NumHullPoints = 0;

	TExtremePoints3<RealType> InitialTet(NumPoints, GetPointFunc, FilterFunc);
	Dimension = InitialTet.Dimension;
	if (Dimension < 3)
	{
		if (Dimension == 1)
		{
			Line = TLine3<RealType>(InitialTet.Origin, InitialTet.Basis[0]);
		}
		else if (Dimension == 2)
		{
			Plane = TPlane3<RealType>(InitialTet.Basis[2], InitialTet.Origin);
		}
		return false;
	}

	// safety check; seems possible the InitialTet chosen points were actually coplanar, because it was constructed w/ inexact math
	if (ExactPredicates::Orient3<RealType>(GetPointFunc(InitialTet.Extreme[0]), GetPointFunc(InitialTet.Extreme[1]), GetPointFunc(InitialTet.Extreme[2]), GetPointFunc(InitialTet.Extreme[3])) == 0)
	{
		Plane = TPlane3<RealType>(InitialTet.Basis[2], InitialTet.Origin);
		Dimension = 2;
		return false;
	}

	// Add triangles from InitialTet
	Hull.Add(FIndex3i(InitialTet.Extreme[1], InitialTet.Extreme[2], InitialTet.Extreme[3]));
	Hull.Add(FIndex3i(InitialTet.Extreme[0], InitialTet.Extreme[3], InitialTet.Extreme[2]));
	Hull.Add(FIndex3i(InitialTet.Extreme[0], InitialTet.Extreme[1], InitialTet.Extreme[3]));
	Hull.Add(FIndex3i(InitialTet.Extreme[0], InitialTet.Extreme[2], InitialTet.Extreme[1]));

	NumHullPoints = 4;

	FHullConnectivity<RealType> Connectivity;
	Connectivity.BuildNeighbors(Hull);
	Connectivity.InitVisibility(Hull, NumPoints, GetPointFunc, FilterFunc);
	while (true)
	{
		if (Progress && (NumHullPoints % 100) == 0 && Progress->Cancelled())
		{
			return false;
		}
		FIndex2i Visible = Connectivity.ChooseVisiblePoint();
		if (Visible.A == -1)
		{
			break;
		}
		NumHullPoints++;
		Connectivity.UpdateHullWithNewPoint(Hull, GetPointFunc, Visible.A, Visible.B);
	}

	if (bSaveTriangleNeighbors)
	{
		HullNeighbors = MoveTemp(Connectivity.TriNeighbors);
	}

	return true;
}

template struct TExtremePoints3<float>;
template struct TExtremePoints3<double>;
template class TConvexHull3<float>;
template class TConvexHull3<double>;

} // end namespace UE::Geometry
} // end namespace UE