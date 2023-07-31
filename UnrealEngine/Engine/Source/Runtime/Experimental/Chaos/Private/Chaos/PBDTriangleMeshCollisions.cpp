// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/Plane.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/Triangle.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/Framework/Parallel.h"
#include <atomic>

namespace Chaos::Softs {

struct FEdgeFaceIntersection
{
	int32 EdgeIndex;
	int32 FaceIndex;
	FSolverReal EdgeCoordinate;
	FSolverVec2 FaceCoordinate;
	FSolverVec3 FaceNormal;
	FSolverVec3 IntersectionPoint;
	bool bFlipGradient = false; // Only used when building global contours
};

// Returned array has NOT been shrunk
template<typename SpatialAccelerator>
static TArray<FEdgeFaceIntersection> FindEdgeFaceIntersections(const FTriangleMesh& TriangleMesh, const SpatialAccelerator& Spatial, const FSolverParticles& Particles)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_IntersectionQuery);

	const FSegmentMesh& SegmentMesh = TriangleMesh.GetSegmentMesh();
	const TArray<TVec2<int32>>& EdgeToFaces = TriangleMesh.GetEdgeToFaces();
	const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();

	TArray<FEdgeFaceIntersection> Intersections;

	// Preallocate enough space for (more than) typical number of expected intersections.
	constexpr int32 PreallocatedIntersectionsPerEdge = 3;
	Intersections.SetNum(PreallocatedIntersectionsPerEdge * SegmentMesh.GetNumElements());
	std::atomic<int32> IntersectionIndex(0);

	// Extra intersections that require a lock to write to if you have more than PreallocatedIntersectionsPerEdge 
	TArray<FEdgeFaceIntersection> ExtraIntersections;
	FCriticalSection CriticalSection;
	PhysicsParallelFor(SegmentMesh.GetNumElements(),
		[&Spatial, &TriangleMesh, &Particles, &SegmentMesh, &EdgeToFaces, &Elements, &IntersectionIndex, &Intersections, PreallocatedIntersectionsPerEdge, &ExtraIntersections, &CriticalSection](int32 EdgeIndex)
		{

			TArray< TTriangleCollisionPoint<FSolverReal> > Result;

			const int32 EdgePointIndex0 = SegmentMesh.GetElements()[EdgeIndex][0];
			const int32 EdgePointIndex1 = SegmentMesh.GetElements()[EdgeIndex][1];
			const FSolverVec3& EdgePosition0 = Particles.X(EdgePointIndex0);
			const FSolverVec3& EdgePosition1 = Particles.X(EdgePointIndex1);


			if (TriangleMesh.EdgeIntersectionQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), EdgeIndex, EdgePosition0, EdgePosition1,
				[&Elements, EdgePointIndex0, EdgePointIndex1](int32 EdgeIndex, int32 TriangleIndex)
				{
					if (EdgePointIndex0 == Elements[TriangleIndex][0] || EdgePointIndex0 == Elements[TriangleIndex][1] || EdgePointIndex0 == Elements[TriangleIndex][2] ||
						EdgePointIndex1 == Elements[TriangleIndex][0] || EdgePointIndex1 == Elements[TriangleIndex][1] || EdgePointIndex1 == Elements[TriangleIndex][2])
					{
						return false;
					}
					return true;
				},
				Result))
			{

				for (int32 CollisionPointIndex = 0; CollisionPointIndex < Result.Num(); ++CollisionPointIndex)
				{
					const TTriangleCollisionPoint<FSolverReal>& CollisionPoint = Result[CollisionPointIndex];

					check(CollisionPoint.ContactType == TTriangleCollisionPoint<FSolverReal>::EContactType::EdgeFace);
					FEdgeFaceIntersection Intersection;
					Intersection.EdgeIndex = CollisionPoint.Indices[0];
					Intersection.FaceIndex = CollisionPoint.Indices[1];
					Intersection.EdgeCoordinate = CollisionPoint.Bary[0];
					Intersection.FaceCoordinate = { CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
					Intersection.FaceNormal = CollisionPoint.Normal;
					Intersection.IntersectionPoint = CollisionPoint.Location;

					if (CollisionPointIndex < PreallocatedIntersectionsPerEdge)
					{
						const int32 IndexToWrite = IntersectionIndex.fetch_add(1);
						Intersections[IndexToWrite] = Intersection;
					}
					else
					{
						CriticalSection.Lock();
						ExtraIntersections.Add(Intersection);
						CriticalSection.Unlock();
					}
				}
			}
		}
	);

	// Set Intersections Num to actual number found.
	const int32 IntersectionNum = IntersectionIndex.load();
	Intersections.SetNum(IntersectionNum, false /*bAllowShrinking*/);

	// Append any ExtraIntersections
	Intersections.Append(ExtraIntersections);
	return Intersections;
}

// Global intersection analysis (identifying global contours, flood filling)
namespace GIA
{
	struct FIntersectionContourTriangleSection
	{
		int32 LoopVertexLocalIndex = INDEX_NONE;
		int32 TriangleIndex = INDEX_NONE;

		TVec2<int32> CrossingEdgeLocalIndex = { INDEX_NONE,INDEX_NONE };

		FIntersectionContourTriangleSection() = default;
		FIntersectionContourTriangleSection(int32 InTriangleIndex)
			: TriangleIndex(InTriangleIndex)
		{}
	};

	using FIntersectionContour = TArray<FIntersectionContourTriangleSection>;

	struct FIntersectionContourPair
	{
		bool bIsClosed = false;
		int8 LoopVertexCount = 0;
		int8 BoundaryEdgeCount = 0;

		FIntersectionContour ColorContours[2];
		TArray<FEdgeFaceIntersection> Intersections;
		TArray<int32> ContourPointCurves[2]; // Track which curves correspond with which Contours. (Sometimes we generate multiple curves per contour)
	};

	// methods for building contours
	namespace ContourBuilding
	{
		// internal helper methods
		namespace __internal
		{
			static inline int32 GetLocalEdgeIndex(const TArray<TVec3<int32>>& FaceToEdges, const int32 FaceIndex, const int32 EdgeIndex)
			{
				for (int32 LocalIndex = 0; LocalIndex < 3; ++LocalIndex)
				{
					if (FaceToEdges[FaceIndex][LocalIndex] == EdgeIndex)
					{
						return LocalIndex;
					}
				}
				check(false); // we should never get here.
				return INDEX_NONE;
			}

			// returns local vertex index within FaceVertices
			// returns INDEX_NONE if no loop vertex found
			static inline int32 FindLoopVertex(const TVec3<int32>& FaceVertices, int32 OppositeVertexIndex)
			{
				for (int32 FaceLocalVertexIndex = 0; FaceLocalVertexIndex < 3; ++FaceLocalVertexIndex)
				{
					if (FaceVertices[FaceLocalVertexIndex] == OppositeVertexIndex)
					{
						return FaceLocalVertexIndex;
					}
				}
				return INDEX_NONE;
			}

			static inline bool IsNextIntersectionEdgeFaceFirstIntersection(const FEdgeFaceIntersection& FirstIntersection, const int32 EdgeFaceCrossingEdgeLocalIndex, const TVec3<int32>& FaceToEdges_EdgeFace, const int32 CurrIntersectionFaceIndex, int32& OutLocalEdgeIndex)
			{
				for (int32 LocalEdgeIndexOffset = 1; LocalEdgeIndexOffset < 3; ++LocalEdgeIndexOffset)
				{
					const int32 LocalEdgeIndex = (LocalEdgeIndexOffset + EdgeFaceCrossingEdgeLocalIndex) % 3;
					if (FirstIntersection.EdgeIndex == FaceToEdges_EdgeFace[LocalEdgeIndex] && FirstIntersection.FaceIndex == CurrIntersectionFaceIndex)
					{
						OutLocalEdgeIndex = LocalEdgeIndex;
						return true;
					}
				}

				return false;
			}

