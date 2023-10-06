// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoxTypes.h"
#include "Containers/Array.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "IndexTypes.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MathUtil.h"
#include "Misc/AssertionMacros.h"
#include "PlaneTypes.h"
#include "TransformTypes.h"
#include "VectorTypes.h"

namespace UE {
namespace Geometry {

// Forward declare fast winding tree for negative space sampling
template<class TriangleMeshType>
class TFastWindingTree;

// TODO: To support meshes where volume is not well defined (e.g., open boundaries or many self-intersecting parts), we'll need alternative error metrics
enum class EConvexErrorMethod
{
	//GeometryToHullRayDistance,
	//HullToGeometryDistance,
	//BothDistances,
	RelativeVolume
};

struct FNegativeSpaceSampleSettings
{
	FNegativeSpaceSampleSettings() = default;
	FNegativeSpaceSampleSettings(int32 TargetNumSamples, double MinSpacing, double ReduceRadiusMargin)
		: TargetNumSamples(TargetNumSamples), MinSpacing(MinSpacing), ReduceRadiusMargin(ReduceRadiusMargin)
	{}

	enum class ESampleMethod
	{
		Uniform
		// TODO: Consider e.g. sampling based on local search / floodfill
	};

	// Method used to place samples
	ESampleMethod SampleMethod = ESampleMethod::Uniform;

	// Target number of spheres to use to cover negative space. Intended as a rough guide; actual number used may be larger or smaller.
	int32 TargetNumSamples = 30;

	// Minimum desired spacing between sampling spheres; not a strictly enforced bound.
	double MinSpacing = 3.0;

	// Space to allow between spheres and actual surface
	double ReduceRadiusMargin = 3.0;

	// Ignore spheres with smaller radius than this
	double MinRadius = 10.0;

	// Make sure the settings values are in valid ranges
	void Sanitize()
	{
		TargetNumSamples = FMath::Max(1, TargetNumSamples);
		MinSpacing = FMath::Max(0.0, MinSpacing);
		ReduceRadiusMargin = FMath::Max(0.0, ReduceRadiusMargin);
		MinRadius = FMath::Max(0.0, MinRadius);
	}
};

// Define a volume with a set of spheres
class FSphereCovering
{
public:

	inline int32 Num() const
	{
		return Position.Num();
	}

	inline FVector3d GetCenter(int32 Idx) const
	{
		return Position[Idx];
	}

	inline double GetRadius(int32 Idx) const
	{
		return Radius[Idx];
	}

	// Add spheres covering the negative space of the given fast winding tree
	// @return true if any spheres were added
	GEOMETRYCORE_API bool AddNegativeSpace(const TFastWindingTree<FDynamicMesh3>& Spatial, const FNegativeSpaceSampleSettings& Settings);

	void Reset()
	{
		Position.Reset();
		Radius.Reset();
	}

private:
	// Sphere centers
	TArray<FVector3d> Position;
	// Sphere radii
	TArray<double> Radius;
};



class FConvexDecomposition3
{

public:

	FConvexDecomposition3() : ResultTransform(FTransformSRT3d::Identity())
	{
	}

	FConvexDecomposition3(const FDynamicMesh3& SourceMesh, bool bMergeEdges = true)
	{
		InitializeFromMesh(SourceMesh, bMergeEdges);
	}

	/**
	 * Initialize from convex hulls allows the caller to only use the hull merging phase of the algorithm
	 * @param NumHulls			Number of convex hulls in the initial decomposition
	 * @param HullVolumes		Function from Hull Index -> Hull Volume
	 * @param HullNumVertices	Function from Hull Index -> Hull Vertex Count
	 * @param HullVertices		Function from Hull Index, Vertex Index -> Hull Vertex Position
	 * @param Proximity			All the local proximities for the hulls. Hulls will not be merged unless they are connected by this proximity graph.
	 */
	GEOMETRYCORE_API void InitializeFromHulls(int32 NumHulls, TFunctionRef<double(int32)> HullVolumes, TFunctionRef<int32(int32)> HullNumVertices, TFunctionRef<FVector3d(int32, int32)> HullVertices, TArrayView<const TPair<int32, int32>> Proximity);

	GEOMETRYCORE_API void InitializeFromMesh(const FDynamicMesh3& SourceMesh, bool bMergeEdges);

