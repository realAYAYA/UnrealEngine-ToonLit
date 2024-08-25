// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/RemeshNode.h"

#include "ChaosClothAsset/ClothPatternToDynamicMesh.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothCollectionAttribute.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/SimulationSelfCollisionSpheresConfigNode.h"
#include "ChaosClothAsset/SimulationLongRangeAttachmentConfigNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "DynamicMesh/MeshTangents.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "CleaningOps/RemeshMeshOp.h"
#include "CleaningOps/SimplifyMeshOp.h"
#include "MeshUVChannelInfo.h"
#include "MeshBoundaryLoops.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Algo/Find.h"
#include "IMeshReductionManagerModule.h"
#include "Algo/RemoveIf.h"
#include "Spatial/PointSetHashTable.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RemeshNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetRemeshNode"

namespace UE::Chaos::ClothAsset::Private
{

	//
	// Functions to support seam remeshing
	//

	struct FSeamCollapseParameters
	{
		bool bCanCollapse = false;
		int32 KeepStitchIndex = -1;
		int32 DeleteStitchIndex = -1;
		double NewPositionEdgeParameter = 0.5;
	};

	/**
	 * A "Seam Edge" is a pair of consecutive stitches in a seam. Corresponds to two "mesh" edges.
	 */
	struct FSeamEdge
	{
		FSeamEdge(FIntVector2 A, FIntVector2 B)
		{
			Stitches[0] = A;
			Stitches[1] = B;
		}

		TStaticArray<FIntVector2, 2> Stitches;

		bool operator==(const FSeamEdge& Other) const
		{
			return (Other.Stitches == Stitches);
		}
	};

	// Try to identify if the corner A -- B -- C is "sharp".
	// 
	// The function computes the dot product of normalized vectors AB and BC, and returns true if the result is less than the given threshold
	// Also returns true if A \approx B or B \approx C
	bool IsSharpCorner(const FVector3d& A, const FVector3d& B, const FVector3d& C, double CosAngleThreshold)
	{
		const FVector3d AB = B - A;
		const FVector3d BC = C - B;
		const double NormAB = AB.Length();
		const double NormBC = BC.Length();

		if (FMath::Abs(NormAB) < UE_SMALL_NUMBER || FMath::Abs(NormBC) < UE_SMALL_NUMBER)
		{
			return true;
		}

		const double CosAngle = AB.Dot(BC) / (NormAB * NormBC);
		return (CosAngle < CosAngleThreshold);
	}

	FSeamCollapseParameters GetSeamEdgeCollapseParameters(int32 SeamID, int32 StitchID, const UE::Geometry::FDynamicMesh3& Mesh, const TArray<TArray<FIntVector2>>& Seams, double CosAngleCornerTheshold = 0.5)
	{
		using namespace UE::Geometry;

		const FSeamEdge SeamEdge(Seams[SeamID][StitchID], Seams[SeamID][StitchID + 1]);

		// Check if the pair of mesh edges refer to the same edge
		const int EdgeA = Mesh.FindEdge(SeamEdge.Stitches[0][0], SeamEdge.Stitches[1][0]);
		const int EdgeB = Mesh.FindEdge(SeamEdge.Stitches[0][1], SeamEdge.Stitches[1][1]);

		if (SeamEdge.Stitches[0][0] == SeamEdge.Stitches[0][1])
		{
			FSeamCollapseParameters Constraints;
			Constraints.bCanCollapse = false;
			return Constraints;
		}

		if (SeamEdge.Stitches[1][0] == SeamEdge.Stitches[1][1])
		{
			FSeamCollapseParameters Constraints;
			Constraints.bCanCollapse = false;
			return Constraints;
		}

		if (EdgeA == EdgeB)
		{
			// Don't collapse the same edge twice
			FSeamCollapseParameters Constraints;
			Constraints.bCanCollapse = false;
			return Constraints;
		}

		// Check if any vertex exists in another stitch somewhere. For now we will skip these operations
		// TODO: If only one vertex is involved in another stitch somewhere else, we could constrain that vertex to be kept instead
		TStaticArray<int32, 4> SeamEdgeVertices;
		SeamEdgeVertices[0] = SeamEdge.Stitches[0][0];
		SeamEdgeVertices[1] = SeamEdge.Stitches[0][1];
		SeamEdgeVertices[2] = SeamEdge.Stitches[1][0];
		SeamEdgeVertices[3] = SeamEdge.Stitches[1][1];

		for (int32 InnerSeamID = 0; InnerSeamID < Seams.Num(); ++InnerSeamID)
		{
			const TArray<FIntVector2>& InnerSeam = Seams[InnerSeamID];
			for (int32 InnerStitchID = 0; InnerStitchID < InnerSeam.Num(); ++InnerStitchID)
			{
				if (InnerSeamID == SeamID)
				{
					if (InnerStitchID == StitchID || InnerStitchID == StitchID + 1)
					{
						// Don't check against adjacent stitches
						continue;
					}
				}

				const FIntVector2& InnerStitch = InnerSeam[InnerStitchID];

				for (const int32& SeamVertex : SeamEdgeVertices) //-V1078
				{
					if (SeamVertex == InnerStitch[0] || SeamVertex == InnerStitch[1])
					{
						FSeamCollapseParameters Constraints;
						Constraints.bCanCollapse = false;
						return Constraints;
					}
				}
			}
		}

		// Now check for conditions that might prevent one or the other vertex from being deleted

		bool bStitchIsConstrained = false;
		bool bNextStitchIsConstrained = false;

		// check if stitch is at the beginning or end of the seam
		if (StitchID == 0)
		{
			bStitchIsConstrained = true;
		}
		if (StitchID + 1 == Seams[SeamID].Num() - 1)
		{
			bNextStitchIsConstrained = true;
		}

		// check for vertices that connect two seam sides -- these were previously added by creating a stitch with the same vertex twice
		if (!bStitchIsConstrained && (SeamEdge.Stitches[0][0] == SeamEdge.Stitches[0][1]))
		{
			bStitchIsConstrained = true;
		}
		if (!bNextStitchIsConstrained &&(SeamEdge.Stitches[1][0] == SeamEdge.Stitches[1][1]))
		{
			bNextStitchIsConstrained = true;
		}

		// check if any vertex is at a sharp corner

		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (StitchID > 0)
			{
				const FSeamEdge PrevSeamEdge(Seams[SeamID][StitchID - 1], Seams[SeamID][StitchID]);

				check(PrevSeamEdge.Stitches[1][Side] == SeamEdge.Stitches[0][Side]);
				check(PrevSeamEdge.Stitches[1][Side] == SeamEdge.Stitches[0][Side])

				const FVector3d A = Mesh.GetVertex(PrevSeamEdge.Stitches[0][Side]);
				const FVector3d B = Mesh.GetVertex(PrevSeamEdge.Stitches[1][Side]);
				const FVector3d C = Mesh.GetVertex(SeamEdge.Stitches[1][Side]);

				bStitchIsConstrained = IsSharpCorner(A, B, C, CosAngleCornerTheshold);
			}

			if (StitchID < Seams[SeamID].Num() - 2)
			{
				const FSeamEdge NextSeamEdge(Seams[SeamID][StitchID + 1], Seams[SeamID][StitchID + 2]);

				check(SeamEdge.Stitches[1][Side] == NextSeamEdge.Stitches[0][Side]);
				check(NextSeamEdge.Stitches[0][Side] == SeamEdge.Stitches[1][Side])

				const FVector3d A = Mesh.GetVertex(SeamEdge.Stitches[0][Side]);
				const FVector3d B = Mesh.GetVertex(SeamEdge.Stitches[1][Side]);
				const FVector3d C = Mesh.GetVertex(NextSeamEdge.Stitches[1][Side]);

				bNextStitchIsConstrained = IsSharpCorner(A, B, C, CosAngleCornerTheshold);
			}
		}

		if (bStitchIsConstrained && bNextStitchIsConstrained)
		{
			FSeamCollapseParameters Constraints;
			Constraints.bCanCollapse = false;
			return Constraints;
		}

		FSeamCollapseParameters Constraints;
		Constraints.bCanCollapse = true;

		if (bStitchIsConstrained && !bNextStitchIsConstrained)
		{
			Constraints.KeepStitchIndex = StitchID;
			Constraints.DeleteStitchIndex = StitchID+1;
			Constraints.NewPositionEdgeParameter = 0.0;		// "Keep" vertex should stay where it is
		}
		else if (!bStitchIsConstrained && bNextStitchIsConstrained)
		{
			Constraints.KeepStitchIndex = StitchID+1;
			Constraints.DeleteStitchIndex = StitchID;
			Constraints.NewPositionEdgeParameter = 0.0;	    // "Keep" vertex should stay where it is
		}
		else
		{
			// unconstrained
			Constraints.KeepStitchIndex = StitchID;
			Constraints.DeleteStitchIndex = StitchID + 1;
			Constraints.NewPositionEdgeParameter = 0.5;		// Collapse to the edge midpoint
		}


