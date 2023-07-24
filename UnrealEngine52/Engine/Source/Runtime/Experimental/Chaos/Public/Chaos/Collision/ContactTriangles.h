// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UnrealTemplate.h"

#include "Chaos/Core.h"
#include "Chaos/Triangle.h"

namespace Chaos
{
	/**
	 * @brief Data held alongside contact points when generating contacts against a (likely non-convex) mesh of triangles
	*/
	class CHAOS_API FContactTriangle
	{
	public:
		// Triangle data
		FVec3 Vertices[3];
		FVec3 FaceNormal;

		// Indices into the mesh (trimesh or heightfield) that generated the triangle
		int32 VertexIndices[3];
		int32 TriangleIndex;

		// Does this triangle contains the specified vertex? (VertexIndex is an index into the owning mesh's vertices)
		inline bool HasVertexIndex(const int32 VertexIndex) const
		{
			return (VertexIndices[0] == VertexIndex) || (VertexIndices[1] == VertexIndex) || (VertexIndices[2] == VertexIndex);
		}

		// Get the centroid of the triangle
		inline FVec3 GetCentroid() const
		{
			return (Vertices[0] + Vertices[1] + Vertices[2]) / FReal(3);
		}

		// Get the positions of the other two vertices in the triangle. (VertexIndex is an index into the owning mesh's vertices)
		inline bool GetOtherVertexPositions(const int32 VertexIndex, FVec3& OutVertex0, FVec3& OutVertex1) const
		{
			if (VertexIndex == VertexIndices[0])
			{
				OutVertex0 = Vertices[1];
				OutVertex1 = Vertices[2];
				return true;
			}
			else if (VertexIndex == VertexIndices[1])
			{
				OutVertex0 = Vertices[2];
				OutVertex1 = Vertices[0];
				return true;
			}
			else if (VertexIndex == VertexIndices[2])
			{
				OutVertex0 = Vertices[0];
				OutVertex1 = Vertices[1];
				return true;
			}
			return false;
		}

