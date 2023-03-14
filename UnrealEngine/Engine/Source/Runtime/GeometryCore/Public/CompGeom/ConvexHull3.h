// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "HalfspaceTypes.h"
#include "IndexTypes.h"
#include "LineTypes.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MathUtil.h"
#include "PlaneTypes.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"
#include "Util/ProgressCancel.h"
#include "VectorTypes.h"

class FProgressCancel;

namespace UE {
namespace Geometry {

using namespace UE::Math;

/**
 * Helper class to find the dimensions spanned by a point cloud
 * and (if it spans 3 dimensions) the indices of four 'extreme' points
 * forming a (non-degenerate, volume > 0) tetrahedron
 *
 * The extreme points are chosen to be far apart, and are used as the starting point for
 * incremental convex hull construction.
 */
template<typename RealType>
struct GEOMETRYCORE_API TExtremePoints3
{
	int Dimension = 0;
	int Extreme[4]{ 0, 0, 0, 0 };

	// Coordinate frame spanned by input points
	TVector<RealType> Origin{ 0,0,0 };
	TVector<RealType> Basis[3]{ {0,0,0}, {0,0,0}, {0,0,0} };

	TExtremePoints3(int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc = [](int32 Idx) {return true; }, RealType Epsilon = TMathUtil<RealType>::Epsilon)
	{
		Init(NumPoints, GetPointFunc, FilterFunc, Epsilon);
	}

private:
	void Init(int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc, RealType Epsilon = TMathUtil<RealType>::Epsilon);
};

/**
 * Calculate the Convex Hull of a 3D point set as a Triangle Mesh
 */
template<typename RealType>
class GEOMETRYCORE_API TConvexHull3
{
public:

	/// Whether neighbors for the hull triangles should be computed/saved.
	/// If true, can call GetTriangleNeighbors() after Solve().
	bool bSaveTriangleNeighbors = false;

	/**
	 * Generate convex hull as long as input is not degenerate
	 * If input is degenerate, this will return false, and caller can call GetDimension()
	 * to determine whether the points were coplanar, collinear, or all the same point
	 *
	 * @param NumPoints Number of points to consider
	 * @param GetPointFunc Function providing array-style access into points
	 * @param Filter Optional filter to include only a subset of the points in the output hull
	 * @return true if hull was generated, false if points span < 2 dimensions
	 */
	bool Solve(int32 NumPoints, TFunctionRef<TVector<RealType>(int32)> GetPointFunc, TFunctionRef<bool(int32)> FilterFunc = [](int32 Idx) {return true;});

	/**
	 * Generate convex hull as long as input is not degenerate
	 * If input is degenerate, this will return false, and caller can call GetDimension()
	 * to determine whether the points were collinear, or all the same point
	 *
	 * @param Points Array of points to consider
	 * @param FilterFunc Optional filter to include only a subset of the points in the output hull
	 * @return true if hull was generated, false if points span < 2 dimensions
	 */
	template<typename VectorType>
	bool Solve(TArrayView<const VectorType> Points, TFunctionRef<bool(int32)> FilterFunc)
	{
		return Solve(Points.Num(), [&Points](int32 Idx)
			{
				return Points[Idx];
			}, FilterFunc);
	}

	// default FilterFunc version of the above Solve(); workaround for clang bug https://bugs.llvm.org/show_bug.cgi?id=25333
	/**
	 * Generate convex hull as long as input is not degenerate
	 * If input is degenerate, this will return false, and caller can call GetDimension()
	 * to determine whether the points were collinear, or all the same point
	 *
	 * @param Points Array of points to consider
	 * @return true if hull was generated, false if points span < 2 dimensions
	 */
	template<typename VectorType>
	bool Solve(TArrayView<const VectorType> Points)
	{
		return Solve(Points.Num(), [&Points](int32 Idx)
			{
				return Points[Idx];
			}, [](int32 Idx) {return true;});
	}

	/** @return true if convex hull is available */
	bool IsSolutionAvailable() const
	{
		return Dimension == 3;
	}

	/**
	 * Call TriangleFunc for each triangle of the Convex Hull. The triangles index into the point set passed to Solve()
	 */
	void GetTriangles(TFunctionRef<void(FIndex3i)> TriangleFunc)
	{
		for (FIndex3i Triangle : Hull)
		{
			TriangleFunc(Triangle);
		}
	}