			static inline bool IsNextIntersectionFaceFirstIntersection(const FEdgeFaceIntersection& FirstIntersection, const TVec3<int32>& FaceToEdges_Face, const int32 EdgeFace, int32& OutLocalEdgeIndex)
			{
				for (int32 LocalEdgeIndex = 0; LocalEdgeIndex < 3; ++LocalEdgeIndex)
				{
					if (FirstIntersection.EdgeIndex == FaceToEdges_Face[LocalEdgeIndex] && FirstIntersection.FaceIndex == EdgeFace)
					{
						OutLocalEdgeIndex = LocalEdgeIndex;
						return true;
					}
				}

				return false;
			}

			static inline bool FindNextIntersectionEdgeFace(TMap<TVec2<int32>, FEdgeFaceIntersection>& IntersectionMap, const int32 EdgeFaceCrossingEdgeLocalIndex, const TVec3<int32>& FaceToEdges_EdgeFace, const int32 CurrIntersectionFaceIndex, FEdgeFaceIntersection& NextIntersection, int32& OutLocalEdgeIndex)
			{
				for (int32 LocalEdgeIndexOffset = 1; LocalEdgeIndexOffset < 3; ++LocalEdgeIndexOffset)
				{
					const int32 LocalEdgeIndex = (LocalEdgeIndexOffset + EdgeFaceCrossingEdgeLocalIndex) % 3;
					if (IntersectionMap.RemoveAndCopyValue({ FaceToEdges_EdgeFace[LocalEdgeIndex], CurrIntersectionFaceIndex }, NextIntersection))
					{
						OutLocalEdgeIndex = LocalEdgeIndex;
						return true;
					}
				}
				return false;
			}

			static inline bool FindNextIntersectionFace(TMap<TVec2<int32>, FEdgeFaceIntersection>& IntersectionMap, const TVec3<int32>& FaceToEdges_Face, const int32 EdgeFace, FEdgeFaceIntersection& NextIntersection, int32& OutLocalEdgeIndex)
			{
				for (int32 LocalEdgeIndex = 0; LocalEdgeIndex < 3; ++LocalEdgeIndex)
				{
					if (IntersectionMap.RemoveAndCopyValue({ FaceToEdges_Face[LocalEdgeIndex], EdgeFace }, NextIntersection))
					{
						OutLocalEdgeIndex = LocalEdgeIndex;
						return true;
					}
				}
				return false;
			}
		} // namespace __internal

		// Build intersection contour data from edge-face intersection data.
		static TArray<FIntersectionContourPair> BuildIntersectionContours(const FTriangleMesh& TriangleMesh, const TArray<FEdgeFaceIntersection>& IntersectionArray, TArray<TArray<FPBDTriangleMeshCollisions::FBarycentricPoint>>& ContourPoints)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_BuildIntersectionContours);

			// Put intersections in a Map so we can look them up by Edge, Face
			// Key is (EdgeIndex, FaceIndex)
			TMap<TVec2<int32>, FEdgeFaceIntersection> IntersectionMap;
			IntersectionMap.Reserve(IntersectionArray.Num());
			for (const FEdgeFaceIntersection& Intersection : IntersectionArray)
			{
				IntersectionMap.Add({ Intersection.EdgeIndex, Intersection.FaceIndex }, Intersection);
			}

			const FSegmentMesh& SegmentMesh = TriangleMesh.GetSegmentMesh();
			const TArray<TVec2<int32>>& EdgeToFaces = TriangleMesh.GetEdgeToFaces();
			const TArray<TVec3<int32>>& FaceToEdges = TriangleMesh.GetFaceToEdges();
			const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();

			TArray<FIntersectionContourPair> Contours;

			while (!IntersectionMap.IsEmpty())
			{
				// Add a new contour
				FIntersectionContourPair& ContourPair = Contours.AddDefaulted_GetRef();

				// Set up first intersection
				TMap<TVec2<int32>, FEdgeFaceIntersection>::TConstIterator Iter = IntersectionMap.CreateConstIterator();
				const FEdgeFaceIntersection FirstIntersection = IntersectionMap.FindAndRemoveChecked(Iter.Key());

				// Track which intersections are used by which contour mostly so we can build contour minimization contours with these.
				ContourPair.Intersections.Add_GetRef(FirstIntersection).bFlipGradient = false;

				// Arbitrarily set colors to start contour
				constexpr int32 InitialFaceContourIndex = 0;
				constexpr int32 InitialEdgeContourIndex = 1;

				FIntersectionContour* FaceContour = &ContourPair.ColorContours[InitialFaceContourIndex];
				FIntersectionContour* EdgeContour = &ContourPair.ColorContours[InitialEdgeContourIndex];
				ContourPair.ContourPointCurves[InitialFaceContourIndex].Add(ContourPoints.Num());
				TArray<FPBDTriangleMeshCollisions::FBarycentricPoint>* FaceContourPoints = &ContourPoints.AddDefaulted_GetRef();
				ContourPair.ContourPointCurves[InitialEdgeContourIndex].Add(ContourPoints.Num());
				TArray<FPBDTriangleMeshCollisions::FBarycentricPoint>* EdgeContourPoints = &ContourPoints.AddDefaulted_GetRef();

				const int32 FirstEdgeFace = EdgeToFaces[FirstIntersection.EdgeIndex][0];
				check(FirstEdgeFace != -1); // Each Edge should be connected to at least one face

				// We will rely on finding the First FaceSection and EdgeFaceSection as the [0] element in these arrays.
				check(FaceContour->Num() == 0);
				check(EdgeContour->Num() == 0);


				// Start main loop to consume intersections to build this contour
				bool bReverseDirection = false; // when not reversing, write to CrossingEdgeLocalIndex[0] first. when reversing, writing to CrossingEdgeLocalIndex[1] first.
				FEdgeFaceIntersection CurrIntersection = FirstIntersection;
				int32 EdgeFace = FirstEdgeFace;
				FIntersectionContourTriangleSection* FaceSection = &FaceContour->Add_GetRef(FIntersectionContourTriangleSection(FirstIntersection.FaceIndex));
				FIntersectionContourTriangleSection* EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(FirstEdgeFace));

				// Setup first EdgeFaceSection crossing
				EdgeFaceSection->CrossingEdgeLocalIndex[0] = __internal::GetLocalEdgeIndex(FaceToEdges, FirstEdgeFace, FirstIntersection.EdgeIndex);
				while (1)
				{
					const TVec3<int32>& FaceVertices = Elements[CurrIntersection.FaceIndex];
					const TVec3<int32>& EdgeFaceVertices = Elements[EdgeFace];

					FaceContourPoints->Add({ CurrIntersection.FaceCoordinate, FaceVertices });
					EdgeContourPoints->Add({ {CurrIntersection.EdgeCoordinate, 0.f}, {EdgeFaceVertices[0], EdgeFaceVertices[1], EdgeFaceVertices[1]} });

					// Walk the intersection contour. Next point in contour is either
					// 1) Loop vertex
					// 2) Intersection between another edge on EdgeFace and Face
					//   a) This is FirstIntersection (i.e., we closed the loop)
					//   b) This is a previously untouched intersection
					// 3) Intersection between an edge on Face with EdgeFace
					//   a) This is FirstIntersection (i.e., we closed the loop)
					//   b) This is a previously untouched intersection

					// General note on limitations: this algorithm can fail to correctly reconstruct complex contours with combinations of loop vertices and sliver regions that double back on themselves.
					// These contours will be handled by contour minimization.

					// Check 1)
					const int32 EdgeFaceCrossingEdgeLocalIndex = bReverseDirection ? EdgeFaceSection->CrossingEdgeLocalIndex[1] : EdgeFaceSection->CrossingEdgeLocalIndex[0];
					check(EdgeFaceCrossingEdgeLocalIndex != INDEX_NONE);
					const int32 FaceLoopVertexLocalIndex = __internal::FindLoopVertex(FaceVertices, EdgeFaceVertices[(EdgeFaceCrossingEdgeLocalIndex + 2) % 3]);
					if (FaceLoopVertexLocalIndex != INDEX_NONE)
					{
						FaceSection->LoopVertexLocalIndex = FaceLoopVertexLocalIndex;
						EdgeFaceSection->LoopVertexLocalIndex = (EdgeFaceCrossingEdgeLocalIndex + 2) % 3;
						++ContourPair.LoopVertexCount;

						if (ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 2)
						{
							// We've found both ends of this contour.
							ContourPair.bIsClosed = ContourPair.LoopVertexCount == 2;
							break;
						}
						check(ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 1);
						// We hit one loop vertex. Pick up at the FirstIntersection and move the opposite direction if possible.

						EdgeFace = EdgeToFaces[FirstIntersection.EdgeIndex][1];
						if (EdgeFace == -1)
						{
							// First intersection edge was a boundary, so this contour is done. We're NOT closed.
							++ContourPair.BoundaryEdgeCount;
							break;
						}

						check(bReverseDirection == false);
						bReverseDirection = true;
						CurrIntersection = FirstIntersection;
						FaceContour = &ContourPair.ColorContours[InitialFaceContourIndex];
						EdgeContour = &ContourPair.ColorContours[InitialEdgeContourIndex];

						// ContourPoints are currently just used for debug drawing. Start new contours for reverse section
						ContourPair.ContourPointCurves[InitialFaceContourIndex].Add(ContourPoints.Num());
						FaceContourPoints = &ContourPoints.AddDefaulted_GetRef();
						ContourPair.ContourPointCurves[InitialEdgeContourIndex].Add(ContourPoints.Num());
						EdgeContourPoints = &ContourPoints.AddDefaulted_GetRef();

						FaceSection = &ContourPair.ColorContours[InitialFaceContourIndex][0];
						EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(EdgeFace));
						EdgeFaceSection->CrossingEdgeLocalIndex[1] = __internal::GetLocalEdgeIndex(FaceToEdges, EdgeFace, CurrIntersection.EdgeIndex);
						continue;
					}