		// Given a contact point on a triangle, check if it is an edge contact or vertex contact and return vertex indices if so
		// @todo(chaos): this function should be unnecessary. Collision detection should return the vertex indices for the vertex/edge/face that we collide with
		inline bool GetEdgeVerticesAtPosition(const FVec3& ContactPosition, FVec3& OutEdgeVertexA, FVec3& OutEdgeVertexB, int32& OutVertexIndexA, int32& OutVertexIndexB, const FReal BaryCentricTolerance = UE_KINDA_SMALL_NUMBER) const
		{
			OutVertexIndexA = INDEX_NONE;
			OutVertexIndexB = INDEX_NONE;

			const FVec3 BaryCentric = ToBarycentric(ContactPosition, Vertices[0], Vertices[1], Vertices[2]);

			// Is it a vertex contact?
			if (FMath::IsNearlyEqual(BaryCentric.X, FReal(1), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[0];
				OutEdgeVertexA = Vertices[0];
				return true;
			}
			if (FMath::IsNearlyEqual(BaryCentric.Y, FReal(1), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[1];
				OutEdgeVertexA = Vertices[1];
				return true;
			}
			if (FMath::IsNearlyEqual(BaryCentric.Z, FReal(1), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[2];
				OutEdgeVertexA = Vertices[2];
				return true;
			}

			// Is it an edge contact?
			if (FMath::IsNearlyEqual(BaryCentric.X, FReal(0), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[1];
				OutVertexIndexB = VertexIndices[2];
				OutEdgeVertexA = Vertices[1];
				OutEdgeVertexB = Vertices[2];
				return true;
			}
			if (FMath::IsNearlyEqual(BaryCentric.Y, FReal(0), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[2];
				OutVertexIndexB = VertexIndices[0];
				OutEdgeVertexA = Vertices[2];
				OutEdgeVertexB = Vertices[0];
				return true;
			}
			if (FMath::IsNearlyEqual(BaryCentric.Z, FReal(0), BaryCentricTolerance))
			{
				OutVertexIndexA = VertexIndices[0];
				OutVertexIndexB = VertexIndices[1];
				OutEdgeVertexA = Vertices[0];
				OutEdgeVertexB = Vertices[1];
				return true;
			}

			return false;
		}
	};

	/**
	 * @brief A set of triangles assocuated with some contact points.
	*/
	class CHAOS_API FContactTriangles
	{
	public:

		inline const int32 Num() const
		{
			return Triangles.Num();
		}

		inline const FContactTriangle& At(const int32 Index) const
		{
			return Triangles[Index];
		}


	private:
		TArray<FContactTriangle> Triangles;
	};

	using FContactVertexID = int32;

	struct FContactEdgeID
	{
		FContactEdgeID()
			: VertexIDs{ INDEX_NONE < INDEX_NONE }
		{
		}

		FContactEdgeID(const FContactVertexID VertextIndexA, const FContactVertexID VertexIndexB)
			: VertexIDs{ FMath::Min(VertextIndexA, VertexIndexB), FMath::Max(VertextIndexA, VertexIndexB) }
		{
		}

		bool IsValid() const
		{
			return (VertexIDs[0] != INDEX_NONE) && (VertexIDs[1] != INDEX_NONE);
		}

		friend bool operator==(const FContactEdgeID& L, const FContactEdgeID& R)
		{
			return L.EdgeID == R.EdgeID;
		}

		friend bool operator!=(const FContactEdgeID& L, const FContactEdgeID& R)
		{
			return L.EdgeID != R.EdgeID;
		}

		friend bool operator<(const FContactEdgeID& L, const FContactEdgeID& R)
		{
			return L.EdgeID < R.EdgeID;
		}

		friend uint32 GetTypeHash(const FContactEdgeID& V)
		{
			return ::GetTypeHash(V.EdgeID);
		}

		union
		{
			FContactVertexID VertexIDs[2];
			uint64 EdgeID;
		};
	};

	/**
	 * @brief Extended data used when processing contactpoints on a triangle mesh. 
	 * Adds information about which triangle we hit (in the local array of FoOntactTriangles)
	 * as well as which mesh edge or vertex we hit if appropriate (used for pruning).
	*/
	class FTriangleContactPoint : public FContactPoint
	{
	public:
		FTriangleContactPoint()
			: FContactPoint()
			, EdgeID(INDEX_NONE, INDEX_NONE)
			, VertexID(INDEX_NONE)
			, ContactTriangleIndex(INDEX_NONE)
		{
		}

		FTriangleContactPoint(const FContactPoint& InContactPoint)
			: FContactPoint(InContactPoint)
			, EdgeID(INDEX_NONE, INDEX_NONE)
			, VertexID(INDEX_NONE)
			, ContactTriangleIndex(INDEX_NONE)
		{
		}

		FContactEdgeID EdgeID;
		FContactVertexID VertexID;

		int32 ContactTriangleIndex;
	};

	/**
	 * Holds the contacts produced when colliding a convex shape against a non-convex triangular mesh, and provides
	 * methods to reduce the total set of contact points down to a minimum manifold.
	*/
	class FContactTriangleCollector
	{
	public:
		FContactTriangleCollector(const bool bInOneSided, const FRigidTransform3& InConvexTransform)
			: bOneSidedCollision(bInOneSided)
			, ConvexTransform(InConvexTransform)
		{
		}

		inline TArrayView<const FContactPoint> GetContactPoints() const
		{ 
			return MakeArrayView(FinalContactPoints);
		}
		
		// Add contacts between the convex and a single triangle for later processing
		inline void AddTriangleContacts(const TArrayView<const FContactPoint>& InContactPoints, const FTriangle& Triangle, const int32 TriangleIndex, const int32 VertexIndex0, const int32 VertexIndex1, const int32 VertexIndex2, const FReal CullDistance)
		{
			if (InContactPoints.Num() > 0)
			{
				const int32 ContactTriangleIndex = AddTriangle(Triangle, TriangleIndex, VertexIndex0, VertexIndex1, VertexIndex2);

				TriangleContactPoints.Reserve(TriangleContactPoints.Num() + InContactPoints.Num());
				for (const FContactPoint& ContactPoint : InContactPoints)
				{
					if (ContactPoint.Phi < CullDistance)
					{
						const int32 ContactIndex = TriangleContactPoints.Add(ContactPoint);

						TriangleContactPoints[ContactIndex].FaceIndex = TriangleIndex;
						TriangleContactPoints[ContactIndex].ContactTriangleIndex = ContactTriangleIndex;
					}
				}
			}
		}

		// To be called when all contacts have been added. Reduces the set of contacts to a minimum and transforms data into shape space
		void ProcessContacts(const FRigidTransform3& MeshToConvexTransform);

	private:
		void SetNumContacts(const int32 Num);
		void RemoveContact(const int32 ContactIndex);

		void BuildContactFeatureSets();
		void SortContactPointsForPruning();
		void SortContactPointsForSolving();
		void PruneEdgeContactPoints();
		void FixInvalidNormalContactPoints();
		void PruneInfacingContactPoints();
		void PruneUnnecessaryContactPoints(const FReal PhiTolerance, const FReal DistanceTolerance);
		void ReduceManifoldContactPointsTriangeMesh();
		void FinalizeContacts(const FRigidTransform3& MeshToConvexTransform);

		int32 AddTriangle(const FTriangle& Triangle, const int32 TriangleIndex, const int32 VertexIndex0, const int32 VertexIndex1, const int32 VertexIndex2);
		int32 GetOtherTriangleIndexSharingEdge(const int32 KnownTriangleIndex, const int32 EdgeVertexIndexA, const int32 EdgeVertexIndexB) const;
		int32 GetNextTriangleIndexSharingVertex(const int32 LastContactTriangleIndex, const int32 VertexIndex) const;

		void DebugDrawContactPoints(const FColor& Color, const FReal LineThickness);

		// The output set of contacts
		// @todo(chaos): would be nice if we didn't need this and we could provide TriangleContactPoints instead
		TArray<FContactPoint> FinalContactPoints;

		// The collected set of contacts with additional triangle information
		TArray<FTriangleContactPoint> TriangleContactPoints;

		// All the triangles referenced by the TriangleContactPoints
		TArray<FContactTriangle> ContactTriangles;

		// A list of all the edges of contacts with faces. Used to prune edge contacts
		TSet<FContactEdgeID> ContactEdges;

		// A list of all the vertices of contacts with faces and edges. Used to prune vertex contacts
		TSet<FContactVertexID> ContactVertices;

		bool bOneSidedCollision;

		FRigidTransform3 ConvexTransform;
	};

}