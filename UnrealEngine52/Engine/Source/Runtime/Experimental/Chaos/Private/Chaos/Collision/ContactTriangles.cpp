// Copyright Epic Games, Inc.All Rights Reserved.
#include "Chaos/Collision/ContactTriangles.h"
#include "Chaos/DebugDrawQueue.h"

namespace Chaos
{
	namespace CVars
	{
		extern int32 ChaosSolverDebugDrawMeshContacts;
	}
	extern bool bChaos_Collision_EnableEdgePrune;


	void FContactTriangleCollector::SetNumContacts(const int32 NumContacts)
	{
		TriangleContactPoints.SetNum(NumContacts, false);
	}

	void FContactTriangleCollector::RemoveContact(const int32 ContactIndex)
	{
		TriangleContactPoints.RemoveAtSwap(ContactIndex, 1, false);
	}

	int32 FContactTriangleCollector::AddTriangle(const FTriangle& Triangle, const int32 TriangleIndex, const int32 VertexIndex0, const int32 VertexIndex1, const int32 VertexIndex2)
	{
		const int32 ContactTriangleIndex = ContactTriangles.AddUninitialized();
		FContactTriangle& ContactTriangle = ContactTriangles[ContactTriangleIndex];
		ContactTriangle.Vertices[0] = Triangle.GetVertex(0);
		ContactTriangle.Vertices[1] = Triangle.GetVertex(1);
		ContactTriangle.Vertices[2] = Triangle.GetVertex(2);
		ContactTriangle.FaceNormal = Triangle.GetNormal();
		ContactTriangle.VertexIndices[0] = VertexIndex0;
		ContactTriangle.VertexIndices[1] = VertexIndex1;
		ContactTriangle.VertexIndices[2] = VertexIndex2;
		ContactTriangle.TriangleIndex = TriangleIndex;
		return ContactTriangleIndex;
	}

	// Find the other triangle in the list that shares the specified edge with the specified triangle
	int32 FContactTriangleCollector::GetOtherTriangleIndexSharingEdge(const int32 KnownTriangleIndex, const int32 EdgeVertexIndexA, const int32 EdgeVertexIndexB) const
	{
		check(EdgeVertexIndexA != INDEX_NONE);
		check(EdgeVertexIndexA != EdgeVertexIndexB);

		for (int32 OtherTriangleIndex = 0; OtherTriangleIndex < ContactTriangles.Num(); ++OtherTriangleIndex)
		{
			if (OtherTriangleIndex != KnownTriangleIndex)
			{
				const FContactTriangle& OtherTriangle = ContactTriangles[OtherTriangleIndex];
				if (OtherTriangle.HasVertexIndex(EdgeVertexIndexA))
				{
					if (EdgeVertexIndexB == INDEX_NONE)
					{
						// @todo(chaos): this is just the first triangle using the vertex, there will be at least 3 faces sharing a vertex and could be more...we should handle that
						return OtherTriangleIndex;
					}

					if (OtherTriangle.HasVertexIndex(EdgeVertexIndexB))
					{
						// This triangle also uses the edge
						return OtherTriangleIndex;
					}
				}
			}
		}

		return INDEX_NONE;
	}

	int32 FContactTriangleCollector::GetNextTriangleIndexSharingVertex(const int32 LastContactTriangleIndex, const int32 VertexIndex) const
	{
		check(VertexIndex != INDEX_NONE);
		for (int32 OtherTriangleIndex = LastContactTriangleIndex + 1; OtherTriangleIndex < ContactTriangles.Num(); ++OtherTriangleIndex)
		{
			const FContactTriangle& OtherTriangle = ContactTriangles[OtherTriangleIndex];
			if (OtherTriangle.HasVertexIndex(VertexIndex))
			{
				return OtherTriangleIndex;
			}
		}

		return INDEX_NONE;
	}