					// Check 2a)
					int32 LocalEdgeIndex;
					if (!bReverseDirection && __internal::IsNextIntersectionEdgeFaceFirstIntersection(FirstIntersection, EdgeFaceCrossingEdgeLocalIndex, FaceToEdges[EdgeFace], CurrIntersection.FaceIndex, LocalEdgeIndex))
					{
						// We can close the contour
						EdgeFaceSection->CrossingEdgeLocalIndex[1] = LocalEdgeIndex;

						FIntersectionContourTriangleSection& FirstFaceSection = ContourPair.ColorContours[InitialFaceContourIndex][0];

						// Merge current FaceSection and FirstFaceSection (if they're not already the same)
						check(FaceContour == &ContourPair.ColorContours[InitialFaceContourIndex]);
						check(FaceSection->TriangleIndex == FirstFaceSection.TriangleIndex);
						if (FaceSection != &FirstFaceSection)
						{
							check(FaceSection->CrossingEdgeLocalIndex[0] != INDEX_NONE);
							FirstFaceSection.CrossingEdgeLocalIndex[0] = FaceSection->CrossingEdgeLocalIndex[0];
							check(FaceSection == &FaceContour->Last());
							FaceContour->RemoveAt(FaceContour->Num() - 1, 1, false);
						}

						ContourPair.bIsClosed = true;

						// Repeat first point in contour points for ease of drawing closed loop
						FaceContourPoints->Add(FPBDTriangleMeshCollisions::FBarycentricPoint((*FaceContourPoints)[0]));
						EdgeContourPoints->Add(FPBDTriangleMeshCollisions::FBarycentricPoint((*EdgeContourPoints)[0]));
						break;
					}

					// Check 3a)
					if (!bReverseDirection && __internal::IsNextIntersectionFaceFirstIntersection(FirstIntersection, FaceToEdges[CurrIntersection.FaceIndex], EdgeFace, LocalEdgeIndex))
					{
						// We can close the contour
						FaceSection->CrossingEdgeLocalIndex[1] = LocalEdgeIndex;

						// Merge current EdgeFaceSection with FirstFaceSection
						FIntersectionContourTriangleSection& FirstFaceSection = ContourPair.ColorContours[InitialFaceContourIndex][0];
						check(EdgeContour == &ContourPair.ColorContours[InitialFaceContourIndex]);
						check(EdgeFaceSection->TriangleIndex == FirstFaceSection.TriangleIndex);
						check(EdgeFaceSection->CrossingEdgeLocalIndex[0] != INDEX_NONE);
						FirstFaceSection.CrossingEdgeLocalIndex[0] = EdgeFaceSection->CrossingEdgeLocalIndex[0];

						check(EdgeFaceSection == &EdgeContour->Last());
						EdgeContour->RemoveAt(EdgeContour->Num() - 1, 1, false);

						ContourPair.bIsClosed = true;

						// Repeat first point in contour points for ease of drawing closed loop
						FaceContourPoints->Add(FPBDTriangleMeshCollisions::FBarycentricPoint((*FaceContourPoints)[0]));
						EdgeContourPoints->Add(FPBDTriangleMeshCollisions::FBarycentricPoint((*EdgeContourPoints)[0]));
						break;
					}

					// Check 2b)
					FEdgeFaceIntersection NextIntersection;
					if (__internal::FindNextIntersectionEdgeFace(IntersectionMap, EdgeFaceCrossingEdgeLocalIndex, FaceToEdges[EdgeFace], CurrIntersection.FaceIndex, NextIntersection, LocalEdgeIndex))
					{
						// We found the second crossing for EdgeFaceSection. Mark it. EdgeFaceSection is now complete!
						int32& SecondCrossingEdgeLocalIndex = bReverseDirection ? EdgeFaceSection->CrossingEdgeLocalIndex[0] : EdgeFaceSection->CrossingEdgeLocalIndex[1];
						check(SecondCrossingEdgeLocalIndex == INDEX_NONE);
						SecondCrossingEdgeLocalIndex = LocalEdgeIndex;

						// Add this intersection to the list of intersections consumed by this contour
						const bool bFlipGradient = EdgeContour == &ContourPair.ColorContours[InitialFaceContourIndex]; // Flip gradient if the edge of this intersection belongs to the contour that started with the face of the first intersection
						ContourPair.Intersections.Add_GetRef(NextIntersection).bFlipGradient = bFlipGradient;


						// Add new FaceEdge section by choosing the face edge that doesn't correspond with our current faceEdge (if it exists)

						const int32 NextEdgeFace = EdgeToFaces[NextIntersection.EdgeIndex][0] == EdgeFace ? EdgeToFaces[NextIntersection.EdgeIndex][1] : EdgeToFaces[NextIntersection.EdgeIndex][0];
						if (NextEdgeFace == -1)
						{
							// We've hit a boundary.
							++ContourPair.BoundaryEdgeCount;

							if (ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 2)
							{
								// We've hit both ends of this contour. Since (at least) one end is a boundary, we won't be able to form a closed contour
								break;
							}

							check(ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 1);

							// Pick up at the FirstIntersection and move the opposite direction if possible.
							EdgeFace = EdgeToFaces[FirstIntersection.EdgeIndex][1];
							if (EdgeFace == -1)
							{
								// First intersection edge was a boundary, so this contour is done. We're NOT closed.
								++ContourPair.BoundaryEdgeCount;
								break;
							}

							check(bReverseDirection == false);
							bReverseDirection = true;
							CurrIntersection = FirstIntersection;
							FaceContour = &ContourPair.ColorContours[0];
							EdgeContour = &ContourPair.ColorContours[1];

							// ContourPoints are currently just used for debug drawing. Start new contours for reverse section
							ContourPair.ContourPointCurves[InitialFaceContourIndex].Add(ContourPoints.Num());
							FaceContourPoints = &ContourPoints.AddDefaulted_GetRef();
							ContourPair.ContourPointCurves[InitialEdgeContourIndex].Add(ContourPoints.Num());
							EdgeContourPoints = &ContourPoints.AddDefaulted_GetRef();

							FaceSection = &ContourPair.ColorContours[InitialFaceContourIndex][0];
							EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(EdgeFace));
							EdgeFaceSection->CrossingEdgeLocalIndex[1] = __internal::GetLocalEdgeIndex(FaceToEdges, EdgeFace, CurrIntersection.EdgeIndex);

							continue;
						}