	/**
	 * Initialize convex decomposition with a triangle index mesh
	 * @param Vertices			Vertex buffer for mesh to decompose
	 * @param Faces				Triangle buffer for mesh to decompose
	 * @param bMergeEdges		Whether to attempt to weld matching edges before computing the convex hull; this can help the convex decomposition find better cutting planes for meshes that have boundaries e.g. due to seams
	 * @param FaceVertexOffset	Indices from the Faces array are optionally offset by this value.  Useful e.g. to take slices of the multi-geometry vertex and face buffers of FGeometryCollection.
	 */
	GEOMETRYCORE_API void InitializeFromIndexMesh(TArrayView<const FVector3f> Vertices, TArrayView<const FIntVector> Faces, bool bMergeEdges, int32 FaceVertexOffset = 0);

	//
	// Settings
	//

	// Note we automatically transform the input to a centered-unit-cube space, and tolerances are measured in that space
	// In other words, distance-based tolerances are expressed as a fraction of the bounding box max axis
	
	// Threshold for considering two convex parts close enough to consider merging
	double ProximityTolerance = 1e-3;
	// Threshold for considering two components to be touching
	double ConnectedComponentTolerance = 1e-3;
	// Maximum 'on plane' distance for points during planar cuts
	double OnPlaneTolerance = 1e-7;

	// Scale factor for error on the largest axis; slightly favoring cutting on the largest axis helps get more consistent result when no initial cut will reduce error (e.g. a torus)
	double CutLargestAxisErrorScale = .99;

	// Angle at edge above which we take 5 sample planes instead of 3, to better cover the wider range (expressed in degrees)
	double ConvexEdgeAngleMoreSamplesThreshold = 30; 
	// Angle at edge above which we don't sample it as a convex edge (expressed in degrees)
	double ConvexEdgeAngleThreshold = 170;

	// A bias term to favor merging away 'too thin' parts before merging other parts, expressed as a volume in units s.t. the longest axis of the original input bounding box is 1 unit long
	// Because these merges *must* be done, it's generally safer to do them earlier, rather than hoping the error associated with them will go down after other merges are performed
	double BiasToRemoveTooThinParts = .1;

	// Maximum number of convex edges to sample for possible cutting planes when calling SplitWorst() to generate an initial convex decomposition
	// Larger values will cost more to run but can let the algorithm find a cleaner decomposition
	int32 MaxConvexEdgePlanes = 50;

	// TODO: Provide hull approximation options?


	/**
	 * Compute a decomposition with the desired number of hulls
	 * Note: A future version of this function may replace NumOutputHulls with MaxOutputHulls, but this version keeps both parameters for compatibility / consistent behavior.
	 * 
	 * @param NumOutputHulls		Number of convex hulls to use in the final convex decomposition
	 * @param NumAdditionalSplits	How far to go beyond the target number of hulls when initially the mesh into pieces -- larger values will require more computation but can find better convex decompositions
	 * @param ErrorTolerance		Stop splitting when hulls have error less than this (expressed in cm; will be cubed for volumetric error). Overrides NumOutputHulls if specified
	 * @param MinThicknessTolerance	Optionally specify a minimum thickness (in cm) for convex parts; parts below this thickness will always be merged away. Overrides NumOutputHulls and ErrorTolerance when needed
	 * @param MaxOutputHulls		If > 0, maximum number of convex hulls to generate. Overrides ErrorTolerance and TargetNumParts when needed
	 */
	void Compute(int32 NumOutputHulls, int32 NumAdditionalSplits = 10, double ErrorTolerance = 0.0, double MinThicknessTolerance = 0, int32 MaxOutputHulls = -1)
	{
		if (MaxOutputHulls > 0)
		{
			NumOutputHulls = FMath::Min(NumOutputHulls, MaxOutputHulls);
		}
		int32 TargetNumSplits = NumOutputHulls + NumAdditionalSplits;
		for (int32 SplitIdx = 0; SplitIdx < TargetNumSplits; SplitIdx++)
		{
			int32 NumNewParts = SplitWorst(bool(SplitIdx % 2), ErrorTolerance);
			if (NumNewParts == 0)
			{
				break;
			}
		}
		
		constexpr bool bAllowCompact = true;
		MergeBest(NumOutputHulls, ErrorTolerance, MinThicknessTolerance, bAllowCompact, false, MaxOutputHulls);
	}