		// Check if either collapse would fail in FDynamicMesh3::CollapseEdge
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const int KeepVertexIndex = Seams[SeamID][Constraints.KeepStitchIndex][Side];
			const int DeleteVertexIndex = Seams[SeamID][Constraints.DeleteStitchIndex][Side];

			const EMeshResult CanCollapsePreview = Mesh.CanCollapseEdge(KeepVertexIndex, DeleteVertexIndex, Constraints.NewPositionEdgeParameter);
			if (CanCollapsePreview != EMeshResult::Ok)
			{
				Constraints.bCanCollapse = false;
				break;
			}
		}

		return Constraints;
	}

	bool CanSplitSeamEdge(int32 SeamID, int32 StitchID, const TArray<TArray<FIntVector2>>& Seams)
	{
		using namespace UE::Geometry;

		bool bCanSplit = true;

		const FSeamEdge SeamEdge(Seams[SeamID][StitchID], Seams[SeamID][StitchID + 1]);

		// Check if any vertex exists in another stitch somewhere. For now we will skip these operations
		// TODO: We could probably enable splits if we are very careful about handling mesh edges that are in more than one seam
		TStaticArray<int32, 4> SeamEdgeVertices;
		SeamEdgeVertices[0] = SeamEdge.Stitches[0][0];
		SeamEdgeVertices[1] = SeamEdge.Stitches[0][1];
		SeamEdgeVertices[2] = SeamEdge.Stitches[1][0];
		SeamEdgeVertices[3] = SeamEdge.Stitches[1][1];

		for (int32 InnerSeamID = 0; InnerSeamID < Seams.Num() && bCanSplit; ++InnerSeamID)
		{
			const TArray<FIntVector2>& InnerSeam = Seams[InnerSeamID];
			for (int32 InnerStitchID = 0; InnerStitchID < InnerSeam.Num() && bCanSplit; ++InnerStitchID)
			{
				if (InnerSeamID == SeamID)
				{
					if (InnerStitchID == StitchID || InnerStitchID == StitchID + 1)
					{
						// Don't check against adjacent stitches
						continue;
					}
				}

				const FIntVector2& InnerStitch = InnerSeam[InnerStitchID];

				for (const int32& SeamVertex : SeamEdgeVertices) //-V1078
				{
					if (SeamVertex == InnerStitch[0] || SeamVertex == InnerStitch[1])
					{
						bCanSplit = false;
						break;
					}
				}
			}
		}


		return bCanSplit;
	}


	void FindCoincidentBoundaryVertices(const UE::Geometry::FDynamicMesh3& Mesh, TArray<FIntVector2>& Pairs)
	{
		using namespace UE::Geometry;

		constexpr double ProximityTolerance = FMathf::ZeroTolerance;

		TSet<int32> BoundaryVertices;
		for (int32 EdgeID : Mesh.BoundaryEdgeIndicesItr())
		{
			const UE::Geometry::FIndex2i Vertices = Mesh.GetEdgeV(EdgeID);
			BoundaryVertices.Add(Vertices[0]);
			BoundaryVertices.Add(Vertices[1]);
		}

		//
		// Create a spatial hash to speed up matching vertex search (this setup code was mostly copied from FMergeCoincidentMeshEdges)
		//
		
		// use denser grid as vertex count increases
		const int HashN = (Mesh.TriangleCount() < 100000) ? 64 : 128;
		const UE::Geometry::FAxisAlignedBox3d Bounds = Mesh.GetBounds(true);
		const double CellSize = FMath::Max(FMathd::ZeroTolerance, Bounds.MaxDim() / (double)HashN);

		UE::Geometry::FPointSetAdapterd BoundaryVertAdapter;
		BoundaryVertAdapter.MaxPointID = [&Mesh]() { return Mesh.MaxVertexID(); };
		BoundaryVertAdapter.PointCount = [&BoundaryVertices]() { return BoundaryVertices.Num(); };
		BoundaryVertAdapter.IsPoint = [&Mesh](int Idx) { return Mesh.IsVertex(Idx) && Mesh.IsBoundaryVertex(Idx); };
		BoundaryVertAdapter.GetPoint = [&Mesh](int Idx) { return Mesh.GetVertex(Idx); };
		BoundaryVertAdapter.HasNormals = [] { return false; };
		BoundaryVertAdapter.GetPointNormal = [](int Idx) { return FVector3f::UnitY(); };

		FPointSetHashtable BoundaryVertsHash(&BoundaryVertAdapter);
		BoundaryVertsHash.Build(CellSize, Bounds.Min);
		const double UseMergeSearchTol = FMathd::Min(CellSize, 2 * ProximityTolerance);

		// Now look for coincident vertices
		
		TSet<FIntVector2> PairSet;

		for (const int32 VertexAIndex : BoundaryVertices)
		{
			const FVector3d VertexAPosition = Mesh.GetVertex(VertexAIndex);

			TArray<int> NearbyVertices;
			BoundaryVertsHash.FindPointsInBall(VertexAPosition, UseMergeSearchTol, NearbyVertices);

			for (const int32 VertexBIndex : NearbyVertices)
			{
				if (VertexAIndex == VertexBIndex)
				{
					continue;
				}

				if (Mesh.FindEdge(VertexAIndex, VertexBIndex) != UE::Geometry::FDynamicMesh3::InvalidID)
				{
					continue;
				}

				const FVector3d VertexBPosition = Mesh.GetVertex(VertexBIndex);
				const double DistSqr = FVector3d::DistSquared(VertexAPosition, VertexBPosition);

				if (DistSqr < ProximityTolerance * ProximityTolerance)
				{
					const FIntVector2 SortedPair = VertexAIndex < VertexBIndex ? FIntVector2{ VertexAIndex, VertexBIndex } : FIntVector2{ VertexBIndex, VertexAIndex };
					PairSet.Add(SortedPair);
				}
			}
		}

		Pairs = PairSet.Array();
	}


	void RemeshSeams(UE::Geometry::FDynamicMesh3& Mesh, TArray<TArray<FIntVector2>>& Seams, double TargetEdgeLength)
	{
		using namespace UE::Geometry;

		// constants pulled from FRemesher::SetTargetEdgeLength
		const double MinLength = 0.66 * TargetEdgeLength;
		const double MaxLength = 1.33 * TargetEdgeLength;

		for (int32 SeamID = 0; SeamID < Seams.Num(); ++SeamID)
		{
			TArray<FIntVector2>& Seam = Seams[SeamID];

			for (int32 StitchID = 0; StitchID < Seam.Num() - 1; ++StitchID)
			{
				const int32 SideAVertexA = Seam[StitchID][0];
				const int32 SideAVertexB = Seam[StitchID + 1][0];

				if (Mesh.FindEdge(SideAVertexA, SideAVertexB) == UE::Geometry::FDynamicMesh3::InvalidID)
				{
					continue;
				}

				const double EdgeALength = (Mesh.GetVertex(SideAVertexA) - Mesh.GetVertex(SideAVertexB)).Length();

				const int32 SideBVertexA = Seam[StitchID][1];
				const int32 SideBVertexB = Seam[StitchID + 1][1];

				if (Mesh.FindEdge(SideBVertexA, SideBVertexB) == UE::Geometry::FDynamicMesh3::InvalidID)
				{
					continue;
				}

				const double EdgeBLength = (Mesh.GetVertex(SideBVertexA) - Mesh.GetVertex(SideBVertexB)).Length();

				if ((EdgeALength < MinLength) && (EdgeBLength < MinLength))
				{
					//
					// Collapse
					//

					const FSeamCollapseParameters CollapseConstraints = GetSeamEdgeCollapseParameters(SeamID, StitchID, Mesh, Seams);
					
					if (!CollapseConstraints.bCanCollapse)
					{
						continue;
					}

					const int32 PatternAKeepVertex = Seam[CollapseConstraints.KeepStitchIndex][0];
					const int32 PatternADeleteVertex = Seam[CollapseConstraints.DeleteStitchIndex][0];
					const int32 PatternBKeepVertex = Seam[CollapseConstraints.KeepStitchIndex][1];
					const int32 PatternBDeleteVertex = Seam[CollapseConstraints.DeleteStitchIndex][1];

					FDynamicMesh3::FEdgeCollapseInfo CollapseInfoA;
					const EMeshResult ResultA = Mesh.CollapseEdge(PatternAKeepVertex, PatternADeleteVertex, CollapseConstraints.NewPositionEdgeParameter, CollapseInfoA);

					FDynamicMesh3::FEdgeCollapseInfo CollapseInfoB;
					const EMeshResult ResultB = Mesh.CollapseEdge(PatternBKeepVertex, PatternBDeleteVertex, CollapseConstraints.NewPositionEdgeParameter, CollapseInfoB);

					check(ResultA == EMeshResult::Ok && ResultB == EMeshResult::Ok)

					Seam.RemoveAt(CollapseConstraints.DeleteStitchIndex);
				}
				else if ((EdgeALength > MaxLength) && (EdgeBLength > MaxLength))
				{
					//
					// Split
					//


					const bool bCanSplit = CanSplitSeamEdge(SeamID, StitchID, Seams);

					if (!bCanSplit)
					{
						continue;
					}

					FDynamicMesh3::FEdgeSplitInfo SplitInfoA;
					const EMeshResult ResultA = Mesh.SplitEdge(SideAVertexA, SideAVertexB, SplitInfoA);

					if (ResultA == EMeshResult::Ok)
					{
						if (Mesh.FindEdge(SideBVertexA, SideBVertexB) == UE::Geometry::FDynamicMesh3::InvalidID)
						{
							// Don't split the same edge twice
							continue;
						}

						FDynamicMesh3::FEdgeSplitInfo SplitInfoB;
						const EMeshResult ResultB = Mesh.SplitEdge(SideBVertexA, SideBVertexB, SplitInfoB);
						check(ResultB == EMeshResult::Ok);

						const FIntVector2 NewStitch{ SplitInfoA.NewVertex, SplitInfoB.NewVertex };

						Seam.Insert(NewStitch, StitchID+1);
					}
				}
			}
		}
	}


	//
	// Boundary remeshing
	//

	struct FEdgeCollapseParameters
	{
		bool bCanCollapse = false;
		int32 KeepVertexIndex = -1;
		int32 DeleteVertexIndex = -1;
		double NewPositionEdgeParameter = 0.5;
	};

	FEdgeCollapseParameters GetBoundaryEdgeCollapseParameters(const UE::Geometry::FDynamicMesh3& Mesh, const TArray<TArray<FIntVector2>>& Seams, const UE::Geometry::FIndex2i& EdgeVerts, double CosAngleThreshold = 0.5)
	{
		using namespace UE::Geometry;

		auto IsSeamVertex = [](const TArray<TArray<FIntVector2>>& Seams, int VertexIndex) -> bool
		{
			for (const TArray<FIntVector2>& Seam : Seams)
			{
				for (int32 StitchID = 0; StitchID < Seam.Num(); ++StitchID)
				{
					const FIntVector2& Stitch = Seam[StitchID];
					if ((int)Stitch[0] == VertexIndex || (int)Stitch[1] == VertexIndex)
					{
						return true;
					}
				}
			}

			return false;
		};

		auto FindAdjacentBoundaryVertex = [](const UE::Geometry::FDynamicMesh3& Mesh, int VertexID, int ExcludedVertexID) -> int
		{
			for (int EdgeID : Mesh.VtxEdgesItr(VertexID))
			{
				if (!Mesh.IsBoundaryEdge(EdgeID))
				{
					continue;
				}

				const UE::Geometry::FIndex2i Edge = Mesh.GetEdge(EdgeID).Vert;
				const int TestVertexID = Edge[0] == VertexID ? Edge[1] : Edge[0];

				if (TestVertexID != ExcludedVertexID)
				{
					return TestVertexID;
				}
			}

			return UE::Geometry::FDynamicMesh3::InvalidID;
		};


		const int VertexA = EdgeVerts[0];
		const int VertexB = EdgeVerts[1];

		// Check for conditions that might prevent one or the other vertex from being deleted
		bool bVertexIsConstrained = false;
		bool bNextVertexIsConstrained = false;

		// Check if either vertex is on a seam

		if (IsSeamVertex(Seams, VertexA))
		{
			bVertexIsConstrained = true;
		}
	
		if (IsSeamVertex(Seams, VertexB))
		{
			bNextVertexIsConstrained = true;
		}

		// check if either vertex is at a sharp boundary corner

		int OtherVertex = FindAdjacentBoundaryVertex(Mesh, VertexA, VertexB);
		if ((OtherVertex != FDynamicMesh3::InvalidID) && !bVertexIsConstrained)
		{
			const FVector3d A = Mesh.GetVertex(OtherVertex);
			const FVector3d B = Mesh.GetVertex(VertexA);
			const FVector3d C = Mesh.GetVertex(VertexB);

			bVertexIsConstrained = IsSharpCorner(A, B, C, CosAngleThreshold);
		}

		OtherVertex = FindAdjacentBoundaryVertex(Mesh, VertexB, VertexA);
		if ((OtherVertex != FDynamicMesh3::InvalidID) && !bNextVertexIsConstrained)
		{
			const FVector3d A = Mesh.GetVertex(VertexA);
			const FVector3d B = Mesh.GetVertex(VertexB);
			const FVector3d C = Mesh.GetVertex(OtherVertex);

			bNextVertexIsConstrained = IsSharpCorner(A, B, C, CosAngleThreshold);
		}

		if (bVertexIsConstrained && bNextVertexIsConstrained)
		{
			FEdgeCollapseParameters Constraints;
			Constraints.bCanCollapse = false;
			return Constraints;
		}

		FEdgeCollapseParameters Constraints;
		Constraints.bCanCollapse = true;

		if (bVertexIsConstrained && !bNextVertexIsConstrained)
		{
			Constraints.KeepVertexIndex = VertexA;
			Constraints.DeleteVertexIndex = VertexB;
			Constraints.NewPositionEdgeParameter = 0.0;		// "Keep" vertex should stay where it is
		}
		else if (!bVertexIsConstrained && bNextVertexIsConstrained)
		{
			Constraints.KeepVertexIndex = VertexB;
			Constraints.DeleteVertexIndex = VertexA;
			Constraints.NewPositionEdgeParameter = 0.0;	    // "Keep" vertex should stay where it is
		}
		else
		{
			// unconstrained
			Constraints.KeepVertexIndex = VertexA;
			Constraints.DeleteVertexIndex = VertexB;
			Constraints.NewPositionEdgeParameter = 0.5;		// Collapse to the edge midpoint
		}

		return Constraints;
	}

	void RemeshBoundaries(UE::Geometry::FDynamicMesh3& Mesh, const TArray<TArray<FIntVector2>>& Seams, double TargetEdgeLength)
	{
		using namespace UE::Geometry;

		auto IsSeamEdge = [](const TArray<TArray<FIntVector2>>& Seams, int EdgeVertexA, int EdgeVertexB)
		{
			for (const TArray<FIntVector2>& Seam : Seams)
			{
				for (int32 StitchID = 0; StitchID < Seam.Num() - 1; ++StitchID)
				{
					const FIntVector2& Stitch = Seam[StitchID];
					const FIntVector2& NextStitch = Seam[StitchID + 1];

					for (int32 Side = 0; Side < 2; ++Side)
					{
						if ((int)Stitch[Side] == EdgeVertexA && (int)NextStitch[Side] == EdgeVertexB)
						{
							return true;
						}

						if ((int)Stitch[Side] == EdgeVertexB && (int)NextStitch[Side] == EdgeVertexA)
						{
							return true;
						}
					}
				}
			}

			return false;
		};

		// constants pulled from FRemesher::SetTargetEdgeLength
		const double MinLength = 0.66 * TargetEdgeLength;
		const double MaxLength = 1.33 * TargetEdgeLength;

		// Get the set of boundary edges up front and then check if they get invalidated later. 
		// If we process edges inside this loop it tends to collapse a bunch of sequential edges, which can lead to very high-valence vertices
		TArray<FIndex2i> BoundaryEdges;
		for (const int EdgeID : Mesh.BoundaryEdgeIndicesItr())
		{
			BoundaryEdges.Add(Mesh.GetEdge(EdgeID).Vert);
		}

		for (const FIndex2i& EdgeVerts : BoundaryEdges)
		{
			const int EdgeID = Mesh.FindEdge(EdgeVerts[0], EdgeVerts[1]);
			if (EdgeID == FDynamicMesh3::InvalidID || !Mesh.IsBoundaryEdge(EdgeID))
			{
				continue;
			}

			if (IsSeamEdge(Seams, EdgeVerts[0], EdgeVerts[1]))
			{
				continue;
			}

			const double EdgeLength = (Mesh.GetVertex(EdgeVerts[0]) - Mesh.GetVertex(EdgeVerts[1])).Length();
			if (EdgeLength < MinLength)
			{
				const Private::FEdgeCollapseParameters CollapseParams = Private::GetBoundaryEdgeCollapseParameters(Mesh, Seams, EdgeVerts);

				if (CollapseParams.bCanCollapse)
				{
					FDynamicMesh3::FEdgeCollapseInfo CollapseInfo;
					EMeshResult CollapseResult = Mesh.CollapseEdge(CollapseParams.KeepVertexIndex, CollapseParams.DeleteVertexIndex, CollapseParams.NewPositionEdgeParameter, CollapseInfo);
				}
			}
			else if (EdgeLength > MaxLength)
			{
				FDynamicMesh3::FEdgeSplitInfo SplitInfo;
				EMeshResult SplitResult = Mesh.SplitEdge(EdgeVerts[0], EdgeVerts[1], SplitInfo);
			}
		};
	}


	//
	// Remeshing away from Seams/Boundaries
	// 

	bool Remesh(UE::Geometry::FDynamicMesh3& Mesh, double TargetEdgeLength, int32 Iterations, float SmoothingRate, bool bUniformSmoothing, const TArray<TArray<FIntVector2>>& Seams, UE::Geometry::FCompactMaps* CompactMaps = nullptr)
	{
		using namespace UE::Geometry;

		//
		// These consts control overall remeshing behavior and are analogs of the properties exposed to the user in the actual Remesh tool
		// (i.e. things we might wish to add to the node properties in the future)
		//

		constexpr bool bReprojectToInputMesh = true;
		constexpr bool bDiscardAttributes = false;
		constexpr bool bUseFullRemeshPasses = true;
		constexpr bool bAllowFlips = true;
		constexpr bool bAllowSplits = true;
		constexpr bool bAllowCollapses = true;
		constexpr bool bPreventNormalFlips = true;
		constexpr bool bPreventTinyTriangles = true;
		constexpr bool bAutoCompact = true;
		constexpr bool bCoarsenBoundaries = false;

		const ERemeshSmoothingType SmoothingType = bUniformSmoothing ? ERemeshSmoothingType::Uniform : ERemeshSmoothingType::MeanValue;

		// Mesh seam behavior
		// Here we are talking UV, Normal, and Color seams, not Cloth Seams
		// This controls bAllowSeamCollapse and bAllowSeamSmoothing on those overlay seams
		// NOTE: These seams are not affected by bReprojectConstraints
		constexpr bool bPreserveSharpEdges = false;

		// Mesh boundaries
		const EEdgeRefineFlags MeshBoundaryConstraint = bCoarsenBoundaries ? EEdgeRefineFlags::NoFlip : EEdgeRefineFlags::FullyConstrained;

		// Group ID boundaries
		const EEdgeRefineFlags PolyGroupBoundaryConstraint = bCoarsenBoundaries ? EEdgeRefineFlags::NoFlip : EEdgeRefineFlags::FullyConstrained;

		// Material ID boundaries
		const EEdgeRefineFlags MaterialBoundaryConstraint = bCoarsenBoundaries ? EEdgeRefineFlags::NoFlip : EEdgeRefineFlags::FullyConstrained;

		// Whether to move boundary vertices back onto the poly-line defined by the original boundary in case of collapse
		const bool bReprojectConstraints = bCoarsenBoundaries;

		// Seam "corners" are held fixed. We use this angle threshold to determine what constitutes a seam corner
		constexpr float SeamCornerThresholdAngleDegrees = 45.0;

		FRemeshMeshOp RemeshOp;

		TSharedPtr<FDynamicMesh3> SourceMesh = MakeShared<FDynamicMesh3>(MoveTemp(Mesh));
		TSharedPtr<FDynamicMeshAABBTree3> SourceSpatial;
		if (bReprojectToInputMesh)
		{
			// acceleration structure is only used for reprojecting
			SourceSpatial = MakeShared<FDynamicMeshAABBTree3>(SourceMesh.Get(), true);
		}

		RemeshOp.OriginalMesh = SourceMesh;
		RemeshOp.OriginalMeshSpatial = SourceSpatial;

		RemeshOp.bDiscardAttributes = bDiscardAttributes;
		RemeshOp.RemeshType = (bUseFullRemeshPasses) ? ERemeshType::FullPass : ERemeshType::Standard;
		RemeshOp.RemeshIterations = Iterations;
		RemeshOp.MaxRemeshIterations = Iterations;
		RemeshOp.ExtraProjectionIterations = 0;		// unused for regular remeshing
		RemeshOp.TriangleCountHint = 0;				// unused for regular remeshing
		RemeshOp.SmoothingStrength = FMath::Clamp(SmoothingRate, 0.0f, 1.0f);
		RemeshOp.SmoothingType = SmoothingType;

		RemeshOp.TargetEdgeLength = TargetEdgeLength;
		RemeshOp.bPreserveSharpEdges = bPreserveSharpEdges;
		RemeshOp.bFlips = bAllowFlips;
		RemeshOp.bSplits = bAllowSplits;
		RemeshOp.bCollapses = bAllowCollapses;
		RemeshOp.bPreventNormalFlips = bPreventNormalFlips;
		RemeshOp.bPreventTinyTriangles = bPreventTinyTriangles;
		RemeshOp.MeshBoundaryConstraint = MeshBoundaryConstraint;
		RemeshOp.GroupBoundaryConstraint = PolyGroupBoundaryConstraint;
		RemeshOp.MaterialBoundaryConstraint = MaterialBoundaryConstraint;
		RemeshOp.bReproject = bReprojectToInputMesh;
		RemeshOp.ProjectionTarget = SourceMesh.Get();
		RemeshOp.ProjectionTargetSpatial = SourceSpatial.Get();
		RemeshOp.bReprojectConstraints = bReprojectConstraints;
		RemeshOp.BoundaryCornerAngleThreshold = SeamCornerThresholdAngleDegrees;
		RemeshOp.TargetMeshLocalToWorld = FTransformSRT3d::Identity();
		RemeshOp.ToolMeshLocalToWorld = FTransformSRT3d::Identity();
		RemeshOp.bUseWorldSpace = false;
		RemeshOp.bParallel = true;


		// Set up constraints for Cloth Seam edges
		UE::Geometry::FMeshConstraints Constraints;
		for (const TArray<FIntVector2>& Seam : Seams)
		{
			if (Seam.Num() == 1)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					constexpr bool bCannotDelete = true;
					constexpr bool bCanMove = false;
					FVertexConstraint VertexConstraint(bCannotDelete, bCanMove);
					Constraints.SetOrCombineVertexConstraint(Seam[0][Side], VertexConstraint);
				}
			}
			else
			{
				for (int32 StitchIndex = 0; StitchIndex < Seam.Num() - 1; ++StitchIndex)
				{
					for (int32 Side = 0; Side < 2; ++Side)
					{
						const int EdgeID = SourceMesh->FindEdge(Seam[StitchIndex][Side], Seam[StitchIndex + 1][Side]);
						check(EdgeID != FDynamicMesh3::InvalidID);
						FEdgeConstraint EdgeConstraint(EEdgeRefineFlags::FullyConstrained);
						Constraints.SetOrUpdateEdgeConstraint(EdgeID, EdgeConstraint);

						constexpr bool bCannotDelete = true;
						constexpr bool bCanMove = false;
						FVertexConstraint VertexConstraint(bCannotDelete, bCanMove);
						Constraints.SetOrCombineVertexConstraint(Seam[StitchIndex][Side], VertexConstraint);
						Constraints.SetOrCombineVertexConstraint(Seam[StitchIndex + 1][Side], VertexConstraint);
					}
				}
			}
		}
		RemeshOp.SetUserSpecifiedConstraints(Constraints);

		constexpr FProgressCancel* Progress = nullptr;		// Don't allow cancel or report progress for now
		RemeshOp.CalculateResult(Progress);

		if (RemeshOp.GetResultInfo().Result == EGeometryResultType::Success)
		{
			TUniquePtr<FDynamicMesh3> ResultMesh = RemeshOp.ExtractResult();
			Mesh = MoveTemp(*ResultMesh);
		}
		else
		{
			return false;
		}

		// compact the input mesh if enabled
		if (bAutoCompact)
		{
			Mesh.CompactInPlace(CompactMaps);
		}

		return true;
	}


	void ProjectTo3D(const UE::Geometry::FDynamicMesh3& Mesh2D,
		const UE::Geometry::FDynamicMesh3& ProjectionTarget2D,
		const UE::Geometry::FDynamicMesh3& ProjectionTarget3D,
		UE::Geometry::FDynamicMesh3& OutMesh3D)
	{
		using namespace UE::Geometry;

		OutMesh3D.Copy(Mesh2D);

		TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3> ProjectionTargetSpatial = MakeShared<UE::Geometry::FDynamicMeshAABBTree3>(&ProjectionTarget2D, true);

		for (const int VertexIndex : Mesh2D.VertexIndicesItr())
		{
			const FVector3d SrcVert = Mesh2D.GetVertex(VertexIndex);

			double Distance;
			const int NearestTriangle = ProjectionTargetSpatial->FindNearestTriangle(SrcVert, Distance);

			const FDistPoint3Triangle3d Dist = TMeshQueries<FDynamicMesh3>::TriangleDistance(ProjectionTarget2D, NearestTriangle, SrcVert);
			const FVector3d Bary = Dist.TriangleBaryCoords;
			const FVector3d InterpolatedPoint = ProjectionTarget3D.GetTriBaryPoint(NearestTriangle, Bary[0], Bary[1], Bary[2]);

			OutMesh3D.SetVertex(VertexIndex, InterpolatedPoint);
		}
	}



	bool Simplify(UE::Geometry::FDynamicMesh3& Mesh, int TargetVertexCount, bool bCoarsenBoundaries, UE::Geometry::FCompactMaps* CompactMaps = nullptr)
	{
		using namespace UE::Geometry;

		//
		// These consts control overall remeshing behavior and are analogs of the properties exposed to the user in the actual Simplify tool
		// (i.e. things we might wish to add to the node properties in the future)
		//

		constexpr ESimplifyType SimplifierType = ESimplifyType::Attribute;
		constexpr bool bDiscardAttributes = false;
		constexpr bool bPreventNormalFlips = true;
		constexpr bool bPreserveSharpEdges = false;
		constexpr bool bPreventTinyTriangles = false;
		constexpr bool bReproject = true;
		constexpr bool bAutoCompact = true;
		constexpr bool bGeometricConstraint = false;

		// Angle threshold in degrees used for testing if two triangles should be considered coplanar, or two lines collinear */
		constexpr float MinimalAngleThreshold = 0.01f;

		// Note PolyEdgeAngleTolerance is very similar to MinimalAngleThreshold, but not redundant b/c the useful ranges are very different (MinimalAngleThreshold should generally be kept very small)
		// Threshold angle change (in degrees) along a polygroup edge, above which a vertex must be added
		constexpr float PolyEdgeAngleTolerance = 0.1f;


		FSimplifyMeshOp Op;

		Op.bDiscardAttributes = bDiscardAttributes;
		Op.bResultMustHaveAttributesEnabled = true;
		Op.bPreventNormalFlips = bPreventNormalFlips;
		Op.bPreserveSharpEdges = bPreserveSharpEdges;
		Op.bAllowSeamCollapse = !bPreserveSharpEdges;
		Op.bPreventTinyTriangles = bPreventTinyTriangles;
		Op.bReproject = bReproject;
		Op.SimplifierType = SimplifierType;
		Op.MinimalPlanarAngleThresh = MinimalAngleThreshold;

		Op.TargetMode = ESimplifyTargetType::VertexCount;
		Op.TargetCount = TargetVertexCount;

		Op.MeshBoundaryConstraint = bCoarsenBoundaries ? EEdgeRefineFlags::CollapseOnly : EEdgeRefineFlags::FullyConstrained;
		Op.GroupBoundaryConstraint = EEdgeRefineFlags::CollapseOnly;
		Op.MaterialBoundaryConstraint = EEdgeRefineFlags::CollapseOnly;

		Op.bGeometricDeviationConstraint = bGeometricConstraint;
		Op.GeometricTolerance = 0.0f;
		Op.PolyEdgeAngleTolerance = PolyEdgeAngleTolerance;

		TSharedPtr<FDynamicMesh3> SourceMesh = MakeShared<FDynamicMesh3>(MoveTemp(Mesh));
		TSharedPtr<FDynamicMeshAABBTree3> SourceSpatial;
		if (bReproject)
		{
			// acceleration structure is only used for reprojecting
			SourceSpatial = MakeShared<FDynamicMeshAABBTree3>(SourceMesh.Get(), true);
		}
		Op.OriginalMesh = SourceMesh;
		Op.OriginalMeshSpatial = SourceSpatial;

		IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
		Op.MeshReduction = MeshReductionModule.GetStaticMeshReductionInterface();

		constexpr FProgressCancel* Progress = nullptr;		// Don't allow cancel or report progress for now
		Op.CalculateResult(Progress);

		if (Op.GetResultInfo().Result == EGeometryResultType::Success)
		{
			TUniquePtr<FDynamicMesh3> ResultMesh = Op.ExtractResult();
			Mesh = MoveTemp(*ResultMesh);
		}
		else
		{
			return false;
		}

		// compact the input mesh if enabled
		if (bAutoCompact)
		{
			Mesh.CompactInPlace(CompactMaps);
		}

		return true;
	}

}


