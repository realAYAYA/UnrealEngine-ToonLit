// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothLodAdapter.h"

// Utility functions to unwrap a 3d sim mesh into a tailored cloth
namespace UE::Chaos::ClothAsset::Private
{
	// Triangle islands to become patterns, although in this case all the seams are internal (same pattern)
	struct FIsland
	{
		TArray<uint32> Indices;  // 3x number of triangles
		TArray<FVector2f> Positions;
		TArray<FVector3f> RestPositions;  // Same size as Positions
		TArray<uint32> SourceIndices;  // Index in the original welded position array
	};

	enum class EIntersectCirclesResult
	{
		SingleIntersect,
		DoubleIntersect,
		Coincident,
		Separate,
		Contained
	};

	static EIntersectCirclesResult IntersectCircles(const FVector2f& C0, float R0, const FVector2f& C1, float R1, FVector2f& OutI0, FVector2f& OutI1)
	{
		const FVector2f C0C1 = C0 - C1;
		const float D = C0C1.Length();
		if (D < SMALL_NUMBER)
		{
			return EIntersectCirclesResult::Coincident;
		}
		else if (D > R0 + R1)
		{
			return EIntersectCirclesResult::Separate;
		}
		else if (D < FMath::Abs(R0 - R1))
		{
			return EIntersectCirclesResult::Contained;
		}
		const float SquareR0 = R0 * R0;
		const float SquareR1 = R1 * R1;
		const float SquareD = D * D;
		const float A = (SquareD - SquareR1 + SquareR0) / (2.f * D);

		OutI0 = OutI1 = C0 + A * (C1 - C0) / D;

		if (FMath::Abs(A - R0) < SMALL_NUMBER)
		{
			return EIntersectCirclesResult::SingleIntersect;
		}

		const float SquareA = A * A;
		const float H = FMath::Sqrt(SquareR0 - SquareA);

		const FVector2f N(C0C1.Y, -C0C1.X);

		OutI0 += N * H / D;
		OutI1 -= N * H / D;

		return EIntersectCirclesResult::DoubleIntersect;
	}
;
	static FUintVector2 MakeSortedUintVector2(uint32 Index0, uint32 Index1)
	{
		return Index0 < Index1 ? FUintVector2(Index0, Index1) : FUintVector2(Index1, Index0);
	}

	static void BuildEdgeMap(const TArray<uint32>& Indices, TMap<FUintVector2, TArray<uint32>>& OutEdgeToTriangles)
	{
		ensure(Indices.Num() % 3 == 0);
		const uint32 NumTriangles = (uint32)Indices.Num() / 3;
		OutEdgeToTriangles.Empty(NumTriangles * 2);  // Rough estimate for the number of edges
		if (!NumTriangles)
		{
			return;
		}

		for (uint32 Triangle = 0; Triangle < NumTriangles; ++Triangle)
		{
			const uint32 Index0 = Indices[Triangle * 3 + 0];
			const uint32 Index1 = Indices[Triangle * 3 + 1];
			const uint32 Index2 = Indices[Triangle * 3 + 2];

			const FUintVector2 Edge0 = MakeSortedUintVector2(Index0, Index1);
			const FUintVector2 Edge1 = MakeSortedUintVector2(Index1, Index2);
			const FUintVector2 Edge2 = MakeSortedUintVector2(Index2, Index0);

			OutEdgeToTriangles.FindOrAdd(Edge0).Add(Triangle);
			OutEdgeToTriangles.FindOrAdd(Edge1).Add(Triangle);
			OutEdgeToTriangles.FindOrAdd(Edge2).Add(Triangle);
		}
	}

