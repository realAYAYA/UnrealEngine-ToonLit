// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sampling/MeshSurfacePointSampling.h"

#include "VectorUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicVerticesOctree3.h"
#include "Async/ParallelFor.h"
#include "Tasks/Task.h"
#include "Math/RandomStream.h"

using namespace UE::Geometry;

namespace UELocal
{


// cache of per-triangle information for a triangle mesh
struct FTriangleInfoCache
{
	TArray<FVector3d> TriNormals;
	TArray<double> TriAreas;
	TArray<FFrame3d> TriFrames;
	TArray<FTriangle2d> UVTriangles;

	double TotalArea;

	template<typename MeshType>
	void InitializeForTriangleSet(const MeshType& SampleMesh)
	{
		TriNormals.SetNumUninitialized(SampleMesh.MaxTriangleID());
		TriAreas.SetNumUninitialized(SampleMesh.MaxTriangleID());
		TriFrames.SetNumUninitialized(SampleMesh.MaxTriangleID());
		UVTriangles.SetNumUninitialized(SampleMesh.MaxTriangleID());
				
		ParallelFor(SampleMesh.MaxTriangleID(), [&](int32 tid)
		{
			if (SampleMesh.IsTriangle(tid))
			{
				FVector3d A, B, C, Centroid;
				SampleMesh.GetTriVertices(tid, A, B, C);
				Centroid = (A + B + C) / 3.0;
				TriNormals[tid] = VectorUtil::NormalArea(A, B, C, TriAreas[tid]);
				TriFrames[tid] = FFrame3d(Centroid, TriNormals[tid]);
				UVTriangles[tid] = FTriangle2d(
					TriFrames[tid].ToPlaneUV(A), TriFrames[tid].ToPlaneUV(B), TriFrames[tid].ToPlaneUV(C));
			}
		});
		TotalArea = 0;
		for (double TriArea : TriAreas)
		{
			TotalArea += TriArea;
		}
	}
};


struct FNonUniformSamplingConfig
{
	FMeshSurfacePointSampling::ESizeDistribution SizeDistribution = FMeshSurfacePointSampling::ESizeDistribution::Uniform;
	double SizeDistributionPower = 2.0;

	TOptional<TFunctionRef<double(int TriangleID, FVector3d Position, FVector3d BaryCoords)>> WeightFunction;

	FMeshSurfacePointSampling::EInterpretWeightMode InterpretWeightMode = FMeshSurfacePointSampling::EInterpretWeightMode::WeightedRandom;
};


struct FDenseSamplePointSet
{
	TArray<FVector3d> DensePoints;
	TArray<int> Triangles;
	TArray<double> Weights;

	FAxisAlignedBox3d Bounds;
	// orientation array
	int MaxVertexID() const { return DensePoints.Num(); }
	bool IsVertex(int VertexID) const { return true; }
	FVector3d GetVertex(int Index) const { return DensePoints[Index]; }
	FAxisAlignedBox3d GetBounds() const { return Bounds; }
};


struct FPerTriangleDensePointSampling
{
	struct FTriangleSubArray
	{
		int32 StartIndex;
		int32 NumSamples;
	};
	TArray<FTriangleSubArray> TriSubArrays;

	template<typename MeshType>
	void InitializeForTriangleSet(
		const MeshType& SampleMesh, 
		const FTriangleInfoCache& TriInfo, 
		double DenseSampleArea, 
		int RandomSeed,
		FDenseSamplePointSet& PointSetOut)
	{
	}