FChaosClothAssetRemeshNode::FChaosClothAssetRemeshNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}


void FChaosClothAssetRemeshNode::EmptySimSelections(const TSharedRef<FManagedArrayCollection>& ClothCollection) const
{
	using namespace UE::Chaos::ClothAsset;

	FCollectionClothSelectionFacade SelectionFacade(ClothCollection);

	const TArray<FName> SelectionNames = SelectionFacade.GetNames();
	for (const FName& SelectionName : SelectionNames)
	{
		const FName GroupName = SelectionFacade.GetSelectionGroup(SelectionName);
		if (GroupName == ClothCollectionGroup::SimVertices3D || GroupName == ClothCollectionGroup::SimVertices2D || GroupName == ClothCollectionGroup::SimFaces)
		{
			TSet<int32>& SelectionSet = SelectionFacade.GetSelectionSet(SelectionName);
			SelectionSet.Empty();
		}
	}
}

void FChaosClothAssetRemeshNode::RemeshSimMesh(const TSharedRef<const FManagedArrayCollection>& ClothCollection,
	const TSharedRef<FManagedArrayCollection>& OutClothCollection) const
{
	using namespace UE::Geometry;
	using namespace UE::Chaos::ClothAsset;

	FCollectionClothConstFacade InClothFacade(ClothCollection);

	if (InClothFacade.GetNumSimPatterns() == 0)
	{
		FClothGeometryTools::DeleteSimMesh(OutClothCollection);
		return;
	}

	// Convert input patterns to a DynamicMesh

	FClothPatternToDynamicMesh Converter;

	FDynamicMesh3 Mesh2D;
	Converter.Convert(ClothCollection, INDEX_NONE, EClothPatternVertexType::Sim2D, Mesh2D);

	const double TotalArea = TMeshQueries<FDynamicMesh3>::GetVolumeArea(Mesh2D).Y;
	const int TriangleCount = Mesh2D.TriangleCount();

	// Copy pattern IDs into polygroup layer

	Mesh2D.EnableAttributes();
	const int32 PatternIndexLayerID = Mesh2D.Attributes()->NumPolygroupLayers();
	Mesh2D.Attributes()->SetNumPolygroupLayers(PatternIndexLayerID + 1);
	FDynamicMeshPolygroupAttribute* const PatternIndexLayer = Mesh2D.Attributes()->GetPolygroupLayer((int)PatternIndexLayerID);
	check(Mesh2D.TriangleCount() == InClothFacade.GetNumSimFaces());
	for (int32 FaceIndex = 0; FaceIndex < InClothFacade.GetNumSimFaces(); ++FaceIndex)
	{
		const int32 PatternID = InClothFacade.FindSimPatternByFaceIndex(FaceIndex);
		PatternIndexLayer->SetValue(FaceIndex, PatternID);
	}

	// Grab the seam information from the input collection
	
	// Seams are comprised of a set of Stitches. Each Stitch is simply a pair of vertex indices indicating vertices that should be welded to form the 3D mesh.
	
	// Stitches are given in random order within the Seam. To make remeshing them easier, we will find connected strips of stitches and store them in sequential order.
	// So the vertices in Stitch N are connected to the vertices in Stitch N+1.

	TArray<TArray<FIntVector2>> Seams;
	for (int32 SeamIndex = 0; SeamIndex < InClothFacade.GetNumSeams(); ++SeamIndex)
	{
		FCollectionClothSeamConstFacade SeamFacade = InClothFacade.GetSeam(SeamIndex);
		FClothGeometryTools::BuildConnectedSeams2D(ClothCollection, SeamIndex, Mesh2D, Seams);

		// Check seams are valid
		for (int32 SeamID = 0; SeamID < Seams.Num(); ++SeamID)
		{
			const TArray<FIntVector2>& SubSeam = Seams[SeamID];
			for (int32 StitchID = 0; StitchID < SubSeam.Num() - 1; ++StitchID)
			{
				const int32 NextStitchID = StitchID + 1;
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 StitchVert = SubSeam[StitchID][Side];
					const int32 NextStitchVert = SubSeam[NextStitchID][Side];

					if (StitchVert == NextStitchVert)
					{
						continue;
					}

					const int32 FoundEdge = Mesh2D.FindEdge(StitchVert, NextStitchVert);

					// This would indicate a problem in BuildConnectedSeams
					checkf(FoundEdge != UE::Geometry::FDynamicMesh3::InvalidID, TEXT("Could not find a mesh edge between sequential seam vertices %d, %d"), StitchVert, NextStitchVert);
				}
			}
		}
	}


	// For remeshing purposes, we also want to find and constrain any vertices that connect the two seam sides together
	// (This is only relevant for "internal" seams, which connect vertices within the same pattern)
	for (TArray<FIntVector2>& Seam : Seams)
	{
		if (Seam.Num() == 0)
		{
			continue;
		}

		for (const FIntVector2& EndStitch : {Seam[0], Seam.Last()})
		{
			for (const int NeighborA : Mesh2D.VtxVerticesItr(EndStitch[0]))
			{
				bool bMatchFound = false;

				for (const int NeighborB : Mesh2D.VtxVerticesItr(EndStitch[1]))
				{
					if (NeighborA == NeighborB)
					{
						if (Seam.Contains(FIntVector2(NeighborA, NeighborB)))
						{
							continue;
						}

						if (EndStitch == Seam[0])
						{
							Seam.Insert(FIntVector2(NeighborA, NeighborB), 0);
						}
						else
						{
							Seam.Add(FIntVector2(NeighborA, NeighborB));
						}

						bMatchFound = true;
						break;
					}
				}

				if (bMatchFound)
				{
					break;
				}
			}
		}
	}

	// Remesh seams

	const int TargetTriangleCount = FMath::RoundToInt(static_cast<float>(TargetPercentSim) / 100.0f * static_cast<float>(TriangleCount));
	const double TargetEdgeLength = FRemeshMeshOp::CalculateTargetEdgeLength(nullptr, TargetTriangleCount, TotalArea);

	for (int32 ResampleIter = 0; ResampleIter < IterationsSim; ++ResampleIter)
	{
		Private::RemeshSeams(Mesh2D, Seams, TargetEdgeLength);
	}

	// Remesh boundaries

	for (int32 ResampleIter = 0; ResampleIter < IterationsSim; ++ResampleIter)
	{
		Private::RemeshBoundaries(Mesh2D, Seams, TargetEdgeLength);
	}

	// Do the remeshing of the rest of the mesh

	UE::Geometry::FCompactMaps CompactMaps;
	constexpr bool bUniformSmoothing = true;
	Private::Remesh(Mesh2D, TargetEdgeLength, IterationsSim, SmoothingSim, bUniformSmoothing, Seams, &CompactMaps);

	// Update stitches
	for (TArray<FIntVector2>& Seam : Seams)
	{
		for (FIntVector2& Stitch : Seam)
		{
			Stitch[0] = CompactMaps.GetVertexMapping(Stitch[0]);
			Stitch[1] = CompactMaps.GetVertexMapping(Stitch[1]);
			checkf(Stitch[0] != FDynamicMesh3::InvalidID, TEXT("Stitch vertex %d was deleted by remeshing"), Stitch[0]);
			checkf(Stitch[1] != FDynamicMesh3::InvalidID, TEXT("Stitch vertex %d was deleted by remeshing"), Stitch[1]);
		}
	}


	// Project the 3D vertices onto the input 3D mesh

	FDynamicMesh3 SourceMesh2D;
	Converter.Convert(ClothCollection, INDEX_NONE, EClothPatternVertexType::Sim2D, SourceMesh2D);
	FDynamicMesh3 SourceMesh3D;
	Converter.Convert(ClothCollection, INDEX_NONE, EClothPatternVertexType::Sim3D, SourceMesh3D);

	FDynamicMesh3 Mesh3D;
	Private::ProjectTo3D(Mesh2D, SourceMesh2D, SourceMesh3D, Mesh3D);


	// Build the output cloth sim mesh

	constexpr bool bAppendToExistingMesh = false;
	constexpr bool bTransferWeightMaps = true;
	constexpr bool bTransferSimSkinningData = true;
	TMap<int, int32> DynamicMeshToClothVertexMap;
	FClothGeometryTools::BuildSimMeshFromDynamicMeshes(OutClothCollection, Mesh2D, Mesh3D, PatternIndexLayerID, bTransferWeightMaps, bTransferSimSkinningData, bAppendToExistingMesh, DynamicMeshToClothVertexMap);


	// Re-apply the seam info from the input sim mesh. This will create a new Seam for each set of connected stitches

	FCollectionClothFacade OutClothFacade(OutClothCollection);
	for (int32 SeamIndex = 0; SeamIndex < Seams.Num(); ++SeamIndex)
	{
		const TArray<FIntVector2>& Seam = Seams[SeamIndex];

		TArray<FIntVector2> NewSeam;
		for (const FIntVector2& Stitch : Seam)
		{
			if (Stitch[0] == Stitch[1])
			{
				continue;
			}

			FIntVector2 NewStitch;
			NewStitch[0] = DynamicMeshToClothVertexMap[Stitch[0]];
			NewStitch[1] = DynamicMeshToClothVertexMap[Stitch[1]];
			NewSeam.Add(NewStitch);
		}

		FCollectionClothSeamFacade NewSeamFacade = OutClothFacade.AddGetSeam();
		NewSeamFacade.Initialize(NewSeam);
	}
}