	static void UnwrapMesh(const TArray<FVector3f>& Positions, const TArray<uint32>& Indices, TArray<FIsland>& OutIslands)
	{
		OutIslands.Reset();

		ensure(Indices.Num() % 3 == 0);
		const uint32 NumTriangles = (uint32)Indices.Num() / 3;
		if (!NumTriangles)
		{
			return;
		}

		// Gather edge information
		TMap<FUintVector2, TArray<uint32>> EdgeToTrianglesMap;
		BuildEdgeMap(Indices, EdgeToTrianglesMap);

		// Build pattern islands
		TSet<uint32> VisitedTriangles;
		VisitedTriangles.Reserve(NumTriangles);

		constexpr float SquaredWeldingDistance = FMath::Square(0.01f);  // 0.1 mm

		for (uint32 SeedTriangle = 0; SeedTriangle < NumTriangles; ++SeedTriangle)
		{
			if (VisitedTriangles.Contains(SeedTriangle))
			{
				continue;
			}

			const uint32 SeedIndex0 = Indices[SeedTriangle * 3 + 0];
			const uint32 SeedIndex1 = Indices[SeedTriangle * 3 + 1];

			if (FVector3f::DistSquared(Positions[SeedIndex0], Positions[SeedIndex1]) <= SquaredWeldingDistance)
			{
				continue;  // A degenerated triangle edge is not a good start
			}

			// Setup first visitor from seed, and add the first two points
			FIsland& Island = OutIslands.AddDefaulted_GetRef();

			Island.RestPositions.Add(Positions[SeedIndex0]);
			Island.RestPositions.Add(Positions[SeedIndex1]);

			const uint32 SeedIndex2D0 = Island.Positions.Add(FVector2f::ZeroVector);
			const uint32 SeedIndex2D1 = Island.Positions.Add(FVector2f(FVector3f::Dist(Positions[SeedIndex0], Positions[SeedIndex1]), 0.f));

			struct FVisitor
			{
				uint32 Triangle;
				FUintVector2 OldEdge;
				FUintVector2 NewEdge;
				uint32 CrossEdgePoint;  // Keep the opposite point to orientate degenerate cases
			} Visitor =
			{
				SeedTriangle,
				FUintVector2(SeedIndex0, SeedIndex1),
				FUintVector2(SeedIndex2D0, SeedIndex2D1),
				(uint32)INDEX_NONE
			};

			VisitedTriangles.Add(SeedTriangle);

			TQueue<FVisitor> Visitors;
			do
			{
				const uint32 Triangle = Visitor.Triangle;
				const uint32 CrossEdgePoint = Visitor.CrossEdgePoint;
				const uint32 OldIndex0 = Visitor.OldEdge.X;
				const uint32 OldIndex1 = Visitor.OldEdge.Y;
				const uint32 NewIndex0 = Visitor.NewEdge.X;
				const uint32 NewIndex1 = Visitor.NewEdge.Y;

				// Find opposite index from this triangle edge
				const uint32 TriangleIndex0 = Indices[Triangle * 3 + 0];
				const uint32 TriangleIndex1 = Indices[Triangle * 3 + 1];
				const uint32 TriangleIndex2 = Indices[Triangle * 3 + 2];

				const uint32 OldIndex2 =
					(OldIndex0 != TriangleIndex0 && OldIndex1 != TriangleIndex0) ? TriangleIndex0 :
					(OldIndex0 != TriangleIndex1 && OldIndex1 != TriangleIndex1) ? TriangleIndex1 : TriangleIndex2;

				// Find the 2D intersection of the two connecting adjacent edges using the 3D reference length
				const FVector3f& P0 = Positions[OldIndex0];
				const FVector3f& P1 = Positions[OldIndex1];
				const FVector3f& P2 = Positions[OldIndex2];

				const float R0 = FVector3f::Dist(P0, P2);
				const float R1 = FVector3f::Dist(P1, P2);
				const FVector2f C0 = Island.Positions[NewIndex0];
				const FVector2f C1 = Island.Positions[NewIndex1];

				FVector2f I0, I1;
				const EIntersectCirclesResult IntersectCirclesResult = IntersectCircles(C0, R0, C1, R1, I0, I1);

				FVector2f C2;
				switch (IntersectCirclesResult)
				{
				case EIntersectCirclesResult::SingleIntersect:
					C2 = I0;  // Degenerated C2 is on (C0C1)
					break;
				case EIntersectCirclesResult::DoubleIntersect:
					C2 = (FVector2f::CrossProduct(C0 - C1, C0 - I0) > 0) ? I0 : I1;  // Keep correct winding order
					break;
				case EIntersectCirclesResult::Coincident:
					check(CrossEdgePoint != INDEX_NONE); // We can't start on a degenerated triangle
					C2 = C0 - (Island.Positions[CrossEdgePoint] - C0).GetSafeNormal() * R0;  // Degenerated C0 == C1, choose C2 on the opposite of the visitor opposite point
					break;
				case EIntersectCirclesResult::Separate: [[fallthrough]];
				case EIntersectCirclesResult::Contained:
					C2 = C0 - (C1 - C0).GetSafeNormal() * R0;  // Degenerated + some tolerance, C2 is on (C0C1)
					break;
				}

				// Add the new position found for the opposite point
				uint32 NewIndex2 = INDEX_NONE;
				for (uint32 UsedIndex = 0; UsedIndex < (uint32)Island.Positions.Num(); ++UsedIndex)
				{
					if (FVector2f::DistSquared(Island.Positions[UsedIndex], C2) <= SquaredWeldingDistance &&
						FVector3f::DistSquared(Island.RestPositions[UsedIndex], Positions[OldIndex2]) <= SquaredWeldingDistance)
					{
						NewIndex2 = UsedIndex;  // Both Rest and 2D positions match, reuse this index
						break;
					}
				}
				if (NewIndex2 == INDEX_NONE)
				{
					NewIndex2 = Island.Positions.Add(C2);
					Island.RestPositions.Add(Positions[OldIndex2]);
				}

				// Add triangle to list of indices, unless it is degenerated to a segment
				if (NewIndex0 != NewIndex1 && NewIndex1 != NewIndex2 && NewIndex2 != NewIndex0)
				{
					Island.Indices.Add(NewIndex0);
					Island.Indices.Add(NewIndex1);
					Island.Indices.Add(NewIndex2);
					Island.SourceIndices.Add(OldIndex0);
					Island.SourceIndices.Add(OldIndex1);
					Island.SourceIndices.Add(OldIndex2);
				}

				// Add neighbor triangles to the queue
				const FUintVector2 OldEdgeList[3] =
				{
					FUintVector2(OldIndex1, OldIndex0),  // Reversed as to keep the correct winding order
					FUintVector2(OldIndex2, OldIndex1),
					FUintVector2(OldIndex0, OldIndex2)
				};
				const FUintVector3 NewEdgeList[3] =
				{
					FUintVector3(NewIndex1, NewIndex0, NewIndex2),  // Adds opposite point index
					FUintVector3(NewIndex2, NewIndex1, NewIndex0),
					FUintVector3(NewIndex0, NewIndex2, NewIndex1)
				};
				for (int32 Edge = 0; Edge < 3; ++Edge)
				{
					const uint32 EdgeIndex0 = OldEdgeList[Edge].X;
					const uint32 EdgeIndex1 = OldEdgeList[Edge].Y;

					const TArray<uint32>& NeighborTriangles = EdgeToTrianglesMap.FindChecked(MakeSortedUintVector2(EdgeIndex0, EdgeIndex1));

					for (const uint32 NeighborTriangle : NeighborTriangles)
					{
						if (!VisitedTriangles.Contains(NeighborTriangle))
						{
							// Mark neighboring triangle as visited
							VisitedTriangles.Add(NeighborTriangle);

							// Enqueue next triangle
							Visitors.Enqueue(FVisitor
								{
									NeighborTriangle,
									FUintVector2(EdgeIndex0, EdgeIndex1),
									FUintVector2(NewEdgeList[Edge].X, NewEdgeList[Edge].Y),
									NewEdgeList[Edge].Z,  // Pass the cross edge 2D opposite point to help define orientation of any degenerated triangles
								});
						}
					}
				}
			} while (Visitors.Dequeue(Visitor));
		}
	}