	template<typename MeshType>
	void InitializeForTriangleSet(
		const MeshType& SampleMesh, 
		const FTriangleInfoCache& TriInfo, 
		double DenseSampleArea, 
		int RandomSeed,
		const FNonUniformSamplingConfig& NonUniformConfig,
		FDenseSamplePointSet& PointSetOut )
	{
		// figure out how many samples in each triangle, and assign each triangle a starting index into PointSetOut arrays
		TriSubArrays.SetNumUninitialized(SampleMesh.MaxTriangleID());
		int32 CurIndex = 0;
		for (int32 tid = 0; tid < SampleMesh.MaxTriangleID(); ++tid)
		{
			if ( SampleMesh.IsTriangle(tid) )
			{
				int NumSamples = FMath::Max( (int)(TriInfo.TriAreas[tid] / DenseSampleArea), 2 );  // a bit arbitrary...
				TriSubArrays[tid] = FTriangleSubArray{CurIndex, NumSamples};
				CurIndex += NumSamples;
			}
			else
			{
				TriSubArrays[tid] = FTriangleSubArray{-1, 0};
			}
		}
		PointSetOut.DensePoints.SetNumUninitialized(CurIndex);
		PointSetOut.Triangles.SetNumUninitialized(CurIndex);

		bool bComputeWeights = (NonUniformConfig.WeightFunction.IsSet());
		if (bComputeWeights)
		{
			PointSetOut.Weights.SetNumUninitialized(CurIndex);
		}

		// This parallel for seems to be quite expensive...maybe contention because all threads are writing to same DensePoints array?
		// Amount of work per TriangleID can vary quite a bit because it depends on triangle size...possibly should pass
		// unbalanced flag if triangle count is small relative to point count
		EParallelForFlags UseFlags = (SampleMesh.MaxTriangleID() < 100) ? EParallelForFlags::Unbalanced : EParallelForFlags::None;
		ParallelFor(TEXT("ComputeDenseSamples"), SampleMesh.MaxTriangleID(), 500, [&](int tid)
		{
			int NumSamples = TriSubArrays[tid].NumSamples;
			if ( NumSamples == 0 ) return;
			int StartIndex = TriSubArrays[tid].StartIndex;

			const FFrame3d& ProjectFrame = TriInfo.TriFrames[tid];
			const FTriangle2d& TriUV = TriInfo.UVTriangles[tid];

			// generate uniform random point in 2D quadrilateral  (http://mathworld.wolfram.com/TrianglePointPicking.html)
			FVector2d V1 = TriUV.V[1] - TriUV.V[0];
			FVector2d V2 = TriUV.V[2] - TriUV.V[0];

			int NumGenerated = 0;
			FRandomStream RandomStream(tid + RandomSeed);
			while (NumGenerated < NumSamples)
			{
				double a1 = RandomStream.GetFraction();
				double a2 = RandomStream.GetFraction();
				FVector2d PointUV = TriUV.V[0] + a1 * V1 + a2 * V2;
				if (TriUV.IsInside(PointUV))
				{
					FVector3d Position = ProjectFrame.FromPlaneUV(PointUV, 2);
					PointSetOut.DensePoints[StartIndex+NumGenerated] = Position;
					PointSetOut.Triangles[StartIndex+NumGenerated] = tid;

					if (bComputeWeights)
					{
						FVector3d BaryCoords = TriUV.GetBarycentricCoords(PointUV);
						PointSetOut.Weights[StartIndex+NumGenerated] = 
							NonUniformConfig.WeightFunction.GetValue()(tid, Position, BaryCoords);
					}

					NumGenerated++;
				}
			}
		}, UseFlags);
	}

};

template<typename MeshType>
void ConstructDenseUniformMeshPointSampling(
	const MeshType& SampleMesh,
	double SampleRadius,
	double SubSampleDensity,
	int RandomSeed,
	const FNonUniformSamplingConfig& NonUniformConfig,
	int MaxNumDenseSamples,
	FDenseSamplePointSet& DensePointSetOut )
{
	FTriangleInfoCache TriInfoCache;
	TriInfoCache.InitializeForTriangleSet<MeshType>(SampleMesh);

	// compute mesh bounds in background thread. ParallelFor could make this faster (eg FDynamicMesh3::GetBounds) but it's going to take longer than the other steps below anyway
	UE::Tasks::FTask BoundsTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&DensePointSetOut, &SampleMesh]()
	{
		DensePointSetOut.Bounds = FAxisAlignedBox3d::Empty();
		int MaxVertexID = SampleMesh.MaxVertexID();
		for (int32 k = 0; k < MaxVertexID; ++k)
		{
			if (SampleMesh.IsVertex(k))
			{
				DensePointSetOut.Bounds.Contain(SampleMesh.GetVertex(k));
			}
		}
	});
	
	double DiscArea = (FMathd::Pi * SampleRadius * SampleRadius);
	double ApproxNumUniformSamples = TriInfoCache.TotalArea / DiscArea;		// uniform disc area
	double EstNumDenseSamples = ApproxNumUniformSamples * SubSampleDensity * 2;		// 2 is fudge-factor
	if (MaxNumDenseSamples != 0 && EstNumDenseSamples > MaxNumDenseSamples)
	{
		EstNumDenseSamples = MaxNumDenseSamples;
	}
	double DenseSampleArea = TriInfoCache.TotalArea / EstNumDenseSamples;


	FPerTriangleDensePointSampling DensePerTriangleSampling;
	DensePerTriangleSampling.InitializeForTriangleSet<MeshType>(SampleMesh, TriInfoCache, DenseSampleArea, RandomSeed, 
		NonUniformConfig, DensePointSetOut);

	//UE_LOG(LogTemp, Warning, TEXT("TotalArea %f  SampleArea %f EstSamples %d  DenseSamples %d   DenseArea %f  NumDensePoints %d"),
	//	TriInfo.TotalArea, DiscArea, (int)ApproxNumUniformSamples, (int)EstNumDenseSamples, DenseSampleArea, NumDensePoints);

	// make sure we have bounds initialized
	BoundsTask.Wait();
}