	void FContactTriangleCollector::BuildContactFeatureSets()
	{
		const int32 NumContactPoints = TriangleContactPoints.Num();

		// @todo(chaos): use sorted array and binary search rather than TSet (which won't hash well for edge IDs)
		ContactEdges.Reserve(3 * NumContactPoints);
		ContactVertices.Reserve(3 * NumContactPoints);

		for (int32 ContactIndex = 0; ContactIndex < NumContactPoints; ++ContactIndex)
		{
			FTriangleContactPoint& ContactPoint = TriangleContactPoints[ContactIndex];
			const FContactTriangle& ContactTriangle = ContactTriangles[ContactPoint.ContactTriangleIndex];

			if (ContactPoint.ContactType == EContactPointType::VertexPlane)
			{
				// Since we collided with this face, we cannot collide with these edges or vertices
				ContactEdges.Add(FContactEdgeID(ContactTriangle.VertexIndices[0], ContactTriangle.VertexIndices[1]));
				ContactEdges.Add(FContactEdgeID(ContactTriangle.VertexIndices[1], ContactTriangle.VertexIndices[2]));
				ContactEdges.Add(FContactEdgeID(ContactTriangle.VertexIndices[2], ContactTriangle.VertexIndices[0]));
				ContactVertices.Add(FContactVertexID(ContactTriangle.VertexIndices[0]));
				ContactVertices.Add(FContactVertexID(ContactTriangle.VertexIndices[1]));
				ContactVertices.Add(FContactVertexID(ContactTriangle.VertexIndices[2]));
			}
			else if ((ContactPoint.ContactType == EContactPointType::EdgeEdge) || (ContactPoint.ContactType == EContactPointType::PlaneVertex))
			{
				int32 VertexIndexA, VertexIndexB;
				FVec3 VertexA, VertexB;
				if (ContactTriangle.GetEdgeVerticesAtPosition(ContactPoint.ShapeContactPoints[1], VertexA, VertexB, VertexIndexA, VertexIndexB, FReal(0.01)))
				{
					if ((VertexIndexA != INDEX_NONE) && (VertexIndexB != INDEX_NONE))
					{
						// Since we collided with this edge, we cannot also collide with these verts
						ContactVertices.Add(FContactVertexID(VertexIndexA));
						ContactVertices.Add(FContactVertexID(VertexIndexB));

						// Store the edge ID so we can see if any other faces add it to the don'tcollide list
						ContactPoint.EdgeID = FContactEdgeID(VertexIndexA, VertexIndexB);
					}
					else if (VertexIndexA != INDEX_NONE)
					{
						// Store the vertex ID so we can see if any other faces or edges add it to the don'tcollide list
						ContactPoint.VertexID = FContactVertexID(VertexIndexA);
					}
				}
			}
		}
	}

	void FContactTriangleCollector::ProcessContacts(const FRigidTransform3& MeshToConvexTransform)
	{
		DebugDrawContactPoints(FColor::White, 0.2);

		// Build the feature set from the full list of contacts before pruning
		BuildContactFeatureSets();

		// Remove edge contacts that should not be possible because we are also colliding with a face that used the edge.
		if (TriangleContactPoints.Num() > 1)
		{
			PruneEdgeContactPoints();
		}

		DebugDrawContactPoints(FColor::Cyan, 0.35);

		// Remove contacts that should not be able to occur if we are a one-sided mesh
		// NOTE: does not require sorted contacts, and does not maintain array order
		if (bOneSidedCollision)
		{
			PruneInfacingContactPoints();
		}

		SortContactPointsForPruning();

		// Remove close contacts and those not near the deepest Phi
		// NOTE: relies on the sort above, and retains order
		if (TriangleContactPoints.Num() > 4)
		{
			PruneUnnecessaryContactPoints(FReal(0.1), FReal(0.1));
		}

		DebugDrawContactPoints(FColor::Yellow, 0.5);

		// Reduce to only 4 contact points from here
		// NOTE: relies on the sort above and may not retain order
		if (TriangleContactPoints.Num() > 4)
		{
			ReduceManifoldContactPointsTriangeMesh();
		}

		// Fix contact normals for edge and vertex collisions
		if (TriangleContactPoints.Num() > 1)
		{
			FixInvalidNormalContactPoints();
		}

		// Sort the contacts so that faces are processed before edges, edges before vertices, and then deepest are processed first
		SortContactPointsForSolving();

		DebugDrawContactPoints(FColor::Red, 1.0);

		FinalizeContacts(MeshToConvexTransform);
	}