void FChaosClothAssetRemeshNode::RebuildTopologyDependentSimData(const TSharedRef<const FManagedArrayCollection>& InClothCollection,
	const TSharedRef<FManagedArrayCollection>& OutClothCollection) const
{
	using namespace UE::Chaos::ClothAsset;
	using namespace ::Chaos::Softs;

	FCollectionClothConstFacade InClothFacade(InClothCollection);
	FCollectionClothFacade OutClothFacade(OutClothCollection);

	// Check that weight maps and skinning info have been interpolated over
	for (const FName& InWeightMapName : InClothFacade.GetWeightMapNames())
	{
		const FName* FoundName = Algo::Find(OutClothFacade.GetWeightMapNames(), InWeightMapName);
		checkf(FoundName, TEXT("Weight map %s was not copied to the output cloth collection"), *InWeightMapName.ToString());
	}
	if (InClothFacade.GetSimBoneIndices().Num() > 0 && OutClothFacade.GetNumSimVertices3D() > 0)
	{
		checkf(OutClothFacade.GetSimBoneIndices().Num() > 0, TEXT("Skinning bone indices not copied to the sim mesh of the output cloth collection"));
	}
	if (InClothFacade.GetSimBoneWeights().Num() > 0 && OutClothFacade.GetNumSimVertices3D() > 0)
	{
		checkf(OutClothFacade.GetSimBoneWeights().Num() > 0, TEXT("Skinning bone weights not copied to the sim mesh of the output cloth collection"));
	}

	Chaos::Softs::FCollectionPropertyConstFacade InProperties(InClothCollection);

	// Reconstruct collision spheres
	const FString SelfCollisionSphereStiffnessString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationSelfCollisionSpheresConfigNode, SelfCollisionSphereStiffness);
	const FString SelfCollisionSphereRadiusString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationSelfCollisionSpheresConfigNode, SelfCollisionSphereRadius);
	const FString SelfCollisionSphereRadiusCullMultiplierString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationSelfCollisionSpheresConfigNode, SelfCollisionSphereRadiusCullMultiplier);
	const FString SelfCollisionSphereSetNameString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationSelfCollisionSpheresConfigNode, SelfCollisionSphereSetName);

	if (InProperties.GetKeyIndex(SelfCollisionSphereStiffnessString) != INDEX_NONE)
	{
		const float SelfCollisionSphereRadius = InProperties.GetValue<float>(SelfCollisionSphereRadiusString);
		const float SelfCollisionSphereRadiusCullMultiplier = InProperties.GetValue<float>(SelfCollisionSphereRadiusCullMultiplierString);
		const float CullDiameterSq = FMath::Square(SelfCollisionSphereRadius * SelfCollisionSphereRadiusCullMultiplier * 2.f);

		if (OutClothFacade.IsValid() && CullDiameterSq > 0.f)
		{
			TConstArrayView<FVector3f> SimPositions = OutClothFacade.GetSimPosition3D();
			TSet<int32> VertexSet;
			FClothGeometryTools::SampleVertices(SimPositions, CullDiameterSq, VertexSet);

			FCollectionClothSelectionFacade Selection(OutClothCollection);
			Selection.DefineSchema();

			const FName SelectionSetName(*InProperties.GetStringValue(SelfCollisionSphereSetNameString, SelfCollisionSphereSetNameString));
			Selection.FindOrAddSelectionSet(SelectionSetName, ClothCollectionGroup::SimVertices3D) = VertexSet;
		}
	}

	// Reconstruct long-range attachments
	const FString TetherStiffnessString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationLongRangeAttachmentConfigNode, TetherStiffness);
	const FString FixedEndWeightMapString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationLongRangeAttachmentConfigNode, FixedEndWeightMap);
	FString UseGeodesicTethersString = GET_MEMBER_NAME_STRING_CHECKED(FChaosClothAssetSimulationLongRangeAttachmentConfigNode, bUseGeodesicTethers);
	UseGeodesicTethersString.RemoveFromStart(TEXT("b"), ESearchCase::CaseSensitive);  // Property collection names doesn't use the b prefix for booleans

	if (InProperties.GetKeyIndex(TetherStiffnessString) != INDEX_NONE)
	{
		const bool bUseGeodesicTethers = InProperties.GetValue<bool>(UseGeodesicTethersString);
		const FName FixedEndWeightMap(InProperties.GetStringValue(FixedEndWeightMapString));

		UE::Chaos::ClothAsset::FClothEngineTools::GenerateTethers(OutClothCollection, FixedEndWeightMap, bUseGeodesicTethers);
	}
}