template<typename MeshType>
void UniformMeshPointSampling(
	const MeshType& SampleMesh,
	TFunctionRef<void(FVector3d, int32, double)> EmitSampleFunc,
	double SampleRadius,
	int32 MaxSamples,
	double SubSampleDensity,
	int RandomSeed,
	int MaxNumDenseSamples,
	FProgressCancel* Progress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UniformMeshPointSampling);

	MaxSamples = (MaxSamples == 0) ? TNumericLimits<int>::Max() : MaxSamples;
	const bool bShuffle = (MaxSamples < TNumericLimits<int>::Max());

	//
	// Step 1: generate dense random point sampling of mesh surface. 
	// 
	FDenseSamplePointSet DensePointSet;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeDenseSamples);
		
		ConstructDenseUniformMeshPointSampling<MeshType>(SampleMesh, SampleRadius, SubSampleDensity, RandomSeed, 
			FNonUniformSamplingConfig(), MaxNumDenseSamples, DensePointSet);
	}
	int32 NumDensePoints = DensePointSet.MaxVertexID();

	if (Progress && Progress->Cancelled() ) return;

	//
	// Step 2: store dense point sampling in octree
	// 
	FSparseDynamicPointOctree3 Octree;
	Octree.ConfigureFromPointCountEstimate(DensePointSet.Bounds.MaxDim(), NumDensePoints);
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BuildDenseOctree);
		Octree.ParallelInsertDensePointSet(NumDensePoints, [&](int32 VertexID) { return DensePointSet.GetVertex(VertexID); } );
	}

	if (Progress && Progress->Cancelled() ) return;

	//
	// Step 3: dart throwing. Draw "random" dense point, add it to output set, and then
	// remove all other dense points within radius of this point. 
	// *NOTE* that in this implementation, we are not necessarily drawing random points.
	// We are drawing from a random sampling on the triangles, but the per-triangle points
	// are added to the PointSet in triangle index order. This introduces some bias
	// but makes the algorithm quite a bit faster...

	// currently only generating a semi-random point ordering (via shuffling) 
	// if a subset of points is requested.  This likely does create some bias, 
	TArray<int32> PointOrdering;
	PointOrdering.Reserve(NumDensePoints);
	if (bShuffle)
	{
		FModuloIteration Iter(NumDensePoints);
		uint32 NextIndex;
		while (Iter.GetNextIndex(NextIndex))
		{
			PointOrdering.Add(NextIndex);
		}
	}
	else
	{
		for (int32 k = 0; k < NumDensePoints; ++k)
		{
			PointOrdering.Add(k);
		}
	}
	int32 CurOrderingIndex = 0;

	double QueryRadiusSqr = 4 * SampleRadius * SampleRadius;
	TArray<bool> IsValidPoint;
	IsValidPoint.Init(true, DensePointSet.MaxVertexID());
	TArray<const FSparsePointOctreeCell*> QueryTempBuffer;
	TArray<int> PointsInBall;

	int NumEmittedSamples = 0;
	{ TRACE_CPUPROFILER_EVENT_SCOPE(ExtractPoints);

		while (CurOrderingIndex < NumDensePoints && NumEmittedSamples < MaxSamples)
		{
			if (NumEmittedSamples % 25 == 0 && Progress && Progress->Cancelled() ) return;

			// pick a vertex in the dense point set, ie "throw a dart that is guaranteed to be valid"
			int32 UseVertexID = IndexConstants::InvalidID;
			{ //TRACE_CPUPROFILER_EVENT_SCOPE(FindInitialPoint);

				while (UseVertexID == IndexConstants::InvalidID && CurOrderingIndex < NumDensePoints)
				{
					int VertexID = PointOrdering[CurOrderingIndex++];
					if (IsValidPoint[VertexID])
					{
						UseVertexID = VertexID;
						break;
					}
				}
			}
			if (UseVertexID == IndexConstants::InvalidID)
			{
				continue;
			}

			// found a valid point
			FVector3d SamplePoint = DensePointSet.DensePoints[UseVertexID];
			EmitSampleFunc(SamplePoint, DensePointSet.Triangles[UseVertexID], SampleRadius);
			NumEmittedSamples++;
			Octree.RemovePoint(UseVertexID);
			IsValidPoint[UseVertexID] = false;

			// remove dense points within sample radius from this point
			PointsInBall.Reset();
			FAxisAlignedBox3d QueryBox(SamplePoint, 2*SampleRadius);
			Octree.RangeQuery(QueryBox,		// add SphereQuery to Octree? would save a chunk...
				[&](int32 PointID) {
					return IsValidPoint[PointID] && DistanceSquared(DensePointSet.GetVertex(PointID), SamplePoint) < QueryRadiusSqr;
				},
				PointsInBall, &QueryTempBuffer);

			for (int32 QueryPointID : PointsInBall)
			{
				Octree.RemovePointUnsafe(QueryPointID);
				IsValidPoint[QueryPointID] = false;
			}

		}
	}
}