	void FContactTriangleCollector::SortContactPointsForPruning()
	{
		if (TriangleContactPoints.Num() > 1)
		{
			std::sort(
				&TriangleContactPoints[0],
				&TriangleContactPoints[0] + TriangleContactPoints.Num(),
				[](const FTriangleContactPoint& L, const FTriangleContactPoint& R)
				{
					return L.Phi < R.Phi;
				}
			);
		}
	}

	// Sort contacts on a shape pair in the order we like to solve them.
	// NOTE: This relies on the enum order of EContactPointType.
	void FContactTriangleCollector::SortContactPointsForSolving()
	{
		if (TriangleContactPoints.Num() > 1)
		{
			std::sort(
				&TriangleContactPoints[0],
				&TriangleContactPoints[0] + TriangleContactPoints.Num(),
				[](const FTriangleContactPoint& L, const FTriangleContactPoint& R)
				{
					if (L.ContactType == R.ContactType)
					{
						return L.Phi < R.Phi;
					}
					return L.ContactType < R.ContactType;
				}
			);
		}
	}

	// For each edge contact, find the valid range of normal and reject the contact if the normal is outside the range.
	// This rejects collisions against concave edges, as well as edge contacts that should be hidden by face contacts on adjacent triangles.
	// NOTE: This only works if both triangle sharing the edge are in the ContactPointTriangles list. They usually will be because you won't collide with an edge unless you overlap the adjacent triangle.
	// NOTE: Assumes the ContactPoints array has both contacts in the same space (Tri Mesh and Heightfields are this way until the final step)
	void FContactTriangleCollector::PruneEdgeContactPoints()
	{
		if (!bChaos_Collision_EnableEdgePrune)
		{
			return;
		}

		// Remove all vertex collisions when we have also an edge collision that includes the vertex
		// Remove all edge collisions when we have a face collision that includes the edge
		int32 NumContactPoints = TriangleContactPoints.Num();
		for (int32 ContactIndex = 0; ContactIndex < NumContactPoints; ++ContactIndex)
		{
			const FTriangleContactPoint& ContactPoint = TriangleContactPoints[ContactIndex];

			bool bRemoveContact = false;
			if (ContactPoint.VertexID != INDEX_NONE)
			{
				bRemoveContact = ContactVertices.Contains(ContactPoint.VertexID);
			}
			else if (ContactPoint.EdgeID.IsValid())
			{
				bRemoveContact = ContactEdges.Contains(ContactPoint.EdgeID);
			}

			if (bRemoveContact)
			{
				RemoveContact(ContactIndex);
				--ContactIndex;
				--NumContactPoints;
			}
		}
	}