						// Setup for next iteration of loop
						// No change in which contour in the contour pair represents the "face" vs "faceEdge" contour (face section hasn't changed)
						CurrIntersection = NextIntersection;
						EdgeFace = NextEdgeFace;

						EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(EdgeFace));
						int32& FirstCrossingLocalIndex = bReverseDirection ? EdgeFaceSection->CrossingEdgeLocalIndex[1] : EdgeFaceSection->CrossingEdgeLocalIndex[0];
						FirstCrossingLocalIndex = __internal::GetLocalEdgeIndex(FaceToEdges, EdgeFace, CurrIntersection.EdgeIndex);
						continue;
					}

					// Check 3b)
					if (__internal::FindNextIntersectionFace(IntersectionMap, FaceToEdges[CurrIntersection.FaceIndex], EdgeFace, NextIntersection, LocalEdgeIndex))
					{
						// We found the second crossing for FaceSection. Mark it.
						int32& SecondCrossingEdgeLocalIndex = bReverseDirection ? FaceSection->CrossingEdgeLocalIndex[0] : FaceSection->CrossingEdgeLocalIndex[1];
						check(SecondCrossingEdgeLocalIndex == INDEX_NONE);
						SecondCrossingEdgeLocalIndex = LocalEdgeIndex;

						// Add this intersection to the list of intersections consumed by this contour
						const bool bFlipGradient = EdgeContour == &ContourPair.ColorContours[InitialEdgeContourIndex]; // Flip gradient if the edge of this intersection belongs to the contour that started with the face of the first intersection
						ContourPair.Intersections.Add_GetRef(NextIntersection).bFlipGradient = bFlipGradient;

						// Add new FaceEdge section by choosing the face edge that doesn't correspond with our current faceEdge (if it exists)
						const int32 NextEdgeFace = EdgeToFaces[NextIntersection.EdgeIndex][0] == CurrIntersection.FaceIndex ? EdgeToFaces[NextIntersection.EdgeIndex][1] : EdgeToFaces[NextIntersection.EdgeIndex][0];
						if (NextEdgeFace == -1)
						{
							// We've hit a boundary.
							++ContourPair.BoundaryEdgeCount;

							if (ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 2)
							{
								// We've hit both ends of this contour. Since (at least) one end is a boundary, we won't be able to form a closed contour
								break;
							}

							check(ContourPair.LoopVertexCount + ContourPair.BoundaryEdgeCount == 1);

							// Pick up at the FirstIntersection and move the opposite direction if possible.
							EdgeFace = EdgeToFaces[FirstIntersection.EdgeIndex][1];
							if (EdgeFace == -1)
							{
								// First intersection edge was a boundary, so this contour is done. We're NOT closed.
								++ContourPair.BoundaryEdgeCount;
								break;
							}

							check(bReverseDirection == false);
							bReverseDirection = true;
							CurrIntersection = FirstIntersection;
							FaceContour = &ContourPair.ColorContours[0];
							EdgeContour = &ContourPair.ColorContours[1];

							// ContourPoints are currently just used for debug drawing. Start new contours for reverse section
							ContourPair.ContourPointCurves[InitialFaceContourIndex].Add(ContourPoints.Num());
							FaceContourPoints = &ContourPoints.AddDefaulted_GetRef();
							ContourPair.ContourPointCurves[InitialEdgeContourIndex].Add(ContourPoints.Num());
							EdgeContourPoints = &ContourPoints.AddDefaulted_GetRef();

							FaceSection = &ContourPair.ColorContours[InitialFaceContourIndex][0];
							EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(EdgeFace));
							EdgeFaceSection->CrossingEdgeLocalIndex[1] = __internal::GetLocalEdgeIndex(FaceToEdges, EdgeFace, CurrIntersection.EdgeIndex);

							continue;
						}

						// Setup for next iteration of loop
						CurrIntersection = NextIntersection;

						// FaceContour and EdgeContour swap
						FIntersectionContour* TmpContourSwap = EdgeContour;
						EdgeContour = FaceContour;
						FaceContour = TmpContourSwap;
						TArray<FPBDTriangleMeshCollisions::FBarycentricPoint>* TmpPointsSwap = EdgeContourPoints;
						EdgeContourPoints = FaceContourPoints;
						FaceContourPoints = TmpPointsSwap;

						FaceSection = EdgeFaceSection;

						EdgeFace = NextEdgeFace;
						EdgeFaceSection = &EdgeContour->Add_GetRef(FIntersectionContourTriangleSection(EdgeFace));
						int32& FirstCrossingLocalIndex = bReverseDirection ? EdgeFaceSection->CrossingEdgeLocalIndex[1] : EdgeFaceSection->CrossingEdgeLocalIndex[0];
						FirstCrossingLocalIndex = __internal::GetLocalEdgeIndex(FaceToEdges, EdgeFace, CurrIntersection.EdgeIndex);
						continue;
					}

					// We failed to find the next intersection in this contour. 
					break;
				}
			}
			return Contours;
		}
	} // namespace ContourBuilding

	// methods for Flood-filling contours built by ContourBuilding
	namespace FloodFill
	{
		enum struct EFloodFillRegion : int8
		{
			Untouched = 0,
			Loop,
			Region0,
			Region1,
		};
		static constexpr EFloodFillRegion FloodFillRegions[2] = { EFloodFillRegion::Region0, EFloodFillRegion::Region1 };

		// internal helper methods
		namespace __internal
		{
			static bool SimplifyContourSegments(const FTriangleMesh& TriangleMesh, const TMultiMap<int32 /*TriangleIndex*/, const FIntersectionContourTriangleSection*>& ContourSegments, TMap<int32, FIntersectionContourTriangleSection>& SimplifiedContourSegments, TArray<int32>& TrianglesWithThinFeatures,
				TArray<int32>& CandidateSeedTriangles, TVec2<int32>& LoopVertices)
			{
				// For purposes of vertex-based flood fill, simplify ContourSegments so there is at most one "segment" per triangle.
				// Collapse edge crossings such that two crossing cancel out (i.e., even number = no crossing, odd number = crossing).
				// Keep a list of triangles where there are only even crossing counts--these have thin intersection features and 
				// are handled with the Triangle-based GIAColors (a black vertex is attracted to a white triangle, but the vertices of
				// the white triangle are not attracted to black triangles).
				// This collapsing can effectively generate multiple closed contours (that were originally connected by thin regions).
				// We will need to flood fill them separately. Track the Segments that were formed by pinching off thin regions. These
				// are good seeds for flood filling to ensure all of the separate contours are filled.		
				int32 ContourLoopVertexCount = 0;
				SimplifiedContourSegments.Reset();
				CandidateSeedTriangles.Reset();
				TrianglesWithThinFeatures.Reset();
				LoopVertices = { INDEX_NONE, INDEX_NONE };

				const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();

				TArray<int32> UniqueTriangles;
				ContourSegments.GetKeys(UniqueTriangles);

				SimplifiedContourSegments.Reserve(UniqueTriangles.Num());

				for (const int32 TriangleIndex : UniqueTriangles)
				{
					TVec3<int32> CrossingCounts(0);
					TVec3<int32> LoopVertexCounts(0);
					TMultiMap<int32 /*TriangleIndex*/, const FIntersectionContourTriangleSection*>::TConstKeyIterator TriangleIter = ContourSegments.CreateConstKeyIterator(TriangleIndex);
					for (; TriangleIter; ++TriangleIter)
					{
						if (TriangleIter.Value()->LoopVertexLocalIndex != INDEX_NONE)
						{
							++(LoopVertexCounts[TriangleIter.Value()->LoopVertexLocalIndex]);
						}
						if (TriangleIter.Value()->CrossingEdgeLocalIndex[0] != INDEX_NONE)
						{
							++(CrossingCounts[TriangleIter.Value()->CrossingEdgeLocalIndex[0]]);
						}
						if (TriangleIter.Value()->CrossingEdgeLocalIndex[1] != INDEX_NONE)
						{
							++(CrossingCounts[TriangleIter.Value()->CrossingEdgeLocalIndex[1]]);
						}
					}

					const TVec3<int32> SimplifiedCrossingCounts = { CrossingCounts[0] & 1, CrossingCounts[1] & 1, CrossingCounts[2] & 1 };
					const int32 NonZeroCrossingEdgeCount = SimplifiedCrossingCounts[0] + SimplifiedCrossingCounts[1] + SimplifiedCrossingCounts[2];
					if (LoopVertexCounts.Max() > 1)
					{
						// This can occur in pathological loop cases where we have lots of doubling back. These contours aren't actually closed.
						return false;
					}
					const int32 NonZeroLoopVertexCount = LoopVertexCounts[0] + LoopVertexCounts[1] + LoopVertexCounts[2];
					if ((NonZeroLoopVertexCount + NonZeroCrossingEdgeCount) % 2 != 0)
					{
						// Similarly, this can occur with slivers doubling back. These contours aren't actually closed.
						return false;
					}

					if (NonZeroCrossingEdgeCount + NonZeroLoopVertexCount == 0)
					{
						TrianglesWithThinFeatures.Add(TriangleIndex);
						continue;
					}

					if (NonZeroLoopVertexCount > 0)
					{
						// Triangle with loop vertex
						FIntersectionContourTriangleSection SimplifiedSegment(TriangleIndex);
						for (int32 LocalVertexIdx = 0; LocalVertexIdx < 3; ++LocalVertexIdx)
						{
							if (LoopVertexCounts[LocalVertexIdx])
							{
								const int32 LoopVertexGlobalIdx = Elements[TriangleIndex][LocalVertexIdx];
								if (LoopVertices[0] != LoopVertexGlobalIdx && LoopVertices[1] != LoopVertexGlobalIdx)
								{
									check(ContourLoopVertexCount < 2);
									LoopVertices[ContourLoopVertexCount++] = LoopVertexGlobalIdx;
								}
								SimplifiedSegment.LoopVertexLocalIndex = LocalVertexIdx; // It doesn't matter in the Simplified context if there's more than one loop vertex and we only record one because we just care if a loop vertex exists or not.
							}
						}
						SimplifiedContourSegments.Add(TriangleIndex, MoveTemp(SimplifiedSegment));
					}
					else
					{
						// Crossings
						FIntersectionContourTriangleSection SimplifiedSegment(TriangleIndex);
						check(NonZeroCrossingEdgeCount <= 2);
						int32 CrossingEdgeIdx = 0;
						for (int32 LocalEdgeIdx = 0; LocalEdgeIdx < 3; ++LocalEdgeIdx)
						{
							if (SimplifiedCrossingCounts[LocalEdgeIdx])
							{
								SimplifiedSegment.CrossingEdgeLocalIndex[CrossingEdgeIdx++] = LocalEdgeIdx;
							}
						}
						SimplifiedContourSegments.Add(TriangleIndex, MoveTemp(SimplifiedSegment));
						if (CrossingCounts[0] && CrossingCounts[1] && CrossingCounts[2])
						{
							// This used to have crossings on 3 sides, and one was pinched off. This is a candidate seed triangle
							CandidateSeedTriangles.Add(TriangleIndex);
						}
					}
				}
				return true;
			}

			// Returns success
			static bool OneFloodFillStep(const TMap<int32 /*TriangleIndex*/, FIntersectionContourTriangleSection>& ContourSegments, const TArray<TVec3<int32>>& Elements, const TConstArrayView<TArray<int32>>& PointToTriangleMap, TArray<int32>& Queue, int32& RegionSize, TArrayView<EFloodFillRegion>& VertexRegions,
				const EFloodFillRegion RegionColor, const EFloodFillRegion OtherRegionColor)
			{
				const int32 CurrVertex = Queue.Pop(false);
				for (int32 NeighborTri : PointToTriangleMap[CurrVertex])
				{
					if (const FIntersectionContourTriangleSection* TriSection = ContourSegments.Find(NeighborTri))
					{
						const int32 CurrVertexLocalIndex = CurrVertex == Elements[NeighborTri][0] ? 0 : CurrVertex == Elements[NeighborTri][1] ? 1 : CurrVertex == Elements[NeighborTri][2] ? 2 : INDEX_NONE;
						check(CurrVertexLocalIndex != INDEX_NONE);

						if (TriSection->LoopVertexLocalIndex != INDEX_NONE)
						{
							// Triangles with loop vertices don't connect other vertices within the triangle
							// We will mark loop vertices elsewhere
							continue;
						}

						check(TriSection->CrossingEdgeLocalIndex[0] != INDEX_NONE && TriSection->CrossingEdgeLocalIndex[1] != INDEX_NONE);
						check(TriSection->CrossingEdgeLocalIndex[0] != TriSection->CrossingEdgeLocalIndex[1]);

						// Order such that SecondCrossEdge = (FirstCrossEdge + 1)%3
						const bool bSwapEdgeOrder = TriSection->CrossingEdgeLocalIndex[1] != (TriSection->CrossingEdgeLocalIndex[0] + 1) % 3;
						const int32 FirstCrossEdge = bSwapEdgeOrder ? TriSection->CrossingEdgeLocalIndex[1] : TriSection->CrossingEdgeLocalIndex[0];
						// const int32 SecondCrossEdge = bSwapEdgeOrder ? TriSection->CrossingEdgeLocalIndex[0] : TriSection->CrossingEdgeLocalIndex[1];

						if (FirstCrossEdge == CurrVertexLocalIndex)
						{
							// CurrVertex is on the same side as CurrVertex - 1
							const int32 TriVertex = Elements[NeighborTri][(CurrVertexLocalIndex + 2) % 3];
							if (VertexRegions[TriVertex] == EFloodFillRegion::Untouched)
							{
								VertexRegions[TriVertex] = RegionColor;
								++RegionSize;
								Queue.Add(TriVertex);
							}
							else if (VertexRegions[TriVertex] == OtherRegionColor)
							{
								// We don't have a closed contour after all.
								return false;
							}
						}
						else if (FirstCrossEdge == (CurrVertexLocalIndex + 1) % 3)
						{
							// CurrVertex is on the same side as CurrVertex + 1
							const int32 TriVertex = Elements[NeighborTri][(CurrVertexLocalIndex + 1) % 3];
							if (VertexRegions[TriVertex] == EFloodFillRegion::Untouched)
							{
								VertexRegions[TriVertex] = RegionColor;
								++RegionSize;
								Queue.Add(TriVertex);
							}
							else if (VertexRegions[TriVertex] == OtherRegionColor)
							{
								// We don't have a closed contour after all.
								return false;
							}
						}
						// else CurrVertex is not on the same side with any other vertices in this triangle
					}
					else
					{
						// Contour doesn't pass through this tri. Just add all (untouched) points to this Region
						for (int32 VertexLocalIndex = 0; VertexLocalIndex < 3; ++VertexLocalIndex)
						{
							const int32 TriVertex = Elements[NeighborTri][VertexLocalIndex];
							if (VertexRegions[TriVertex] == EFloodFillRegion::Untouched)
							{
								VertexRegions[TriVertex] = RegionColor;
								++RegionSize;
								Queue.Add(TriVertex);
							}
							else if (VertexRegions[TriVertex] == OtherRegionColor)
							{
								// We don't have a closed contour after all.
								return false;
							}
						}
					}
				}
				return true;
			}

			// Flood fill a single contour
			static bool FloodFillContourColor(const FTriangleMesh& TriangleMesh, const int32 NumParticles, const int32 Offset, const TMultiMap<int32 /*TriangleIndex*/, const FIntersectionContourTriangleSection*>& ContourSegments, int32 ContourIndex, bool bIsColorB,
				TArray<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors)
			{
				// Simplify Contour 
				TMap<int32, FIntersectionContourTriangleSection> SimplifiedContourSegments;
				TArray<int32> TrianglesWithThinFeatures;
				TArray<int32> CandidateSeedTriangles;
				TVec2<int32> LoopVertices;
				if (!SimplifyContourSegments(TriangleMesh, ContourSegments, SimplifiedContourSegments, TrianglesWithThinFeatures, CandidateSeedTriangles, LoopVertices))
				{
					return false;
				}

				const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
				const TConstArrayView<TArray<int32>>& PointToTrianglemMap = TriangleMesh.GetPointToTriangleMap();

				// Main contour flood fill
				if (SimplifiedContourSegments.Num())
				{
					// This collapsing can effectively generate multiple closed contours (that were originally connected by thin regions).
					// We will need to flood fill them separately.
					if (CandidateSeedTriangles.Num() == 0)
					{
						// If there weren't any candidates found by pinching regions, just start with first non-loop segment
						TMap<int32, FIntersectionContourTriangleSection>::TConstIterator Iter = SimplifiedContourSegments.CreateConstIterator();
						for (; Iter; ++Iter)
						{
							const FIntersectionContourTriangleSection& FirstSection = Iter.Value();
							if (FirstSection.LoopVertexLocalIndex != INDEX_NONE)
							{
								continue;
							}

							const int32 FirstVertex0 = FirstSection.CrossingEdgeLocalIndex[0];
							const int32 FirstVertex1 = (FirstSection.CrossingEdgeLocalIndex[0] + 1) % 3;
							if (Elements[Iter.Key()][FirstVertex0] == LoopVertices[0] || Elements[Iter.Key()][FirstVertex0] == LoopVertices[1] ||
								Elements[Iter.Key()][FirstVertex1] == LoopVertices[0] || Elements[Iter.Key()][FirstVertex1] == LoopVertices[1])
							{
								continue;
							}
							CandidateSeedTriangles.Add(Iter.Key());
							break;
						}
					}

					for (const int32 SeedTriangle : CandidateSeedTriangles)
					{
						// Initialize queue for flood fill with first Section in Contour
						const FIntersectionContourTriangleSection& FirstSection = SimplifiedContourSegments.FindChecked(SeedTriangle);
						check(FirstSection.CrossingEdgeLocalIndex[0] != INDEX_NONE);
						const int32 FirstVertex0 = FirstSection.CrossingEdgeLocalIndex[0];
						const int32 FirstVertex1 = (FirstSection.CrossingEdgeLocalIndex[0] + 1) % 3;

						// Check if this Triangle already has been colored on either side.
						if (VertexGIAColors[Elements[FirstSection.TriangleIndex][FirstVertex0] - Offset].HasContourColorSet(ContourIndex)
							|| VertexGIAColors[Elements[FirstSection.TriangleIndex][FirstVertex1] - Offset].HasContourColorSet(ContourIndex))
						{
							continue;
						}

						// Flood fill both sides of contour. Stop when one side is complete -- we want the smaller region.
						// For now, assume triangle areas are similar, and just calculate "smaller" based on number of vertices

						TArray<int32 /*VertexIndex*/> Queue[2];
						int32 RegionSizes[2] = { 0, 0 };
						TArray<EFloodFillRegion> VertexRegionsNoOffset;
						static_assert((int8)EFloodFillRegion::Untouched == 0);
						VertexRegionsNoOffset.SetNumZeroed(NumParticles);

						TArrayView<EFloodFillRegion> VertexRegions(VertexRegionsNoOffset.GetData() - Offset, Offset + NumParticles);

						Queue[0].Add(Elements[FirstSection.TriangleIndex][FirstVertex0]);
						Queue[1].Add(Elements[FirstSection.TriangleIndex][FirstVertex1]);

						VertexRegions[Elements[FirstSection.TriangleIndex][FirstVertex0]] = FloodFillRegions[0];
						VertexRegions[Elements[FirstSection.TriangleIndex][FirstVertex1]] = FloodFillRegions[1];
						++RegionSizes[0];
						++RegionSizes[1];

						// Set Loop vertices so we don't cross them when flooding
						if (LoopVertices[0] != INDEX_NONE)
						{
							VertexRegions[LoopVertices[0]] = EFloodFillRegion::Loop;
						}
						if (LoopVertices[1] != INDEX_NONE)
						{
							VertexRegions[LoopVertices[1]] = EFloodFillRegion::Loop;
						}

						while (Queue[0].Num() > 0 && Queue[1].Num() > 0)
						{
							for (int32 RegionIndex = 0; RegionIndex < 2; ++RegionIndex)
							{
								if (!OneFloodFillStep(SimplifiedContourSegments, Elements, PointToTrianglemMap, Queue[RegionIndex], RegionSizes[RegionIndex], VertexRegions, FloodFillRegions[RegionIndex], FloodFillRegions[RegionIndex ^ 1]))
								{
									return false;
								}
							}
						}

						const int32 CompletedQueueIndex = Queue[0].Num() == 0 ? 0 : 1;
						if (RegionSizes[CompletedQueueIndex] >= RegionSizes[CompletedQueueIndex ^ 1])
						{
							// Run FloodFill on incomplete queue until complete/larger than completed queue
							while (Queue[CompletedQueueIndex ^ 1].Num() > 0 && (RegionSizes[CompletedQueueIndex] >= RegionSizes[CompletedQueueIndex ^ 1]))
							{
								if (!OneFloodFillStep(SimplifiedContourSegments, Elements, PointToTrianglemMap, Queue[CompletedQueueIndex ^ 1], RegionSizes[CompletedQueueIndex ^ 1], VertexRegions, FloodFillRegions[CompletedQueueIndex ^ 1], FloodFillRegions[CompletedQueueIndex]))
								{
									return false;
								}
							}
						}

						if (RegionSizes[0] == RegionSizes[1])
						{
							// Both regions are the same size. Can't determine inside.
							return false;
						}

						// Mark VertexGIAColors for smaller region
						const EFloodFillRegion SmallerRegionColor = RegionSizes[0] < RegionSizes[1] ? FloodFillRegions[0] : FloodFillRegions[1];
						for (int ParticleIndexNoOffset = 0; ParticleIndexNoOffset < NumParticles; ++ParticleIndexNoOffset)
						{
							if (VertexRegionsNoOffset[ParticleIndexNoOffset] == SmallerRegionColor)
							{
								VertexGIAColors[ParticleIndexNoOffset].SetContourColor(ContourIndex, bIsColorB);
							}
						}
					}
				}

				// Loop vertex handling
				if (ContourIndex == FPBDTriangleMeshCollisions::FGIAColor::LoopContourIndex)
				{
					if (LoopVertices[0] != INDEX_NONE)
					{
						VertexGIAColors[LoopVertices[0] - Offset].SetContourColor(ContourIndex, bIsColorB);
					}
					if (LoopVertices[1] != INDEX_NONE)
					{
						VertexGIAColors[LoopVertices[1] - Offset].SetContourColor(ContourIndex, bIsColorB);
					}
				}

				// Thin region triangles
				for (int32 TriangleIndex : TrianglesWithThinFeatures)
				{
					TriangleGIAColors[TriangleIndex].SetContourColor(ContourIndex, bIsColorB);
				}
				return true;
			}
		} // namespace __internal

		// Flood fill contours built by ContourBuilding to generate VertexGIAColors. Identify triangles with thin intersections (TriangleGIAColors)
		static void FloodFillContours(const FTriangleMesh& TriangleMesh, const int32 NumParticles, const int32 Offset, TArray<FIntersectionContourPair>& Contours, TArray<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_FloodFillContours);
			const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
			const TConstArrayView<TArray<int32>>& PointToTrianglemMap = TriangleMesh.GetPointToTriangleMap();

			// If we used a critsect/made writing to the GIAColors threadsafe with atomics, we parallelize by contour. Not sure if it's worth the overhead though.

			int32 CurrentContourIndex = 1; // ContourIndex 0 is reserved for loop 
			for (const FIntersectionContourPair& Contour : Contours)
			{
				if (!Contour.bIsClosed)
				{
					continue;
				}

				TMultiMap<int32 /*TriangleIndex*/, const FIntersectionContourTriangleSection*> ContourSegments;
				const bool bCombineContours = Contour.LoopVertexCount > 0; // Loop contours were built still as a contour pair, but really represent a single contour doubling back on itself.
				const int32 ContourSegmentReserveNum = bCombineContours ? Contour.ColorContours[0].Num() + Contour.ColorContours[1].Num() : FMath::Max(Contour.ColorContours[0].Num(), Contour.ColorContours[1].Num());
				ContourSegments.Reserve(ContourSegmentReserveNum);

				for (const FIntersectionContourTriangleSection& Section : Contour.ColorContours[0])
				{
					ContourSegments.Emplace(Section.TriangleIndex, &Section);
				}

				if (bCombineContours)
				{
					for (const FIntersectionContourTriangleSection& Section : Contour.ColorContours[1])
					{
						ContourSegments.Emplace(Section.TriangleIndex, &Section);
					}
				}

				const int32 ContourIndex = bCombineContours ? FPBDTriangleMeshCollisions::FGIAColor::LoopContourIndex : (((CurrentContourIndex++) - 1) % 31) + 1;

				if (!__internal::FloodFillContourColor(TriangleMesh, NumParticles, Offset, ContourSegments, ContourIndex, bCombineContours, VertexGIAColors, TriangleGIAColors))
				{
					const_cast<FIntersectionContourPair&>(Contour).bIsClosed = false;
					continue;
				}

				if (!bCombineContours)
				{
					ContourSegments.Reset();

					for (const FIntersectionContourTriangleSection& Section : Contour.ColorContours[1])
					{
						ContourSegments.Emplace(Section.TriangleIndex, &Section);
					}

					if (!__internal::FloodFillContourColor(TriangleMesh, NumParticles, Offset, ContourSegments, ContourIndex, true, VertexGIAColors, TriangleGIAColors))
					{
						const_cast<FIntersectionContourPair&>(Contour).bIsClosed = false;
						continue;
					}
				}
			}
		}

		// Once FloodFill has completed, we can mark the IntersectionContourTypes for debug drawing ContourPoints.
		static void AssignContourPointTypes(const TArray<FIntersectionContourPair>& Contours, const TArray<TArray<FPBDTriangleMeshCollisions::FBarycentricPoint>>& ContourPoints, TArray<FPBDTriangleMeshCollisions::FContourType>& IntersectionContourTypes)
		{
			IntersectionContourTypes.Reset();
			IntersectionContourTypes.SetNumZeroed(ContourPoints.Num());
			for (const FIntersectionContourPair& ContourPair : Contours)
			{
				if (ContourPair.bIsClosed)
				{
					if (ContourPair.LoopVertexCount > 0)
					{
						for (const int32 PointIndex : ContourPair.ContourPointCurves[0])
						{
							IntersectionContourTypes[PointIndex] = FPBDTriangleMeshCollisions::FContourType::Loop;
						}
						for (const int32 PointIndex : ContourPair.ContourPointCurves[1])
						{
							IntersectionContourTypes[PointIndex] = FPBDTriangleMeshCollisions::FContourType::Loop;
						}
					}
					else
					{
						for (const int32 PointIndex : ContourPair.ContourPointCurves[0])
						{
							IntersectionContourTypes[PointIndex] = FPBDTriangleMeshCollisions::FContourType::Contour0;
						}
						for (const int32 PointIndex : ContourPair.ContourPointCurves[1])
						{
							IntersectionContourTypes[PointIndex] = FPBDTriangleMeshCollisions::FContourType::Contour1;
						}
					}
				}
			}
		}
	} // namespace FloodFill
} // namespace GIA