	/** @return convex hull triangles */
	TArray<FIndex3i> const& GetTriangles() const
	{
		return Hull;
	}

	/**
	 * Only valid if bSaveTriangleNeighbors was true when Solve() was called
	 * @return Neighbors of each hull triangle, in edge order -- i.e., Nbr.A is the triangle across edge(Tri.A, Tri.B)
	 */
	TArray<FIndex3i> const& GetTriangleNeighbors() const
	{
		ensure(bSaveTriangleNeighbors && Hull.Num() == HullNeighbors.Num());
		return HullNeighbors;
	}

	/** @return convex hull triangles by a move */
	TArray<FIndex3i>&& MoveTriangles()
	{
		return MoveTemp(Hull);
	}

	/** 
	 * Convert an already-computed convex hull into a halfspace representation
	 * Following the logic of ContainmentQueries3.h, all halfspaces are oriented "outwards,"
	 * so a point is inside the convex hull if it is outside of all halfspaces in the array
	 * 
	 * @param Points Array of points to consider
	 * @return Array of halfspaces
	 */
	template<typename VectorType>
	TArray<THalfspace3<RealType>> GetAsHalfspaces(TArrayView<const VectorType> Points) const
	{
		TArray<THalfspace3<RealType>> Halfspaces;
		for (FIndex3i Tri : Hull)
		{
			THalfspace3<RealType> TriHalfspace(Points[Tri.A], Points[Tri.B], Points[Tri.C]);
			Halfspaces.Add(TriHalfspace);
		}
		return Halfspaces;
	}

	/**
	 * Convert an already-computed convex hull into a halfspace representation
	 * Following the logic of ContainmentQueries3.h, all halfspaces are oriented "outwards,"
	 * so a point is inside the convex hull if it is outside of all halfspaces in the array
	 *
	 * @param GetPointFunc Function providing array-style access into points
	 * @return Array of halfspaces
	 */
	template<typename VectorType>
	TArray<THalfspace3<RealType>> GetAsHalfspaces(TFunctionRef<VectorType(int32)> GetPointFunc) const
	{
		TArray<THalfspace3<RealType>> Halfspaces;
		for (FIndex3i Tri : Hull)
		{
			THalfspace3<RealType> TriHalfspace(GetPointFunc(Tri.A), GetPointFunc(Tri.B), GetPointFunc(Tri.C));
			Halfspaces.Add(TriHalfspace);
		}
		return Halfspaces;
	}

	/**
	 * Empty any previously-computed convex hull data.  Frees the hull memory.
	 * Note: You do not need to call this before calling Generate() with new data.
	 */
	void Empty()
	{
		Dimension = 0;
		NumHullPoints = 0;
		Hull.Empty();
	}

	/**
	 * Number of dimensions spanned by the input points.
	 */
	inline int GetDimension() const
	{
		return Dimension;
	}
	/** @return If GetDimension() returns 1, this returns the line the data was on.  Otherwise result is not meaningful. */
	inline TLine3<RealType> const& GetLine() const
	{
		return Line;
	}
	/** @return If GetDimension() returns 2, this returns the plane the data was on.  Otherwise result is not meaningful. */
	inline TPlane3<RealType> const& GetPlane() const
	{
		return Plane;
	}

	/** @return Number of points on the output hull */
	int GetNumHullPoints() const
	{
		return NumHullPoints;
	}


	/** Set this to be able to cancel running operation */
	FProgressCancel* Progress = nullptr;

protected:

	int32 Dimension;
	TLine3<RealType> Line;
	TPlane3<RealType> Plane;

	int NumHullPoints = 0;
	TArray<FIndex3i> Hull;
	// We can optionally also return the hull triangle adjacencies
	TArray<FIndex3i> HullNeighbors;
	
};

typedef TExtremePoints3<float> FExtremePoints3f;
typedef TExtremePoints3<double> FExtremePoints3d;
typedef TConvexHull3<float> FConvexHull3f;
typedef TConvexHull3<double> FConvexHull3d;


} // end namespace UE::Geometry
} // end namespace UE