	// Examine all edge and vertex contacts. If the contact normal is invalid (i.e., it points into the prism formed by 
	// infinitly extending the face along the normal) then fix it to be in the valid range.
	void FContactTriangleCollector::FixInvalidNormalContactPoints()
	{
		const int32 NumContactPoints = TriangleContactPoints.Num();
		for (int32 ContactIndex = 0; ContactIndex < NumContactPoints; ++ContactIndex)
		{
			FTriangleContactPoint& ContactPoint = TriangleContactPoints[ContactIndex];

			// Do we have a collision with the triangle edge or vertex?
			// NOTE: second shape is always the triangle
			const bool bIsTriangleEdgeOrVertex = (ContactPoint.ContactType == EContactPointType::EdgeEdge) || (ContactPoint.ContactType == EContactPointType::PlaneVertex);
			if (bIsTriangleEdgeOrVertex)
			{
				const int32 ContactTriangleIndex = ContactPoint.ContactTriangleIndex;
				const FContactTriangle& ContactTriangle = ContactTriangles[ContactTriangleIndex];

				// Find the vertex positions and indices for this contact
				int32 VertexIndexA, VertexIndexB;
				FVec3 VertexA, VertexB;
				if (ContactTriangle.GetEdgeVerticesAtPosition(ContactPoint.ShapeContactPoints[1], VertexA, VertexB, VertexIndexA, VertexIndexB))
				{
					// If VertexIndexA and VertexIndexB are valid we have an edge collision.
					// If only VertexIndexA is valid (VertexIndexB is INDEX_NONE) we have a vertex collision. 
					// @todo(chaos): Add check for valid normals with vertex collisions. This is much harder than the edge case...
					if ((VertexIndexA != INDEX_NONE) && (VertexIndexB != INDEX_NONE))
					{
						// Get the other triangle that shares this edge
						const int32 OtherContactTriangleIndex = GetOtherTriangleIndexSharingEdge(ContactTriangleIndex, VertexIndexA, VertexIndexB);
						if (OtherContactTriangleIndex != INDEX_NONE)
						{
							const FContactTriangle& OtherContactTriangle = ContactTriangles[OtherContactTriangleIndex];
							const FReal MinContactDotNormal = FVec3::DotProduct(OtherContactTriangle.FaceNormal, ContactTriangle.FaceNormal);
							const FReal ContactDotNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, ContactTriangle.FaceNormal);
							if (ContactDotNormal < MinContactDotNormal)
							{
								// We are outside the valid normal range for this edge
								// Convert the edge collision to a face collision on one of the faces, selected to get the smallest depth
								// NOTE: We keep the contact depth as it is because we know that the depth is a lower-bound. 
								// We have to update the ShapeContactPoint[0] because Phi is actually derived from the positions (the value in ContactPoint is just a cache of current state)
								// @todo(chaos): face selection logic might be better if we knew the contact velocity
								const FReal OtherContactDotNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, ContactTriangle.FaceNormal);
								if (ContactDotNormal >= OtherContactDotNormal)
								{
									ContactPoint.ShapeContactNormal = (ContactDotNormal > -SMALL_NUMBER) ? ContactTriangle.FaceNormal : -ContactTriangle.FaceNormal;
									ContactPoint.ShapeContactPoints[0] = ContactPoint.ShapeContactPoints[1] + ContactPoint.Phi * ContactPoint.ShapeContactNormal;
								}
								else
								{
									ContactPoint.ShapeContactNormal = (OtherContactDotNormal > -SMALL_NUMBER) ? OtherContactTriangle.FaceNormal : -OtherContactTriangle.FaceNormal;
									ContactPoint.ShapeContactPoints[0] = ContactPoint.ShapeContactPoints[1] + ContactPoint.Phi * ContactPoint.ShapeContactNormal;
									ContactPoint.ContactTriangleIndex = OtherContactTriangleIndex;
								}
							}
						}
					}
					else if (VertexIndexA != INDEX_NONE)
					{
						for (int32 TriangleIndex = 0; TriangleIndex < ContactTriangles.Num(); ++TriangleIndex)
						{
							const FContactTriangle& OtherContactTriangle = ContactTriangles[TriangleIndex];
							if (OtherContactTriangle.HasVertexIndex(VertexIndexA))
							{
								FVec3 EdgeVertex0, EdgeVertex1;
								if (OtherContactTriangle.GetOtherVertexPositions(VertexIndexA, EdgeVertex0, EdgeVertex1))
								{
									// Does the contact normal point into the infinite prism formed by extruding the triangle along the face normal?
									const FVec3 EdgeDelta0 = EdgeVertex0 - VertexA;
									const FVec3 EdgeDelta1 = EdgeVertex1 - VertexA;
									const FReal EdgeSign0 = FVec3::DotProduct(FVec3::CrossProduct(ContactPoint.ShapeContactNormal, EdgeVertex0 - VertexA), OtherContactTriangle.FaceNormal);
									const FReal EdgeSign1 = FVec3::DotProduct(FVec3::CrossProduct(ContactPoint.ShapeContactNormal, EdgeVertex1 - VertexA), OtherContactTriangle.FaceNormal);
									if (FMath::Sign(EdgeSign0) == FMath::Sign(EdgeSign1))
									{
										const FVec3 Centroid = OtherContactTriangle.GetCentroid();
										const FReal NormalDotCentroid = FVec3::DotProduct(ContactPoint.ShapeContactNormal, Centroid - ContactPoint.ShapeContactPoints[1]);
										if (NormalDotCentroid > 0)
										{
											const FReal OtherContactDotNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, ContactTriangle.FaceNormal);
											ContactPoint.ShapeContactNormal = (OtherContactDotNormal > -SMALL_NUMBER) ? OtherContactTriangle.FaceNormal : -OtherContactTriangle.FaceNormal;
											ContactPoint.ShapeContactPoints[0] = ContactPoint.ShapeContactPoints[1] + ContactPoint.Phi * ContactPoint.ShapeContactNormal;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Remove contact points that have a normal pointing inthe opposite direction of the face normal
	void FContactTriangleCollector::PruneInfacingContactPoints()
	{
		for (int32 ContactIndex = 0; ContactIndex < TriangleContactPoints.Num(); ++ContactIndex)
		{
			const FTriangleContactPoint& ContactPoint = TriangleContactPoints[ContactIndex];
			const FContactTriangle& ContactTriangle = ContactTriangles[ContactPoint.ContactTriangleIndex];

			const FReal ContactNormalDotFaceNormal = FVec3::DotProduct(ContactPoint.ShapeContactNormal, ContactTriangle.FaceNormal);
			if (ContactNormalDotFaceNormal < 0)
			{
				RemoveContact(ContactIndex);
				--ContactIndex;
			}
		}
	}

	// Remove contact points that are much more shallow that the deepest
	// Remove contact points that are too close to others.
	// NOTE: ContactPoints must be sorted on Phi on entry, and will be sorted on exit
	// @todo(chaos): we should only remove shallow contacts that are hidden by nearby deeper contacts. It should be ok to have shallow contacts
	// far from the deeper ones. E.g., a box landing on plane at an angle.
	void FContactTriangleCollector::PruneUnnecessaryContactPoints(const FReal PhiTolerance, const FReal DistanceTolerance)
	{
		// Remove all points except for the deepest one, and ones with phis similar to it
		int32 NewContactPointCount = TriangleContactPoints.Num() > 0 ? 1 : 0;
		for (int32 Index = 1; Index < TriangleContactPoints.Num(); Index++)
		{
			if ((TriangleContactPoints[Index].Phi < 0) || ((TriangleContactPoints[Index].Phi - TriangleContactPoints[0].Phi) < PhiTolerance))
			{
				NewContactPointCount++;
			}
			else
			{
				break;
			}
		}
		SetNumContacts(NewContactPointCount);

		// Remove points that are very close together. Always keep the one that is least likely to be an edge contact
		const FReal DistanceToleranceSq = FMath::Square(DistanceTolerance);
		for (int32 ContactIndex0 = 0; ContactIndex0 < TriangleContactPoints.Num(); ++ContactIndex0)
		{
			const FTriangleContactPoint& ContactPoint0 = TriangleContactPoints[ContactIndex0];
			const FContactTriangle& ContactTriangle0 = ContactTriangles[ContactPoint0.ContactTriangleIndex];

			for (int32 ContactIndex1 = ContactIndex0 + 1; ContactIndex1 < TriangleContactPoints.Num(); ++ContactIndex1)
			{
				const FTriangleContactPoint& ContactPoint1 = TriangleContactPoints[ContactIndex1];
				const FContactTriangle& ContactTriangle1 = ContactTriangles[ContactPoint1.ContactTriangleIndex];

				// NOTE: ShapeContactPoints[1] is on the triangle
				const FVec3 ContactDelta = ContactPoint0.ShapeContactPoints[1] - ContactPoint1.ShapeContactPoints[1];
				const FReal ContactDeltaSq = ContactDelta.SizeSquared();
				if (ContactDeltaSq < DistanceToleranceSq)
				{
					const FReal NormalDot0 = FVec3::DotProduct(ContactPoint0.ShapeContactNormal, ContactTriangle0.FaceNormal);
					const FReal NormalDot1 = FVec3::DotProduct(ContactPoint1.ShapeContactNormal, ContactTriangle1.FaceNormal);
					if (FMath::Abs(NormalDot0) >= FMath::Abs(NormalDot1))
					{
						TriangleContactPoints.RemoveAt(ContactIndex1, 1, false);
						--ContactIndex1;
						continue;
					}
					else
					{
						TriangleContactPoints.RemoveAt(ContactIndex0, 1, false);
						--ContactIndex0;
						break;
					}
				}
			}
		}
	}

	// Reduce the number of contact points (in place)
	// Prerequisites to calling this function:
	// ContactPoints are sorted on phi (ascending)
	void FContactTriangleCollector::ReduceManifoldContactPointsTriangeMesh()
	{
		if (TriangleContactPoints.Num() <= 4)
		{
			return;
		}

		// Point 1) is the deepest contact point
		// It is already in position

		//
		// @todo(chaos): actually this isn't really good enough. We want to take the outer 4 points for the most stable manifold, but also
		// must take the deepest for correct behaviour. We also have a problem right now where if we have a number of contacts at the same 
		// depth, the "deepest" one is arbitrary and may be a poor choice as the anchor of the manifold building. E.g., Consider 5 contact 
		// points in a X config where the center point is first in the list - this leads to a triangular/unstable manifold.
		// 
		// Maybe we should do this:
		//	- build the manifold using the current process
		//	- revisit point 0 at the end and replace with the best choice for stability if the depth is similar
		// Or this:
		//	- build manifold for best stabilty (largest convex area)
		//	- if manifold does not contain deepest point, replace one point so minimize area reduction
		//

		// Point 2) Find the point with the largest distance to the deepest contact point
		{
			uint32 FarthestPointIndex = 1;
			FReal FarthestPointDistanceSQR = -1.0f;
			for (int32 PointIndex = 1; PointIndex < TriangleContactPoints.Num(); PointIndex++)
			{
				FReal PointAToPointBSizeSQR = (TriangleContactPoints[PointIndex].ShapeContactPoints[1] - TriangleContactPoints[0].ShapeContactPoints[1]).SizeSquared();
				if (PointAToPointBSizeSQR > FarthestPointDistanceSQR)
				{
					FarthestPointIndex = PointIndex;
					FarthestPointDistanceSQR = PointAToPointBSizeSQR;
				}
			}
			// Farthest point will be added now
			Swap(TriangleContactPoints[1], TriangleContactPoints[FarthestPointIndex]);
		}

		// Point 3) Largest triangle area
		{
			uint32 LargestTrianglePointIndex = 2;
			FReal LargestTrianglePointSignedArea = 0.0f; // This will actually be double the signed area
			FVec3 P0to1 = TriangleContactPoints[1].ShapeContactPoints[1] - TriangleContactPoints[0].ShapeContactPoints[1];
			for (int32 PointIndex = 2; PointIndex < TriangleContactPoints.Num(); PointIndex++)
			{
				FReal TriangleSignedArea = FVec3::DotProduct(FVec3::CrossProduct(P0to1, TriangleContactPoints[PointIndex].ShapeContactPoints[1] - TriangleContactPoints[0].ShapeContactPoints[1]), TriangleContactPoints[0].ShapeContactNormal);
				if (FMath::Abs(TriangleSignedArea) > FMath::Abs(LargestTrianglePointSignedArea))
				{
					LargestTrianglePointIndex = PointIndex;
					LargestTrianglePointSignedArea = TriangleSignedArea;
				}
			}
			// Point causing the largest triangle will be added now
			Swap(TriangleContactPoints[2], TriangleContactPoints[LargestTrianglePointIndex]);
			// Ensure the winding order is consistent
			if (LargestTrianglePointSignedArea < 0)
			{
				Swap(TriangleContactPoints[0], TriangleContactPoints[1]);
			}
		}

		// Point 4) Find the largest triangle connecting with our current triangle
		{
			uint32 LargestTrianglePointIndex = 3;
			FReal LargestPositiveTrianglePointSignedArea = 0.0f;
			for (int32 PointIndex = 3; PointIndex < TriangleContactPoints.Num(); PointIndex++)
			{
				for (uint32 EdgeIndex = 0; EdgeIndex < 3; EdgeIndex++)
				{
					FReal TriangleSignedArea = FVec3::DotProduct(FVec3::CrossProduct(TriangleContactPoints[PointIndex].ShapeContactPoints[1] - TriangleContactPoints[EdgeIndex].ShapeContactPoints[1], TriangleContactPoints[(EdgeIndex + 1) % 3].ShapeContactPoints[1] - TriangleContactPoints[EdgeIndex].ShapeContactPoints[1]), TriangleContactPoints[0].ShapeContactNormal);
					if (TriangleSignedArea > LargestPositiveTrianglePointSignedArea)
					{
						LargestTrianglePointIndex = PointIndex;
						LargestPositiveTrianglePointSignedArea = TriangleSignedArea;
					}
				}
			}
			// Point causing the largest positive triangle area will be added now
			Swap(TriangleContactPoints[3], TriangleContactPoints[LargestTrianglePointIndex]);
		}

		// Revisit Point 0 - if we have a choice that increases area at similar depth, use it

		// Will end up with 4 points
		SetNumContacts(4);
	}

	void FContactTriangleCollector::FinalizeContacts(const FRigidTransform3& MeshToConvexTransform)
	{
		FinalContactPoints.Reset(TriangleContactPoints.Num());

		// Transform contact data back into shape-local space and build the output array
		for (int32 ContactIndex = 0; ContactIndex < TriangleContactPoints.Num(); ++ContactIndex)
		{
			FTriangleContactPoint& ContactPoint = TriangleContactPoints[ContactIndex];
			const FContactTriangle& ContactTriangle = ContactTriangles[ContactPoint.ContactTriangleIndex];

			ContactPoint.ShapeContactPoints[1] = MeshToConvexTransform.InverseTransformPositionNoScale(ContactPoint.ShapeContactPoints[1]);
			ContactPoint.ShapeContactNormal = MeshToConvexTransform.InverseTransformVectorNoScale(ContactPoint.ShapeContactNormal);

			FinalContactPoints.Add(ContactPoint);
		}
	}

	void FContactTriangleCollector::DebugDrawContactPoints(const FColor& Color, const FReal LineThickness)
	{
#if CHAOS_DEBUG_DRAW
		if (CVars::ChaosSolverDebugDrawMeshContacts && FDebugDrawQueue::GetInstance().IsDebugDrawingEnabled())
		{
			TArray<bool> bTriangleDrawn;
			bTriangleDrawn.SetNumZeroed(ContactTriangles.Num());

			const FReal Duration = 0;
			const int32 DrawPriority = 10;
			for (const FTriangleContactPoint& ContactPoint : TriangleContactPoints)
			{
				const FVec3 P0 = ConvexTransform.TransformPosition(ContactPoint.ShapeContactPoints[0]);
				const FVec3 P1 = ConvexTransform.TransformPosition(ContactPoint.ShapeContactPoints[1]);
				const FVec3 N = ConvexTransform.TransformVectorNoScale(ContactPoint.ShapeContactNormal);

				// Draw the normal from the triangle face
				FDebugDrawQueue::GetInstance().DrawDebugLine(P1, P1 + FReal(15) * N, Color, false, FRealSingle(Duration), DrawPriority, FRealSingle(LineThickness));

				// Draw a thin black line connecting the two contact points (triangle face to convex surface)
				FDebugDrawQueue::GetInstance().DrawDebugLine(P0, P1, FColor::Black, false, FRealSingle(Duration), DrawPriority, 0.5f * FRealSingle(LineThickness));

				if (!bTriangleDrawn[ContactPoint.ContactTriangleIndex])
				{
					FContactTriangle& Triangle = ContactTriangles[ContactPoint.ContactTriangleIndex];
					const FVec3 V0 = ConvexTransform.TransformPosition(Triangle.Vertices[0]);
					const FVec3 V1 = ConvexTransform.TransformPosition(Triangle.Vertices[1]);
					const FVec3 V2 = ConvexTransform.TransformPosition(Triangle.Vertices[2]);
					FDebugDrawQueue::GetInstance().DrawDebugLine(V0, V1, FColor::Silver, false, FRealSingle(Duration), DrawPriority, 0.5f * FRealSingle(LineThickness));
					FDebugDrawQueue::GetInstance().DrawDebugLine(V1, V2, FColor::Silver, false, FRealSingle(Duration), DrawPriority, 0.5f * FRealSingle(LineThickness));
					FDebugDrawQueue::GetInstance().DrawDebugLine(V2, V0, FColor::Silver, false, FRealSingle(Duration), DrawPriority, 0.5f * FRealSingle(LineThickness));
					bTriangleDrawn[ContactPoint.ContactTriangleIndex] = true;
				}
			}
		}
#endif
	}

}