// Calculating gradient direction to minimize contour length.
namespace ContourMinimization
{
	static void BuildLocalContourMinimizationIntersection(const FEdgeFaceIntersection& EdgeFaceIntersection, const FTriangleMesh& TriangleMesh, const FSegmentMesh& SegmentMesh, const FSolverParticles& Particles, FPBDTriangleMeshCollisions::FContourMinimizationIntersection& ContourIntersection)
	{
		ContourIntersection.EdgeVertices = SegmentMesh.GetElements()[EdgeFaceIntersection.EdgeIndex];
		ContourIntersection.FaceVertices = TriangleMesh.GetElements()[EdgeFaceIntersection.FaceIndex];
		ContourIntersection.LocalGradientVector = FSolverVec3(0.f);

		// Calculate local gradient
		const TArray<TVec2<int32>>& EdgeToFaces = TriangleMesh.GetEdgeToFaces();
		const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();

		const FSolverVec3& EdgePosition0 = Particles.X(ContourIntersection.EdgeVertices[0]);
		const FSolverVec3& EdgePosition1 = Particles.X(ContourIntersection.EdgeVertices[1]);
		const FSolverVec3 EdgeDir = EdgePosition1 - EdgePosition0; // Unnormalized
		const FSolverReal EdgeDirDotNormal = FSolverVec3::DotProduct(EdgeDir, EdgeFaceIntersection.FaceNormal);
		if (FMath::Abs(EdgeDirDotNormal) >= UE_SMALL_NUMBER)
		{
			const FSolverVec3 NOverEDotN = EdgeFaceIntersection.FaceNormal * ((FSolverReal)1.f / EdgeDirDotNormal);

			for (int32 LocalFaceIndex = 0; LocalFaceIndex < 2; ++LocalFaceIndex)
			{
				const int32 EdgeFace_i = EdgeToFaces[EdgeFaceIntersection.EdgeIndex][LocalFaceIndex];
				if (EdgeFace_i == -1)
				{
					continue;
				}

				const FSolverVec3& P0 = Particles.X(Elements[EdgeFace_i][0]);
				const FSolverVec3& P1 = Particles.X(Elements[EdgeFace_i][1]);
				const FSolverVec3& P2 = Particles.X(Elements[EdgeFace_i][2]);

				// B_i
				const TTriangle<FSolverReal> Triangle_i(P0, P1, P2);

				// N_i
				const FSolverVec3 Normal_i = Triangle_i.GetNormal();

				// R_i
				FSolverVec3 IntersectionVector_i = FSolverVec3::CrossProduct(EdgeFaceIntersection.FaceNormal, Normal_i);
				IntersectionVector_i.Normalize();
				// This should point inward from the Edge. Flip if necessary.

				// SideDirection = Normal_i ^ (EdgeDir) // NOTE: Need to calculate EdgeDir based on vertex order within the triangle, not what's in the SegmentMesh to ensure SideDirection points INTO from triangle.
				// If Dot(IntersectionVector_i, SideDirection) < 0, need to flip IntersectionVector_i
				// Dot(IntersectionVector_i, SideDirection) = Dot((CollisionPoint.Normal ^ Normal_i), (Normal_i ^ EdgeDir))
				// Using triple product rules, this is
				// Dot(CollisionPoint.Normal, Normal_i) * Dot(Normal_i, EdgeDir) - Dot(CollisionPoint.Normal, EdgeDir) * Dot(Normal_i, Normal_i)
				const int32 EdgePointLocalIndex0 = ContourIntersection.EdgeVertices[0] == Elements[EdgeFace_i][0] ? 0 : ContourIntersection.EdgeVertices[0] == Elements[EdgeFace_i][1] ? 1 : ContourIntersection.EdgeVertices[0] == Elements[EdgeFace_i][2] ? 2 : INDEX_NONE;
				check(EdgePointLocalIndex0 != INDEX_NONE); // Otherwise the EdgeToFaces is messed up because this edge doesn't below to this face
				check(ContourIntersection.EdgeVertices[1] == Elements[EdgeFace_i][(EdgePointLocalIndex0 + 1) % 3] || ContourIntersection.EdgeVertices[1] == Elements[EdgeFace_i][(EdgePointLocalIndex0 + 2) % 3]);
				const bool bEdgePointsFlipped = ContourIntersection.EdgeVertices[1] != Elements[EdgeFace_i][(EdgePointLocalIndex0 + 1) % 3];
				const FSolverReal TestFlipRValue = (FSolverVec3::DotProduct(EdgeFaceIntersection.FaceNormal, Normal_i) * FSolverVec3::DotProduct(Normal_i, EdgeDir) - FSolverVec3::DotProduct(EdgeFaceIntersection.FaceNormal, EdgeDir)) * (bEdgePointsFlipped ? (FSolverReal)-1.f : (FSolverReal)1.f);
				if (TestFlipRValue < 0)
				{
					IntersectionVector_i *= (FSolverReal)-1.f;
				}

				ContourIntersection.LocalGradientVector += IntersectionVector_i - (FSolverReal)2.f * FSolverVec3::DotProduct(EdgeDir, IntersectionVector_i) * NOverEDotN;
			}
		}

		ContourIntersection.GlobalGradientVector = ContourIntersection.LocalGradientVector;
	}