	struct FSeam
	{
		TSet<FIntVector2> Stitches;
		FIntVector2 Patterns;
	};

	// Rebuild the seam information from the torn/unwrapped mesh islands data
	// Note that the isolated mesh islands are not technically patterns despite being considered so, 
	// since they aren't sewed together in the source welded mesh.
	// The algorithm will have to be slightly modified to be used with provided UV panels.
	static void BuildSeams(const TArray<FIsland>& Islands, TArray<FSeam>& OutSeams)
	{
		OutSeams.Reset();

		for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
		{
			const FIsland& Island = Islands[IslandIndex];
			
			FSeam Seam;
			Seam.Patterns = FIntVector2(IslandIndex);  // Just patching an unwrap here, will only do pattern to same

			// Gather edge information for the source mesh
			TMap<FUintVector2, TArray<uint32>> SourceEdgeToTrianglesMap;
			BuildEdgeMap(Island.SourceIndices, SourceEdgeToTrianglesMap);

			auto GetTriangleEdgeMatchingSourceEdge = [&Island](uint32 Triangle, const FUintVector2& SourceEdge) -> FUintVector2
				{
					const uint32 TriangleBase = Triangle * 3;
					const uint32 TriangleIndex0 = Island.SourceIndices[TriangleBase + 0];
					const uint32 TriangleIndex1 = Island.SourceIndices[TriangleBase + 1];
					const uint32 TriangleIndex2 = Island.SourceIndices[TriangleBase + 2];

					if (SourceEdge[0] == TriangleIndex0)
					{
						if (SourceEdge[1] == TriangleIndex1)
						{
							return FUintVector2(Island.Indices[TriangleBase + 0], Island.Indices[TriangleBase + 1]);  // Edge 0 1
						}
						else if (SourceEdge[1] == TriangleIndex2)
						{
							return FUintVector2(Island.Indices[TriangleBase + 0], Island.Indices[TriangleBase + 2]);  // Edge 0 2
						}
					}
					else if (SourceEdge[0] == TriangleIndex1)
					{
						if (SourceEdge[1] == TriangleIndex0)
						{
							return FUintVector2(Island.Indices[TriangleBase + 1], Island.Indices[TriangleBase + 0]);  // Edge 1 0
						}
						else if (SourceEdge[1] == TriangleIndex2)
						{
							return FUintVector2(Island.Indices[TriangleBase + 1], Island.Indices[TriangleBase + 2]);  // Edge 1 2
						}
					}
					else if (SourceEdge[0] == TriangleIndex2)
					{
						if (SourceEdge[1] == TriangleIndex0)
						{
							return FUintVector2(Island.Indices[TriangleBase + 2], Island.Indices[TriangleBase + 0]);  // Edge 2 0
						}
						else if (SourceEdge[1] == TriangleIndex1)
						{
							return FUintVector2(Island.Indices[TriangleBase + 2], Island.Indices[TriangleBase + 1]);  // Edge 2 1
						}
					}
					check(false);  // Sanity check
					return FUintVector2(0);
				};

			// Look for disconnected triangles
			for (const TPair<FUintVector2, TArray<uint32>>& SourceEdgeToTriangles : SourceEdgeToTrianglesMap)
			{
				const TArray<uint32>& Triangles = SourceEdgeToTriangles.Value;
				const int32 NumTriangles = Triangles.Num();
				if (NumTriangles > 1)
				{
					const FUintVector2& SourceEdge = SourceEdgeToTriangles.Key;

					for (int32 Index0 = 0; Index0 < NumTriangles - 1; ++Index0)
					{
						const uint32 TriangleIndex0 = Triangles[Index0];

						const FUintVector2 Edge0 = GetTriangleEdgeMatchingSourceEdge(TriangleIndex0, SourceEdge);
						check(Edge0[0] <= (uint32)TNumericLimits<int32>::Max() && Edge0[1] <= (uint32)TNumericLimits<int32>::Max());

						for (int32 Index1 = Index0 + 1; Index1 < NumTriangles; ++Index1)
						{
							const uint32 TriangleIndex1 = Triangles[Index1];

							const FUintVector2 Edge1 = GetTriangleEdgeMatchingSourceEdge(TriangleIndex1, SourceEdge);
							check(Edge1[0] <= (uint32)TNumericLimits<int32>::Max() && Edge1[1] <= (uint32)TNumericLimits<int32>::Max());

							if (Edge0[0] != Edge1[0])
							{
								Seam.Stitches.Emplace(FIntVector2(MakeSortedUintVector2(Edge0[0], Edge1[0])));
							}
							if (Edge0[1] != Edge1[1])
							{
								Seam.Stitches.Emplace(FIntVector2(MakeSortedUintVector2(Edge0[1], Edge1[1])));
							}
						}
					}
				}
			}

			// Add this island's seams
			if (Seam.Stitches.Num())
			{
				OutSeams.Emplace(Seam);
			}
		}
	}
}  // End namespace UE::ClothAsset::Private