template<typename MeshType>
void NonUniformMeshPointSampling(
	const MeshType& SampleMesh,
	TFunctionRef<void(FVector3d, int32, double)> EmitSampleFunc,
	double MinSampleRadius,
	double MaxSampleRadius,
	int32 MaxSamples,
	double SubSampleDensity,
	int RandomSeed,
	const FNonUniformSamplingConfig& NonUniformConfig,
	int MaxNumDenseSamples,
	FProgressCancel* Progress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UniformMeshPointSampling);

	MaxSamples = (MaxSamples == 0) ? TNumericLimits<int>::Max() : MaxSamples;

	//
	// Step 1: generate dense random point sampling of mesh surface. 
	// 
	FDenseSamplePointSet DensePointSet;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeDenseSamples);
		ConstructDenseUniformMeshPointSampling<MeshType>(SampleMesh, MinSampleRadius, SubSampleDensity, RandomSeed, 
			NonUniformConfig, MaxNumDenseSamples, DensePointSet);
	}
	int32 NumDensePoints = DensePointSet.MaxVertexID();
	bool bHaveWeights = DensePointSet.Weights.Num() > 0;

	if (Progress && Progress->Cancelled() ) return;

	//
	// Step 2: store dense point sampling in octree
	// 
	FSparseDynamicPointOctree3 Octree;
	Octree.ConfigureFromPointCountEstimate(DensePointSet.Bounds.MaxDim(), NumDensePoints);
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BuildDenseOctree);
		Octree.ParallelInsertDensePointSet(NumDensePoints, [&](int32 VertexID) { return DensePointSet.GetVertex(VertexID); } );
	}

	if (Progress && Progress->Cancelled() ) return;

	//
	// Step 3: dart throwing. Draw "random" dense point, add it to output set, and then
	// remove all other dense points within radius of this point. 
	// *NOTE* that in this implementation, we are not necessarily drawing random points.
	// We are drawing from a random sampling on the triangles, but the per-triangle points
	// are added to the PointSet in triangle index order. This introduces some bias
	// but makes the algorithm quite a bit faster...

	// currently only generating a semi-random point ordering (via shuffling) 
	// if a subset of points is requested.  This likely does create some bias, 
	TArray<int32> PointOrdering;
	PointOrdering.Reserve(NumDensePoints);
	FModuloIteration Iter(NumDensePoints);
	{
		uint32 NextIndex;
		while (Iter.GetNextIndex(NextIndex))
		{
			PointOrdering.Add(NextIndex);
		}
	}
	int32 CurOrderingIndex = 0;

	TArray<bool> IsValidPoint;
	IsValidPoint.Init(true, DensePointSet.MaxVertexID());
	TArray<const FSparsePointOctreeCell*> QueryTempBuffer;
	TArray<int> PointsInBall;

	FRandomStream RadiusStream(RandomSeed);

	// likely could benefit from hash grid here? 
	TArray<FVector3d> EmittedSamples;
	TArray<double> EmittedRadius;
	auto FindOverlappingSample = [&EmittedSamples, &EmittedRadius, &PointsInBall, &QueryTempBuffer, MaxSampleRadius](FVector3d Position, double Radius) -> double
	{
		for (int32 k = 0; k < EmittedSamples.Num(); ++k)
		{
			double NeighbourDist = Distance(EmittedSamples[k], Position);
			//if ( NeighbourDist < (Radius + EmittedRadius[k]) )
			if ( Radius > (NeighbourDist - EmittedRadius[k]) )
			{
				return (NeighbourDist - EmittedRadius[k]);
			}
		}
		return TNumericLimits<double>::Max();
	};

	// In weighted sampling, we cannot guarantee that a dense sample point w/ a radius > MinSampleRadius will
	// actually fit without collision. The "correct" way to handle this, by randomly choosing new points until
	// a valid one is found, can take a very long time. So instead we can "decay" the radius down to 
	// MinSampleRadius in multiple steps, trying to find a radius that fits. We are guaranteed that any active
	// dense point will fit with MinSampleRadius, so this significantly accellerates the sampling, at the cost
	// of introducing some bias.
	TArray<double, TInlineAllocator<10>> DecaySteps;
	bool bIsFixedRadiusMethod = false;
	if (NonUniformConfig.InterpretWeightMode == FMeshSurfacePointSampling::EInterpretWeightMode::RadiusInterp)
	{
		DecaySteps = TArray<double, TInlineAllocator<10>>({1.0});
		bIsFixedRadiusMethod = true;
	}
	else
	{
		DecaySteps = TArray<double, TInlineAllocator<10>>({1.0, 0.8, 0.6, 0.4, 0.2, 0.0});
	}

	TArray<double> CurDistances;
	CurDistances.Init(TNumericLimits<double>::Max(), NumDensePoints);

	int NumEmittedSamples = 0;
	{ TRACE_CPUPROFILER_EVENT_SCOPE(ExtractPoints);

		int NumFailures = 0;
		while (PointOrdering.Num() > 0 && NumEmittedSamples < MaxSamples && NumFailures < 1000)
		{
			if (NumEmittedSamples % 25 == 0 && Progress && Progress->Cancelled() ) return;

			int32 UseVertexID = IndexConstants::InvalidID;
			int32 PointOrderingIndex = -1;
			double SampleRadius = MinSampleRadius;

			// try to find a valid (point, radius) pair. This may fail if we cannot find a 
			// valid radius...
			int32 NumRemaining = PointOrdering.Num();
			for ( int32 k = 0; k < NumRemaining; ++k)
			{
				int VertexID = PointOrdering[k];

				if (IsValidPoint[VertexID] == false)		// if point is expired, discard it
				{
					PointOrdering.RemoveAtSwap(k, 1, EAllowShrinking::No);
					NumRemaining--;
					k--;		// reconsider point we just swapped to index k
					continue;
				}

				FVector3d Position = DensePointSet.GetVertex(VertexID);

				// based on the weight/random strategy, generate a parameter in range [0,1] that will
				// be used to interpolate the Min/Max Radius below
				double InterpRadiusT = 0;
				if (bHaveWeights)
				{
					if (NonUniformConfig.InterpretWeightMode == FMeshSurfacePointSampling::EInterpretWeightMode::WeightedRandom)
					{
						double Weight = FMathd::Clamp(DensePointSet.Weights[VertexID], 0, 1);
						double Random = RadiusStream.GetFraction();
						InterpRadiusT = (Weight + Random) / 2.0;		// can parameterize this as ( (N-1)*Weight + Random ) / N
					}
					else  // RadiusInterp
					{
						InterpRadiusT = DensePointSet.Weights[VertexID];
					}
				}
				else 
				{
					InterpRadiusT = RadiusStream.GetFraction();
				}
				if (NonUniformConfig.SizeDistribution == FMeshSurfacePointSampling::ESizeDistribution::Smaller)
				{
					InterpRadiusT = FMathd::Pow(InterpRadiusT, NonUniformConfig.SizeDistributionPower);
				}
				else if (NonUniformConfig.SizeDistribution == FMeshSurfacePointSampling::ESizeDistribution::Larger)
				{
					InterpRadiusT = FMathd::Pow(InterpRadiusT, 1.0 / NonUniformConfig.SizeDistributionPower);
				}

				// try to fit a sample at the selected point, possibly incrementally shrinking the 
				// sample radius down to MinRadius to guarantee a fit
				bool bFound = false;
				double MinNeighbourGap = TNumericLimits<double>::Max();
				for (double DecayStep : DecaySteps)
				{
					double UseRadius = FMathd::Lerp(MinSampleRadius, MaxSampleRadius, InterpRadiusT * DecayStep);
					if (UseRadius > CurDistances[VertexID])
					{
						continue;
					}

					double NeighbourGap = FindOverlappingSample(Position, UseRadius);
					//if ( NeighbourGap == TNumericLimts<double>::Max() )
					if ( UseRadius < NeighbourGap )
					{
						UseVertexID = VertexID;
						SampleRadius = UseRadius;
						PointOrderingIndex = k;
						bFound = true;
						break;
					}
					else
					{
						MinNeighbourGap = FMathd::Min(MinNeighbourGap, NeighbourGap);
					}
				}
				if (bFound)
				{
					break;
				}
				CurDistances[VertexID] = FMathd::Min(CurDistances[VertexID], MinNeighbourGap);

				// if this is a method w/ no random variation or decay, this (point,radius) pair will never fit and can be removed
				if (bIsFixedRadiusMethod)
				{
					PointOrdering.RemoveAtSwap(k, 1, EAllowShrinking::No);
					NumRemaining--;
					k--;		// reconsider point we just swapped to index k
				}
			}
			if (UseVertexID == IndexConstants::InvalidID)
			{
				NumFailures++;
				continue;
			}

			// remove selected point from ordering
			PointOrdering.RemoveAtSwap(PointOrderingIndex, 1, EAllowShrinking::No);

			// emit our valid (point, triangle, radius) sample
			FVector3d SamplePoint = DensePointSet.DensePoints[UseVertexID];
			EmitSampleFunc(SamplePoint, DensePointSet.Triangles[UseVertexID], SampleRadius);
			NumEmittedSamples++;
			Octree.RemovePoint(UseVertexID);
			IsValidPoint[UseVertexID] = false;

			// add point to known samples list
			int32 SampleID = EmittedSamples.Num();
			EmittedSamples.Add(SamplePoint);
			EmittedRadius.Add(SampleRadius);

			// once we add this point, no point can be within it's radius, and any other point closer than
			// MinSampleRadius would collide, so we can decimate all points within the radius sum
			double CombinedRadiusSqr = ((SampleRadius + MinSampleRadius) * (SampleRadius + MinSampleRadius));

			// find all dense points within our query radius
			PointsInBall.Reset();
			FAxisAlignedBox3d QueryBox(SamplePoint, 2*SampleRadius);
			Octree.RangeQuery(QueryBox,		// add SphereQuery to Octree? would save a chunk...
				[&](int32 PointID) {
					return IsValidPoint[PointID] && DistanceSquared(DensePointSet.GetVertex(PointID), SamplePoint) < CombinedRadiusSqr;
				},
				PointsInBall, &QueryTempBuffer);

			// remove all those dense points from the octree and mark them invalid
			for (int32 QueryPointID : PointsInBall)
			{
				Octree.RemovePointUnsafe(QueryPointID);
				IsValidPoint[QueryPointID] = false;
			}
		}
	}
}








} // end namespace UELocal