	// Split the worst convex part, and return the increase in the total number of convex parts after splitting (can be more than 1 if result has multiple separate connected components)
	// Note: could return 0 if no splits were possible
	// @param bCanSkipUnreliableGeoVolumes		if true, don't split hulls where we have questionable geometry volume results, unless there is no hull with good geometry volume results
	GEOMETRYCORE_API int32 SplitWorst(bool bCanSkipUnreliableGeoVolumes = false, double ErrorTolerance = 0.0);

	// Merge the pairs of convex hulls in the decomposition that will least increase the error.  Intermediate results can be used across merges, so it is best to do all merges in one call.
	// Note: A future version of this function may replace NumOutputHulls with MaxOutputHulls, but this version keeps both parameters for compatibility / consistent behavior.
	// @param TargetNumParts		The target number of parts for the decomposition; will be overriden by non-default ErrorTolerance or MinPartThickness
	// @param ErrorTolerance		If > 0, continue to merge (if there are possible merges) until the resulting error would be greater than this value. Overrides TargetNumParts as the stopping condition.
	//								Note: ErrorTolerance is expressed in cm, and will be cubed for volumetric error.
	// @param MinPartThickness		Optionally specify a minimum thickness (in cm) for convex parts; parts below this thickness will always be merged away. Overrides TargetNumParts and ErrorTolerance when needed.
	//								Note: These parts may be further split so they can be merged into multiple hulls
	// @param bAllowCompact			Allow the algorithm to discard underlying geometry once it will no longer be used, resulting in a smaller representation & faster merges
	// @param bRequireHullTriangles	Require all hulls to have associated triangles after MergeBest is completed. (If using InitializeFromHulls, will need to triangulate any un-merged hulls.)
	// @param MaxOutputHulls		If > 0, maximum number of convex hulls to generate. Overrides ErrorTolerance and TargetNumParts when needed
	// @param OptionalNegativeSpace Optional representation of negative space that should not be covered by merges
	// @param OptionalNegativeSpaceTransform Optional transform from space of the convex hulls into the space of the sphere covering; if not provided, assume spheres are in the same coordinate space as the hulls
	// @return						The number of merges performed
	GEOMETRYCORE_API int32 MergeBest(int32 TargetNumParts, double ErrorTolerance = 0, double MinThicknessTolerance = 0, bool bAllowCompact = true, bool bRequireHullTriangles = false, int MaxOutputHulls = -1,
		const FSphereCovering* OptionalNegativeSpace = nullptr, const FTransform* OptionalTransformIntoNegativeSpace = nullptr);

	// simple helper to convert an error tolerance expressed in world space to a local-space volume tolerance
	inline double ConvertDistanceToleranceToLocalVolumeTolerance(double DistTolerance) const
	{
		double LocalDist = ConvertDistanceToleranceToLocalSpace(DistTolerance);
		double LocalVolume = LocalDist * LocalDist * LocalDist;
		return LocalVolume;
	}

	// simple helper to convert an error tolerance expressed in world space to local space
	inline double ConvertDistanceToleranceToLocalSpace(double DistTolerance) const
	{
		double ScaleFactor = ResultTransform.GetScale().GetMax();
		if (!ensure(ScaleFactor >= FMathd::Epsilon)) // If ScaleFactor is zero, ResultTransform is trying to collapse all the hulls to a degenerate space -- likely indicates an incorrect ResultTransform
		{
			ScaleFactor = FMathd::Epsilon;
		}
		double LocalDist = DistTolerance / ScaleFactor;
		return LocalDist;
	}

	// Compact the decomposition representation to the minimal needed for output: Just the hull vertices and triangles, no internal geometry
	// This will prevent some further processing on the representation; to ensure it is called only when the geometry is no longer needed,
	// instead of calling this directly, call MergeBest with bAllowCompact=true
	void Compact()
	{
		for (FConvexPart& Part : Decomposition)
		{
			Part.Compact();
		}
	}

	//
	// Results
	//

	int32 NumHulls() const
	{
		return Decomposition.Num();
	}

	/** @return convex hull triangles */
	TArray<FIndex3i> const& GetTriangles(int32 HullIdx) const
	{
		return Decomposition[HullIdx].HullTriangles;
	}