	static void BuildLocalContourMinimizationIntersections(const FTriangleMesh& TriangleMesh, const FSolverParticles& Particles, const TArray<FEdgeFaceIntersection>& Intersections, TArray<FPBDTriangleMeshCollisions::FContourMinimizationIntersection>& ContourMinimizationIntersections)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_BuildLocalContourMinimizationIntersections);
		const FSegmentMesh& SegmentMesh = TriangleMesh.GetSegmentMesh();

		ContourMinimizationIntersections.SetNum(Intersections.Num());
		PhysicsParallelFor(Intersections.Num(),
			[&TriangleMesh, &SegmentMesh, &Particles, &Intersections, &ContourMinimizationIntersections](int32 IntersectionIndex)
			{
				const FEdgeFaceIntersection& EdgeFaceIntersection = Intersections[IntersectionIndex];
				FPBDTriangleMeshCollisions::FContourMinimizationIntersection& ContourIntersection = ContourMinimizationIntersections[IntersectionIndex];
				BuildLocalContourMinimizationIntersection(EdgeFaceIntersection, TriangleMesh, SegmentMesh, Particles, ContourIntersection);
			}
		);
	}

	static void BuildGlobalContourMinimizationIntersections(const FTriangleMesh& TriangleMesh, const FSolverParticles& Particles, const TArray<GIA::FIntersectionContourPair>& IntersectionContours, TArray<FPBDTriangleMeshCollisions::FContourMinimizationIntersection>& ContourMinimizationIntersections)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_BuildGlobalContourMinimizationIntersections);
		const FSegmentMesh& SegmentMesh = TriangleMesh.GetSegmentMesh();
		// Only do contour minimization for non-closed contours. Closed contours will be handled via GIA

		// Determine total number of non-closed contours and number of segments
		struct FContourRange
		{
			int32 ContourPairIndex;
			int32 GlobalIndexStart;
		};

		TArray<FContourRange> ContourRanges;
		ContourRanges.Reset(IntersectionContours.Num());
		int32 IndexStart = 0;
		for (int32 ContourPairIndex = 0; ContourPairIndex < IntersectionContours.Num(); ++ContourPairIndex)
		{
			if (IntersectionContours[ContourPairIndex].bIsClosed && IntersectionContours[ContourPairIndex].LoopVertexCount == 0)
			{
				continue;
			}
			ContourRanges.Add({ ContourPairIndex, IndexStart });
			IndexStart += IntersectionContours[ContourPairIndex].Intersections.Num();
		}

		if (ContourRanges.Num() == 0)
		{
			return;
		}

		ContourMinimizationIntersections.SetNum(IndexStart);

		// For now, parallelize by contour. Might want to balance by contour length?
		PhysicsParallelFor(ContourRanges.Num(),
			[&TriangleMesh, &SegmentMesh, &Particles, &ContourRanges, &IntersectionContours, &ContourMinimizationIntersections](int32 ContourRangeIndex)
			{ 
				const TArray<FEdgeFaceIntersection>& Intersections = IntersectionContours[ContourRanges[ContourRangeIndex].ContourPairIndex].Intersections;
				const int32 GlobalIndexStart = ContourRanges[ContourRangeIndex].GlobalIndexStart;
				FSolverVec3 GlobalGradient(0.f);
				for (int32 SegmentIndex = 0; SegmentIndex < Intersections.Num(); ++SegmentIndex)
				{
					const FEdgeFaceIntersection& EdgeFaceIntersection = Intersections[SegmentIndex];
					FPBDTriangleMeshCollisions::FContourMinimizationIntersection& ContourIntersection = ContourMinimizationIntersections[GlobalIndexStart + SegmentIndex];
					BuildLocalContourMinimizationIntersection(EdgeFaceIntersection, TriangleMesh, SegmentMesh, Particles, ContourIntersection);
					GlobalGradient += ContourIntersection.LocalGradientVector * (EdgeFaceIntersection.bFlipGradient ? -1.f : 1.f);
				}

				for (int32 SegmentIndex = 0; SegmentIndex < Intersections.Num(); ++SegmentIndex)
				{
					const FEdgeFaceIntersection& EdgeFaceIntersection = Intersections[SegmentIndex];
					FPBDTriangleMeshCollisions::FContourMinimizationIntersection& ContourIntersection = ContourMinimizationIntersections[GlobalIndexStart + SegmentIndex];
					ContourIntersection.GlobalGradientVector = GlobalGradient * (EdgeFaceIntersection.bFlipGradient ? -1.f : 1.f);
				}
			}
		);		
	}
} // namespace ContourMinimization