// Cloth LOD adapter
namespace UE::Chaos::ClothAsset
{
	FClothLodConstAdapter::FClothLodConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection, int32 InLodIndex)
		: ClothCollection(InClothCollection)
		, LodIndex(InLodIndex)
	{
		check(ClothCollection.IsValid());
		check(LodIndex >= 0 && LodIndex < ClothCollection->NumElements(FClothCollection::LodsGroup));
	}

	FClothLodConstAdapter::FClothLodConstAdapter(const FClothPatternConstAdapter& ClothPatternConstAdapter)
		: ClothCollection(ClothPatternConstAdapter.GetClothCollection())
		, LodIndex(ClothPatternConstAdapter.GetLodIndex())
	{
	}

	FClothPatternConstAdapter FClothLodConstAdapter::GetPattern(int32 PatternIndex) const
	{
		return FClothPatternConstAdapter(ClothCollection, LodIndex, PatternIndex);
	}

	void FClothLodConstAdapter::BuildSimulationMesh(TArray<FVector3f>& Positions, TArray<FVector3f>& Normals, TArray<uint32>& Indices) const
	{
		const int32 NumSimVertices = GetPatternsNumSimVertices();

		// Initialize welding map with same index
		// The welding map redirects to an existing vertex index if these two are part of the same welding group.
		// The redirected index must be the smallest index in the group.
		TArray<int32> WeldingMap;
		WeldingMap.SetNumUninitialized(NumSimVertices);
		for (int32 SimVertexIndex = 0; SimVertexIndex < NumSimVertices; ++SimVertexIndex)
		{
			WeldingMap[SimVertexIndex] = SimVertexIndex;
		}

		// Define welding groups
		// Welding groups contain all stitched pair of indices to be welded together that are required to build the welding map.
		// Key is the smallest redirected index in the group, and will be the one index used in the welding map redirects.
		TMap<int32, TSet<int32>> WeldingGroups;

		auto UpdateWeldingMap =
			[&WeldingMap, &WeldingGroups](int32 Index0, int32 Index1)
		{
			// Only process pairs that are not already redirected to the same index
			if (WeldingMap[Index0] != WeldingMap[Index1])
			{
				// Make sure Index0 points to the the smallest redirected index, so that merges are done into the correct group
				if (WeldingMap[Index0] > WeldingMap[Index1])
				{
					Swap(Index0, Index1);
				}

				// Find the group for Index0 if any
				const int32 Key0 = WeldingMap[Index0];
				TSet<int32>* WeldingGroup0 = WeldingGroups.Find(Key0);
				if (!WeldingGroup0)
				{
					// No existing group, create a new one
					check(Key0 == Index0);  // No group means this index can't already have been redirected  // TODO: Make this a checkSlow
					WeldingGroup0 = &WeldingGroups.Add(Key0);
					WeldingGroup0->Add(Index0);
				}

				// Find the group for Index1, if it exists merge the two groups
				const int32 Key1 = WeldingMap[Index1];
				if (TSet<int32>* const WeldingGroup1 = WeldingGroups.Find(Key1))
				{
					// Update group1 redirected indices with the new key
					for (int32 Index : *WeldingGroup1)
					{
						WeldingMap[Index] = Key0;
					}

					// Merge group0 & group1
					WeldingGroup0->Append(*WeldingGroup1);

					// Remove group1
					WeldingGroups.Remove(Key1);

					// Sanity check
					check(WeldingGroup0->Contains(Key0) && WeldingGroup0->Contains(Key1));  // TODO: Make this a checkSlow
				}
				else
				{
					// Otherwise add Index1 to Index0's group
					check(Key1 == Index1);  // No group means this index can't already have been redirected  // TODO: Make this a checkSlow
					WeldingMap[Index1] = Key0;
					WeldingGroup0->Add(Index1);
				}
			}
		};

		// Apply all seams
		const int32 NumSeams = GetNumSeams();
		const TConstArrayView<TArray<FIntVector2>> SeamStitches = GetSeamStitches();
		for (int32 SeamIndex = 0; SeamIndex < NumSeams; ++SeamIndex)
		{
			for (const FIntVector2& Stitch : SeamStitches[SeamIndex])
			{
				UpdateWeldingMap(Stitch[0], Stitch[1]);
			}
		}

		// Calculate the number of welded vertices
		int32 NumWeldedVertices = NumSimVertices;
		for (const TPair<int32, TSet<int32>>& WeldingGroup : WeldingGroups)
		{
			NumWeldedVertices -= WeldingGroup.Value.Num() - 1;
		}

		// Fill up the vertex arrays
		Positions.SetNumUninitialized(NumWeldedVertices);
		Normals.SetNumUninitialized(NumWeldedVertices);

		const TConstArrayView<FVector3f> SimRestPosition = GetPatternsSimRestPosition();
		const TConstArrayView<FVector3f> SimRestNormal = GetPatternsSimRestNormal();

		TArray<uint32> WeldedIndices;
		WeldedIndices.SetNumUninitialized(NumSimVertices);

		uint32 WeldedIndex = 0;
		for (int32 VertexIndex = 0; VertexIndex < NumSimVertices; ++VertexIndex)
		{
			if (WeldingMap[VertexIndex] == VertexIndex)
			{
				Positions[WeldedIndex] = SimRestPosition[VertexIndex];
				Normals[WeldedIndex] = SimRestNormal[VertexIndex];
				WeldedIndices[VertexIndex] = WeldedIndex++;
			}
			else
			{
				WeldedIndices[VertexIndex] = WeldedIndices[WeldingMap[VertexIndex]];
			}
		}

		// Fill up the face array
		const int32 NumSimFaces = GetPatternsNumSimFaces();
		Indices.SetNumUninitialized(NumSimFaces * 3);

		const TConstArrayView<FIntVector3> SimIndices = GetPatternsSimIndices();

		for (int32 FaceIndex = 0; FaceIndex < NumSimFaces; ++FaceIndex)
		{
			Indices[FaceIndex * 3 + 0] = WeldedIndices[SimIndices[FaceIndex][0]];
			Indices[FaceIndex * 3 + 1] = WeldedIndices[SimIndices[FaceIndex][1]];
			Indices[FaceIndex * 3 + 2] = WeldedIndices[SimIndices[FaceIndex][2]];
		}
	}

	FClothLodAdapter::FClothLodAdapter(const TSharedPtr<FClothCollection>& InClothCollection, int32 InLodIndex)
		: FClothLodConstAdapter(InClothCollection, InLodIndex)
	{
	}

	int32 FClothLodAdapter::AddPattern()
	{
		const int32 PatternElementIndex = GetClothCollection()->AddElements(1, FClothCollection::PatternsGroup);

		GetClothCollection()->PatternEnd[GetElementIndex()] = PatternElementIndex;

		// If this is the first pattern being added, set also the start
		int32& PatternStart = GetClothCollection()->PatternStart[GetElementIndex()];
		if (PatternStart == INDEX_NONE)
		{
			PatternStart = (GetLodIndex() > 0) ? GetClothCollection()->PatternEnd[GetLodIndex() - 1] + 1 : 0;
		}

		const int32 PatternIndex = PatternElementIndex - PatternStart;

		GetPattern(PatternIndex).SetDefaults();

		return PatternIndex;
	}

	FClothPatternAdapter FClothLodAdapter::GetPattern(int32 PatternIndex)
	{
		return FClothPatternAdapter(GetClothCollection(), GetLodIndex(), PatternIndex);
	}

	void FClothLodAdapter::Reset()
	{
		const int32 ElementIndex = GetElementIndex();

		const int32 NumPatterns = GetNumPatterns();
		for (int32 PatternIndex = 0; PatternIndex < NumPatterns; ++PatternIndex)
		{
			GetPattern(PatternIndex).Reset();
		}
		GetClothCollection()->RemoveElements(FClothCollection::PatternsGroup, NumPatterns, GetClothCollection()->PatternStart[ElementIndex]);

		const int32 NumStitchings = GetNumSeams();
		for (int32 StitchingIndex = 0; StitchingIndex < NumStitchings; ++StitchingIndex)
		{
			//GetStitching(StitchingIndex).Reset();  // TODO
		}
		GetClothCollection()->RemoveElements(FClothCollection::SeamsGroup, NumStitchings, GetClothCollection()->SeamStart[ElementIndex]);

		const int32 NumTetherBatches = GetNumTetherBatches();
		for (int32 TetherBatchIndex = 0; TetherBatchIndex < NumTetherBatches; ++TetherBatchIndex)
		{
			//TetherBatch(TetherBatchIndex).Reset();  // TODO
		}
		GetClothCollection()->RemoveElements(FClothCollection::TetherBatchesGroup, NumTetherBatches, GetClothCollection()->TetherBatchStart[ElementIndex]);

		SetDefaults();
	}

	void FClothLodAdapter::SetDefaults()
	{
		const int32 ElementIndex = GetElementIndex();

		GetClothCollection()->PatternStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->PatternEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->SeamStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->SeamEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->TetherBatchStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->TetherBatchEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->LodBiasDepth[ElementIndex] = 0;
	}

	int32 FClothLodAdapter::SetNumSeams(int32 NumSeams)
	{
		return GetClothCollection()->SetNumElements(
			NumSeams,
			FClothCollection::SeamsGroup,
			GetClothCollection()->SeamStart,
			GetClothCollection()->SeamEnd,
			GetElementIndex());
	}

	void FClothLodAdapter::Initialize(const TArray<FVector3f>& Positions, const TArray<uint32>& Indices)
	{
		using namespace UE::Chaos::ClothAsset::Private;

		TArray<FIsland> Islands;
		UnwrapMesh(Positions, Indices, Islands);  // Unwrap to 2D and reconstruct indices on 3D mesh

		for (FIsland& Island : Islands)
		{
			if (Island.Indices.Num() && Island.Positions.Num() && Island.RestPositions.Num())
			{
				FClothPatternAdapter Pattern = AddGetPattern();
				Pattern.Initialize(Island.Positions, Island.RestPositions, Island.Indices);
			}
		}

		TArray<FSeam> Seams;
		BuildSeams(Islands, Seams);  // Build the seam information as to be able to re-weld the mesh for simulation

		SetNumSeams(Seams.Num());

		const TArrayView<TArray<FIntVector2>> SeamStitches = GetSeamStitches();
		const TArrayView<FIntVector2> SeamPatterns = GetSeamPatterns();
		const TArrayView<int32> SimVerticesStart = GetSimVerticesStart();

		for (int32 SeamIndex = 0; SeamIndex < Seams.Num(); ++SeamIndex)
		{
			const FIntVector2& Patterns = Seams[SeamIndex].Patterns;
			SeamPatterns[SeamIndex] = Patterns;

			const int32 NumStitches = Seams[SeamIndex].Stitches.Num();
			SeamStitches[SeamIndex].Reset(NumStitches);
			for (const FIntVector2& Stitch : Seams[SeamIndex].Stitches)
			{
				SeamStitches[SeamIndex].Emplace(Stitch[0] + SimVerticesStart[Patterns[0]], Stitch[1] + SimVerticesStart[Patterns[1]]);
			}
		}
	}
}  // End namespace UE::Chaos::ClothAsset