	template<typename RealType>
	TArray<TVector<RealType>> GetVertices(int32 HullIdx, bool bTransformedToOutput = true) const
	{
		TArray<TVector<RealType>> Vertices;
		Vertices.SetNum(Decomposition[HullIdx].InternalGeo.MaxVertexID());
		for (int32 VID = 0; VID < Decomposition[HullIdx].InternalGeo.MaxVertexID(); VID++)
		{
			if (Decomposition[HullIdx].InternalGeo.IsVertex(VID))
			{
				Vertices[VID] = (TVector<RealType>) ResultTransform.TransformPosition(Decomposition[HullIdx].InternalGeo.GetVertex(VID));
			}
		}
		return Vertices;
	}

	FDynamicMesh3 GetHullMesh(int32 HullIdx) const
	{
		FDynamicMesh3 HullMesh;
		for (int32 VID = 0; VID < Decomposition[HullIdx].InternalGeo.MaxVertexID(); VID++)
		{
			if (Decomposition[HullIdx].InternalGeo.IsVertex(VID))
			{
				HullMesh.AppendVertex(ResultTransform.TransformPosition(Decomposition[HullIdx].InternalGeo.GetVertex(VID)));
			}
			else
			{
				HullMesh.AppendVertex(FVector3d::ZeroVector);
			}
		}
		for (const FIndex3i& Tri : Decomposition[HullIdx].HullTriangles)
		{
			HullMesh.AppendTriangle(Tri);
		}
		return HullMesh;
	}

	// Get the non-hull geometry underlying a part of the convex decomposition
	const FDynamicMesh3& GetInternalMesh(int32 HullIdx) const
	{
		return Decomposition[HullIdx].InternalGeo;
	}

	// Representation of a convex hull in the decomposition + associated information to help further split or merge
	struct FConvexPart
	{
		FConvexPart() {}
		FConvexPart(const FDynamicMesh3& SourceMesh, bool bMergeEdges, FTransformSRT3d& TransformOut);
		FConvexPart(TArrayView<const FVector3f> Vertices, TArrayView<const FIntVector3> Faces, bool bMergeEdges, FTransformSRT3d& TransformOut, int32 FaceVertexOffset = 0);

		// Allow direct construction of a compact part (e.g. to allow construction of a pre-existing convex hull)
		FConvexPart(bool bIsCompact) : bIsCompact(bIsCompact)
		{}

		void Reset()
		{
			InternalGeo.Clear();
			HullTriangles.Reset();
			HullPlanes.Reset();
			HullVolume = 0;
			GeoVolume = 0;
			SumHullsVolume = -FMathd::MaxReal;
			GeoCenter = FVector3d::ZeroVector;
			Bounds = FAxisAlignedBox3d::Empty();
			HullError = FMathd::MaxReal;
			bIsCompact = false;
			bFailed = false;
			bGeometryVolumeUnreliable = false;
			bMustMerge = false;
		}

		// Underlying geometry represented by the convex part
		// Note: If IsCompact(), this will only store vertices
		FDynamicMesh3 InternalGeo;

		inline bool IsCompact() const
		{
			return bIsCompact;
		}

		inline bool IsFailed() const
		{
			return bFailed;
		}

		TArray<FIndex3i> HullTriangles;
		TArray<FPlane3d> HullPlanes; // 1:1 with HullTriangles

		// Measurements of the geo and hull, to be used when evaluating potential further splits
		double HullVolume = 0, GeoVolume = 0;
		double SumHullsVolume = -FMathd::MaxReal; // Sum of volume of any hulls that have been merged to form this hull; only valid if the FConvexPart was created by merging
		FVector3d GeoCenter = FVector3d::ZeroVector; // Some central point of the geometry (e.g., an average of vertices)
		FAxisAlignedBox3d Bounds;
		bool bGeometryVolumeUnreliable = false;

		// Flag indicating the hull should be merged into another part during a MergeBest() call
		bool bMustMerge = false;

		// Approximate value indicating how badly the hull mismatches the input geometry
		double HullError = FMathd::MaxReal;

		void ComputeHullPlanes();

		// get the depth of a point inside the hull (positive values are inside the hull, negative outside)
		double GetHullDepth(FVector3d Pt)
		{
			int32 NumPlanes = HullPlanes.Num();
			ensure(HullTriangles.Num() == NumPlanes);
			if (NumPlanes == 0)
			{
				return 0;
			}
			double MaxDist = HullPlanes[0].DistanceTo(Pt);
			for (int32 Idx = 0; Idx < NumPlanes; Idx++)
			{
				const FPlane3d& Plane = HullPlanes[Idx];
				double Dist = Plane.DistanceTo(Pt);
				MaxDist = FMath::Max(Dist, MaxDist);
			}
			return -MaxDist;
		}