void FChaosClothAssetRemeshNode::EmptyRenderSelections(const TSharedRef<FManagedArrayCollection>& ClothCollection) const
{
	using namespace UE::Chaos::ClothAsset;

	FCollectionClothSelectionFacade SelectionFacade(ClothCollection);

	const TArray<FName> SelectionNames = SelectionFacade.GetNames();
	for (const FName& SelectionName : SelectionNames)
	{
		const FName GroupName = SelectionFacade.GetSelectionGroup(SelectionName);
		if (GroupName == ClothCollectionGroup::RenderVertices || GroupName == ClothCollectionGroup::RenderFaces)
		{
			TSet<int32>& SelectionSet = SelectionFacade.GetSelectionSet(SelectionName);
			SelectionSet.Empty();
		}
	}
}




void FChaosClothAssetRemeshNode::RemeshRenderMesh(const TSharedRef<const FManagedArrayCollection>& ClothCollection,
	const TSharedRef<FManagedArrayCollection>& OutClothCollection) const
{
	using namespace UE::Geometry;
	using namespace UE::Chaos::ClothAsset;

	// Get the source mesh
	FClothPatternToDynamicMesh Converter;
	FDynamicMesh3 DynamicMesh;

	// NOTE: When applied to the Render mesh, this Convert function will assign PatternIDs to the MaterialID attribute of the DynamicMesh. 
	// After remeshing we will use the MatrialID attribute to determine which triangles should go into which output pattern.
	Converter.Convert(ClothCollection, INDEX_NONE, EClothPatternVertexType::Render, DynamicMesh);

	check(DynamicMesh.HasAttributes());

	const int InputMeshVertexCount = DynamicMesh.VertexCount();
	const int InputMeshTriangleCount = DynamicMesh.TriangleCount();

	const bool bHasUVs = (DynamicMesh.Attributes()->PrimaryUV() != nullptr);

	const int TargetTriangleCount = FMath::RoundToInt(static_cast<float>(TargetPercentRender) / 100.0f * static_cast<float>(InputMeshTriangleCount));
	const double TargetEdgeLength = FRemeshMeshOp::CalculateTargetEdgeLength(&DynamicMesh, TargetTriangleCount);

	TArray<TArray<FIntVector2>> Seams;

	if (bRemeshRenderSeams)
	{
		// Create pseudo-stitches based on boundary vertex proximity. These stitches aren't going to actually weld vertices together, but they will guide boundary remeshing.
		// The goal is to maintain a vertex pairing along boundaries in order to avoid holes opening up when the mesh deforms due to skinning.
		TArray<FIntVector2> Stitches;
		Private::FindCoincidentBoundaryVertices(DynamicMesh, Stitches);

		FClothGeometryTools::BuildConnectedSeams(Stitches, DynamicMesh, Seams);

		for (int RemeshPass = 0; RemeshPass < RenderSeamRemeshIterations; ++RemeshPass)
		{
			Private::RemeshSeams(DynamicMesh, Seams, TargetEdgeLength);
		}

		// Also remesh the open boundaries that are not constrained by seams
		for (int RemeshPass = 0; RemeshPass < RenderSeamRemeshIterations; ++RemeshPass)
		{
			Private::RemeshBoundaries(DynamicMesh, Seams, TargetEdgeLength);
		}
	}


	UE::Geometry::FCompactMaps CompactMaps;
	if (RemeshMethodRender == EChaosClothAssetRemeshMethod::Remesh)
	{
		constexpr bool bUniformSmoothing = false;	// uniform smoothing can distort the UV layer pretty badly
		const bool bSuccess = Private::Remesh(DynamicMesh, TargetEdgeLength, IterationsRender, SmoothingRender, bUniformSmoothing, Seams, &CompactMaps);
		check(bSuccess);
	}
	else
	{
		const bool bCoarsenBoundariesDuringSimplify = !bRemeshRenderSeams;
		const int TargetVertexCount = FMath::RoundToInt(static_cast<float>(TargetPercentRender) / 100.0f * static_cast<float>(InputMeshVertexCount));
		Private::Simplify(DynamicMesh, TargetVertexCount, bCoarsenBoundariesDuringSimplify, &CompactMaps);
	}

	// Collect outputs

	//
	// Normals
	//

	const bool bHasNormals = (DynamicMesh.Attributes()->PrimaryNormals() != nullptr);
	check(bHasNormals);

	TArray<FVector3f> Normals;
	Normals.SetNum(DynamicMesh.VertexCount());
	const FDynamicMeshNormalOverlay* const NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
	for (const int TriangleIndex : DynamicMesh.TriangleIndicesItr())
	{
		const FIndex3i Tri = DynamicMesh.GetTriangle(TriangleIndex);

		for (int TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
		{
			const int VertexIndex = Tri[TriangleVertexIndex];

			// NOTE: This assumes one normal per vertex in the overlay (i.e. no "hard edges")
			Normals[VertexIndex] = NormalOverlay->GetElementAtVertex(TriangleIndex, VertexIndex);
		}
	}

	//
	// Tangents
	//

	TArray<FVector3f> TangentUs;
	TArray<FVector3f> TangentVs;
	{
		const bool bHasTangentUs = (DynamicMesh.Attributes()->PrimaryTangents() != nullptr);
		const bool bHasTangentVs = (DynamicMesh.Attributes()->PrimaryBiTangents() != nullptr);
		if (!bHasTangentUs || !bHasTangentVs)
		{
			FMeshTangentsf::ComputeDefaultOverlayTangents(DynamicMesh);
		}
		TangentUs.SetNumZeroed(DynamicMesh.VertexCount());
		TangentVs.SetNumZeroed(DynamicMesh.VertexCount());

		const FDynamicMeshNormalOverlay* const TangentUOverlay = DynamicMesh.Attributes()->PrimaryTangents();
		const FDynamicMeshNormalOverlay* const TangentVOverlay = DynamicMesh.Attributes()->PrimaryBiTangents();

		for (const int TriangleIndex : DynamicMesh.TriangleIndicesItr())
		{
			const FIndex3i Tri = DynamicMesh.GetTriangle(TriangleIndex);

			for (int TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
			{
				const int VertexIndex = Tri[TriangleVertexIndex];

				TangentUs[VertexIndex] += TangentUOverlay->GetElementAtVertex(TriangleIndex, VertexIndex);
				TangentVs[VertexIndex] += TangentVOverlay->GetElementAtVertex(TriangleIndex, VertexIndex);
			}
		}

		for (int32 VertexIndex = 0; VertexIndex < DynamicMesh.VertexCount(); ++VertexIndex)
		{
			TangentUs[VertexIndex].Normalize();
			TangentVs[VertexIndex].Normalize();
		}
	}

	//
	// UVs
	//

	// TODO: Account for multiple UV layers
	TArray<FVector2f> UVs;
	if (bHasUVs)
	{
		UVs.SetNum(DynamicMesh.VertexCount());

		const FDynamicMeshUVOverlay* const UVOverlay = DynamicMesh.Attributes()->PrimaryUV();

		// Assume no seams in the dynamic mesh UV overlay
		checkSlow(!UVOverlay->HasInteriorSeamEdges());

		for (const int TriangleIndex : DynamicMesh.TriangleIndicesItr())
		{
			const FIndex3i Tri = DynamicMesh.GetTriangle(TriangleIndex);

			for (int TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
			{
				const int VertexIndex = Tri[TriangleVertexIndex];
				UVs[VertexIndex] = UVOverlay->GetElementAtVertex(TriangleIndex, VertexIndex);
			}
		}
	}

	//
	// Skin Weights
	//

	FDynamicMeshAttributeSet* Attributes = DynamicMesh.Attributes();
	check(Attributes);

	TArray<TArray<int32>> BoneIndices;
	TArray<TArray<float>> BoneWeights;
	BoneIndices.SetNum(DynamicMesh.VertexCount());
	BoneWeights.SetNum(DynamicMesh.VertexCount());

	for (const TPair<FName, TUniquePtr<FDynamicMeshVertexSkinWeightsAttribute>>& SkinWeightLayer : Attributes->GetSkinWeightsAttributes())
	{
		const FDynamicMeshVertexSkinWeightsAttribute* const SkinWeightAttribute = SkinWeightLayer.Value.Get();

		for (const int TriangleIndex : DynamicMesh.TriangleIndicesItr())
		{
			const FIndex3i Tri = DynamicMesh.GetTriangle(TriangleIndex);

			for (int TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
			{
				const int VertexIndex = Tri[TriangleVertexIndex];
				SkinWeightAttribute->GetValue(VertexIndex, BoneIndices[VertexIndex], BoneWeights[VertexIndex]);
			}
		}
	}


	// Find the set of triangles per MaterialID

	TMap<int32, TArray<int32>> MaterialTriangles;

	const FDynamicMeshMaterialAttribute* const MaterialAttribute = Attributes->GetMaterialID();
	check(MaterialAttribute);

	for (const int32 TriangleID : DynamicMesh.TriangleIndicesItr())
	{
		const int32 MaterialID = MaterialAttribute->GetValue(TriangleID);
		if (!MaterialTriangles.Contains(MaterialID))
		{
			MaterialTriangles.Add(MaterialID);
		}
		MaterialTriangles[MaterialID].Add(TriangleID);
	}

	TArray<int32> MaterialIDs;
	MaterialTriangles.GetKeys(MaterialIDs);

	const int32 NumMaterials = MaterialIDs.Num();

	//
	// Populate output cloth collection
	// 

	FClothGeometryTools::DeleteRenderMesh(OutClothCollection);
	FCollectionClothFacade OutClothFacade(OutClothCollection);
	OutClothFacade.SetNumRenderPatterns(NumMaterials);

	for (int32 DestPatternID = 0; DestPatternID < MaterialIDs.Num(); ++DestPatternID)
	{
		FCollectionClothRenderPatternFacade OutClothPatternFacade = OutClothFacade.GetRenderPattern(DestPatternID);
		check(OutClothPatternFacade.GetNumRenderFaces() == 0);
		check(OutClothPatternFacade.GetNumRenderVertices() == 0);

		const int32 SourceMaterialID = MaterialIDs[DestPatternID];
		check(MaterialTriangles.Contains(SourceMaterialID));
		const TArray<int32>& TriangleIDs = MaterialTriangles[SourceMaterialID];

		TSet<int32> VertexIndices;
		for (int32 PatternTriangleIndex = 0; PatternTriangleIndex < TriangleIDs.Num(); ++PatternTriangleIndex)
		{
			const int32 TInd = TriangleIDs[PatternTriangleIndex];
			const FIndex3i Tri = DynamicMesh.GetTriangle(TInd);
			VertexIndices.Add(Tri[0]);
			VertexIndices.Add(Tri[1]);
			VertexIndices.Add(Tri[2]);
		}
		const TArray<int32> SourceVertexIndicesArray = VertexIndices.Array();
		const int32 NumVerticesThisPattern = SourceVertexIndicesArray.Num();

		OutClothPatternFacade.SetNumRenderVertices(NumVerticesThisPattern);
		TArrayView<FVector3f> RenderPosition = OutClothPatternFacade.GetRenderPosition();
		TArrayView<FVector3f> RenderNormal = OutClothPatternFacade.GetRenderNormal();
		TArrayView<FVector3f> RenderTangentU = OutClothPatternFacade.GetRenderTangentU();
		TArrayView<FVector3f> RenderTangentV = OutClothPatternFacade.GetRenderTangentV();
		TArrayView<TArray<FVector2f>> RenderUVs = OutClothPatternFacade.GetRenderUVs();
		TArrayView<FLinearColor> RenderColor = OutClothPatternFacade.GetRenderColor();
		TArrayView<TArray<int32>> RenderBoneIndices = OutClothPatternFacade.GetRenderBoneIndices();
		TArrayView<TArray<float>> RenderBoneWeights = OutClothPatternFacade.GetRenderBoneWeights();
		
		TMap<int32, int32> SourceToDestVertexMap;
		for (int32 PatternVertexIndex = 0; PatternVertexIndex < NumVerticesThisPattern; ++PatternVertexIndex)
		{
			const int32 SourceVertexIndex = SourceVertexIndicesArray[PatternVertexIndex];

			SourceToDestVertexMap.Add(SourceVertexIndex, PatternVertexIndex);

			RenderPosition[PatternVertexIndex] = FVector3f(DynamicMesh.GetVertex(SourceVertexIndex));
			if (bHasUVs)
			{
				RenderUVs[PatternVertexIndex].SetNum(MAX_TEXCOORDS);
				RenderUVs[PatternVertexIndex][0] = UVs[SourceVertexIndex];
			}
			if (bHasNormals)
			{
				RenderNormal[PatternVertexIndex] = Normals[SourceVertexIndex];
			}
			RenderTangentU[PatternVertexIndex] = TangentUs[SourceVertexIndex];
			RenderTangentV[PatternVertexIndex] = TangentVs[SourceVertexIndex];
			RenderColor[PatternVertexIndex] = FLinearColor::White;
			RenderBoneIndices[PatternVertexIndex] = BoneIndices[SourceVertexIndex];
			RenderBoneWeights[PatternVertexIndex] = BoneWeights[SourceVertexIndex];
		}

		OutClothPatternFacade.SetNumRenderFaces(TriangleIDs.Num());
		for (int32 PatternTriangleIndex = 0; PatternTriangleIndex < TriangleIDs.Num(); ++PatternTriangleIndex)
		{
			const int32 VertexOffset = OutClothPatternFacade.GetRenderVerticesOffset();
			TArrayView<FIntVector3> RenderIndices = OutClothPatternFacade.GetRenderIndices();

			const int32 TInd = TriangleIDs[PatternTriangleIndex];
			const FIndex3i SourceTri = DynamicMesh.GetTriangle(TInd);

			RenderIndices[PatternTriangleIndex][0] = VertexOffset + SourceToDestVertexMap[SourceTri[0]];
			RenderIndices[PatternTriangleIndex][1] = VertexOffset + SourceToDestVertexMap[SourceTri[1]];
			RenderIndices[PatternTriangleIndex][2] = VertexOffset + SourceToDestVertexMap[SourceTri[2]];
		}

		FCollectionClothConstFacade InClothFacade(ClothCollection);
		FCollectionClothRenderPatternConstFacade InPatternFacade = InClothFacade.GetRenderPattern(SourceMaterialID);
		OutClothPatternFacade.SetRenderMaterialPathName(InPatternFacade.GetRenderMaterialPathName());
	}
}


void FChaosClothAssetRemeshNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);

		// Copy collection to output
		TSharedPtr<FManagedArrayCollection> OutputClothCollection = MakeShared<FManagedArrayCollection>();

		if ((bRemeshSim || bRemeshRender) && ClothFacade.IsValid(EClothCollectionOptionalSchemas::RenderDeformer))
		{
			FClothDataflowTools::LogAndToastWarning(*this, 
				LOCTEXT("InputHasDeformerDataHeadline", "Proxy Deformer Data Found"),
				LOCTEXT("InputHasDeformerDataDetails", "The input Cloth Collection has Proxy Deformer data that will be removed by the Remesh node. Default deformer bindings will be computed in the final asset. Consider placing ProxyDeformer Node after the Remesh Node."));

			// Don't copy proxy deformer data
			const TArray<FName> GroupsToSkip = TArray<FName>();
			TArray<TTuple<FName, FName>> AttributesToSkip;
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerNumInfluences, ClothCollectionGroup::RenderPatterns });
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerPositionBaryCoordsAndDist, ClothCollectionGroup::RenderVertices});
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerNormalBaryCoordsAndDist, ClothCollectionGroup::RenderVertices });
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerTangentBaryCoordsAndDist, ClothCollectionGroup::RenderVertices });
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerSimIndices3D, ClothCollectionGroup::RenderVertices });
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerWeight, ClothCollectionGroup::RenderVertices });
			AttributesToSkip.Add({ ClothCollectionAttribute::RenderDeformerSkinningBlend, ClothCollectionGroup::RenderVertices });
			
			ClothCollection->CopyTo(OutputClothCollection.Get(), GroupsToSkip, AttributesToSkip);
		}
		else
		{
			ClothCollection->CopyTo(OutputClothCollection.Get());
		}

		TSharedRef<FManagedArrayCollection> OutputClothCollectionRef = OutputClothCollection.ToSharedRef();

		if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			if (bRemeshSim)
			{
				EmptySimSelections(OutputClothCollectionRef);
				RemeshSimMesh(ClothCollection, OutputClothCollectionRef);
				RebuildTopologyDependentSimData(ClothCollection, OutputClothCollectionRef);
			}

			if (bRemeshRender)
			{
				EmptyRenderSelections(OutputClothCollectionRef);
				RemeshRenderMesh(ClothCollection, OutputClothCollectionRef);
			}
		}

		SetValue(Context, MoveTemp(*OutputClothCollectionRef), &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