void FPBDTriangleMeshCollisions::Init(const FSolverParticles& Particles)
{
	if (TriangleMesh.GetNumElements() == 0)
	{
		return;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosFPBDTriangleMeshCollisions_BuildSpatialHash);
		TriangleMesh.BuildSpatialHash(static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), SpatialHash);
	}
	ContourMinimizationIntersections.Reset();
	VertexGIAColors.Reset();
	VertexGIAColors.SetNumZeroed(NumParticles);
	TriangleGIAColors.Reset();
	TriangleGIAColors.SetNumZeroed(TriangleMesh.GetNumElements());
	IntersectionContourPoints.Reset();
	IntersectionContourTypes.Reset();

	if (!bGlobalIntersectionAnalysis && !bContourMinimization)
	{
		return;
	}

	// Detect all EdgeFace Intersections
	TArray<FEdgeFaceIntersection> Intersections = FindEdgeFaceIntersections(TriangleMesh, SpatialHash, Particles);
	if (Intersections.Num() == 0)
	{
		return;
	}

	TArray<GIA::FIntersectionContourPair> IntersectionContours;
	if (bGlobalIntersectionAnalysis)
	{ 
		// Walk EdgeFace intersections to build global contours
		IntersectionContours = GIA::ContourBuilding::BuildIntersectionContours(TriangleMesh, Intersections, IntersectionContourPoints);
		// Flood fill global contours to determine intersecting regions.
		GIA::FloodFill::FloodFillContours(TriangleMesh, NumParticles, Offset, IntersectionContours, VertexGIAColors, TriangleGIAColors);
		GIA::FloodFill::AssignContourPointTypes(IntersectionContours, IntersectionContourPoints, IntersectionContourTypes);
	}

	if (bContourMinimization)
	{
		if (bGlobalIntersectionAnalysis)
		{
			// Global contours which are non-closed or loop are handled via ContourMinimization impulses. Build global gradient (by adding contribution across all intersections per contour).
			ContourMinimization::BuildGlobalContourMinimizationIntersections(TriangleMesh, Particles, IntersectionContours, ContourMinimizationIntersections);
		}
		else
		{
			// Just build local gradient for all Intersections
			ContourMinimization::BuildLocalContourMinimizationIntersections(TriangleMesh, Particles, Intersections, ContourMinimizationIntersections);
		}
	}
}
}  // End namespace Chaos::Softs