		// Helper to create hull after InternalGeo is set
		bool ComputeHull(bool bComputePlanes = true);

		// Helper to compute volumes, centroid, bounds, etc after the convex part is initialized
		void ComputeStats();

		void Compact();

	protected:
		bool bIsCompact = false;
		bool bFailed = false;

	private:
		// helper to initialize the part from the already-set InternalGeo member; used to support constructors
		void InitializeFromInternalGeo(bool bMergeEdges, FTransformSRT3d& TransformOut);
	};

	// All convex hulls parts in the convex decomposition
	// Note this must be an indirect array, not a TArray, because FConvexPart holds an FDynamicMesh3 mesh, which is not trivially movable
	TIndirectArray<FConvexPart> Decomposition;

	/// Transform taking the result hull vertices back to the original space of the inputs; automatically applied by GetVertices()
	FTransformSRT3d ResultTransform = FTransformSRT3d::Identity();

	// Edge in a graph of merge candidates, indicating which Decomposition parts could be merged together
	// Note a proximity link does not guarantee that the geometry (or convex hulls) are touching
	struct FProximity
	{
		FProximity() = default;
		FProximity(const FIndex2i& Link, const FPlane3d& Plane, bool bPlaneSeparates) : Link(Link), Plane(Plane), bPlaneSeparates(bPlaneSeparates)
		{}

		FIndex2i Link{ -1, -1 };
		
		// the shared plane between the two hulls (because adjacent hulls are formed by planar cuts in the splitting step)
		// Note: Plane becomes invalid in the merge-up phase; it is used to accelerate filtering in the split-down phase
		FPlane3d Plane;
		
		// Whether the geometry is separated by Plane
		bool bPlaneSeparates = false;

		// Whether this merge can be allowed (e.g. has not been ruled out by negative space)
		bool bIsValidLink = true;
		
		// What the volume of the convex hull would be if we merged these two parts
		double MergedVolume = VolumeNotComputed();

		double GetMergeCost(const TIndirectArray<FConvexPart>& DecompositionIn) const
		{
			if (!HasMergedVolume() || !DecompositionIn.IsValidIndex(Link.A) || !DecompositionIn.IsValidIndex(Link.B))
			{
				return FMathd::MaxReal;
			}
			ensure(DecompositionIn[Link.A].SumHullsVolume != -FMathd::MaxReal && DecompositionIn[Link.B].SumHullsVolume != -FMathd::MaxReal);
			return MergedVolume - DecompositionIn[Link.A].SumHullsVolume - DecompositionIn[Link.B].SumHullsVolume;
		}

		bool HasMergedVolume() const
		{
			return MergedVolume != VolumeNotComputed();
		}
		void ClearMergedVolume()
		{
			MergedVolume = VolumeNotComputed();
		}

		constexpr double VolumeNotComputed() const { return FMathd::MaxReal; }
		
	};

	// All potential merge edges between Decomposition FConvexParts
	TArray<FProximity> Proximities;

	// Mapping from Decomposition indices to Proximity indices
	TMultiMap<int32, int32> DecompositionToProximity;

	// Helper function to delete a proximity relationship, and fix index references to account for the new array ordering
	// @param bDeleteMapReferences		If true, also update references in the DecompositionToProximity map
	GEOMETRYCORE_API void DeleteProximity(int32 ProxIdx, bool bDeleteMapReferences);

	// Helper function to update all proximity data after splitting an FConvexPart apart
	// @param SplitIdx				The index of the FConvexPart that has been split into multiple parts.  Now contains the first new part.
	// @param NewIdxStart			The first index in the Decomposition array of new FConvexParts aside from SplitIdx.  Everything from this to the end of the Decomposition array must be new.
	// @param CutPlane				The plane used for the split
	// @param SecondSideIdxStart	The first index in the Decomposition array of new FConvexParts that were on the opposite side of the plane from SplitIdx.  Will be different from NewIdxStart only if the first part was split into multiple components.
	// @param OrigHullVolume		The volume of the convex hull *before* the split was performed -- used to precompute the merge cost.
	GEOMETRYCORE_API void UpdateProximitiesAfterSplit(int32 SplitIdx, int32 NewIdxStart, FPlane3d CutPlane, int32 SecondSideIdxStart, double OrigHullVolume);
};

} // end namespace UE::Geometry
} // end namespace UE