void FMeshSurfacePointSampling::ComputePoissonSampling(const FDynamicMesh3& Mesh, FProgressCancel* Progress)
{
	Result = FGeometryResult(EGeometryResultType::InProgress);

	Samples.Reset();
	Radii.Reset();
	TriangleIDs.Reset();
	BarycentricCoords.Reset();
	auto AddSampleFunc = [&](FVector3d Position, int TriangleID, double Radius)
	{
		Samples.Add(FFrame3d(Position, Mesh.GetTriNormal(TriangleID)));
		Radii.Add(Radius);
		TriangleIDs.Add(TriangleID);
	};

	if (MaxSampleRadius > SampleRadius)
	{
		UELocal::FNonUniformSamplingConfig NonUniformConfig;
		NonUniformConfig.InterpretWeightMode = this->InterpretWeightMode;
		NonUniformConfig.SizeDistribution = this->SizeDistribution;
		NonUniformConfig.SizeDistributionPower = FMath::Clamp(this->SizeDistributionPower, 1.0, 10.0);

		if (bUseVertexWeights && VertexWeights.Num() == Mesh.MaxVertexID())
		{
			auto ComputeVertexWeightFunc = [&](int TriangleID, FVector3d Position, FVector3d BaryCoords)
				{
					FIndex3i Tri = Mesh.GetTriangle(TriangleID);
					double Weight = BaryCoords.X*VertexWeights[Tri.A] + BaryCoords.Y*VertexWeights[Tri.B] + BaryCoords.Z*VertexWeights[Tri.C];
					if (bInvertWeights)
					{
						Weight = 1.0 - FMathd::Clamp(Weight, 0.0, 1.0);
					}
					return Weight;
				};
			NonUniformConfig.WeightFunction = ComputeVertexWeightFunc;

			UELocal::NonUniformMeshPointSampling<FDynamicMesh3>(Mesh, AddSampleFunc,
				SampleRadius, MaxSampleRadius, MaxSamples, SubSampleDensity, RandomSeed, NonUniformConfig, MaxSubSamplePoints, Progress);
		}
		else
		{
			UELocal::NonUniformMeshPointSampling<FDynamicMesh3>(Mesh, AddSampleFunc,
				SampleRadius, MaxSampleRadius, MaxSamples, SubSampleDensity, RandomSeed, NonUniformConfig, MaxSubSamplePoints, Progress );
		}
	}
	else
	{
		UELocal::UniformMeshPointSampling<FDynamicMesh3>(Mesh, AddSampleFunc,
			SampleRadius, MaxSamples, SubSampleDensity, RandomSeed, MaxSubSamplePoints, Progress );
	}

	if (bComputeBarycentrics)
	{
		FVector3d A,B,C;
		int32 N = Samples.Num();
		BarycentricCoords.SetNum(N);
		for (int32 k = 0; k < N; ++k)
		{
			Mesh.GetTriVertices(TriangleIDs[k], A, B, C);
			BarycentricCoords[k] = VectorUtil::BarycentricCoords( Samples[k].Origin, A, B, C);
		}
	}

	Result.SetSuccess(true, Progress);
}