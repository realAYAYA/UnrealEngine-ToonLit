// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ConvertData.h"
#include "MuR/MeshPrivate.h"

#include "Math/Plane.h"
#include "Math/Ray.h"
#include "Math/NumericLimits.h"

#include "IndexTypes.h"
#include "Spatial/MeshAABBTree3.h"



// TODO: Make the handling of rotations an option. It is more expensive on CPU and memory, and for some
// cases it is not required at all.

// TODO: Face stretch to scale the deformation per-vertex? 

// TODO: Support multiple binding influences per vertex, to have smoother deformations.

// TODO: Support multiple binding sets, to have different shapes deformations at once.

// TODO: Deformation mask, to select the intensisty of the deformation per-vertex.

// TODO: This is a reference implementation with ample roof for optimization.

namespace mu
{

	template<class PositionCompType, class NormalCompType>
	struct FShapeMeshDescriptor
	{
		TArray<UE::Math::TVector<PositionCompType>> Positions;
		TArray<UE::Math::TVector<NormalCompType>> Normals;
		TArray<UE::Geometry::FIndex3i> Triangles;
	};

	using FShapeMeshDescriptorBind = FShapeMeshDescriptor<double, float>;
		
	struct FShapeMeshAdapter
	{
		const FShapeMeshDescriptorBind& Mesh;

		FShapeMeshAdapter(const FShapeMeshDescriptorBind& InMesh ) 
			: Mesh(InMesh)
		{}

		int32 MaxTriangleID() const
		{
			return Mesh.Triangles.Num();
		}

		int32 MaxVertexID() const
		{
			return Mesh.Positions.Num();
		}

		bool IsTriangle(int32 Index) const
		{
			return Mesh.Triangles.IsValidIndex(Index);
		}

		bool IsVertex(int32 Index) const
		{
			return Mesh.Positions.IsValidIndex(Index);
		}

		int32 TriangleCount() const
		{
			return Mesh.Triangles.Num();
		}

		FORCEINLINE int32 VertexCount() const
		{
			return Mesh.Positions.Num();
		}

		FORCEINLINE uint64 GetChangeStamp() const
		{
			return 1;
		}

		FORCEINLINE UE::Geometry::FIndex3i GetTriangle(int32 Index) const
		{
			return Mesh.Triangles[Index];
		}

		FORCEINLINE FVector3d GetVertex(int32 Index) const
		{
			return Mesh.Positions[Index];
		}

		FORCEINLINE void GetTriVertices(int32 TriIndex, FVector3d& V0, FVector3d& V1, FVector3d& V2) const
		{
			const UE::Geometry::FIndex3i& Indices = Mesh.Triangles[TriIndex];

			V0 = Mesh.Positions[Indices.A];
			V1 = Mesh.Positions[Indices.B];
			V2 = Mesh.Positions[Indices.C];
		}
	};

	using FShapeMeshTree = UE::Geometry::TMeshAABBTree3<const FShapeMeshAdapter>;

	// Structure used for vertex bing data in vertexbuffers for reshape operations.
	struct FReshapeVertexBindingData
	{
		// Barycentric coordinates on the shape triangle
		float S, T;

		// Distance along the interpolated normal of the shape triangle.
		float D;

		// Index of the shape triangle.
		int32 Triangle;


		// Used to calculate the rotation to apply to the reshaped vertex tangent space.
		FVector3f ShapeNormal;
		
		// Bind point, if the point belongs to a rigid cluster, the attachment point, otherwise
		// the original point.
		FVector3f AttachmentPoint;

		// Weight of the effect for this binding. Ranged between 0 and 1 where 0 denotes no effect at all and 1
		// full effect. This weight should be proportional to the confidence we have that the binding data is valid. 
		float Weight;
	};
	static_assert(sizeof(FReshapeVertexBindingData) == 44);

	struct FReshapeVertexBindingDataBufferDescriptor
	{
		constexpr static int Channels = 4;
		constexpr static MESH_BUFFER_SEMANTIC Semantics[Channels] = { MBS_BARYCENTRICCOORDS, MBS_DISTANCE, MBS_TRIANGLEINDEX, MBS_OTHER };
		constexpr static MESH_BUFFER_FORMAT Formats[Channels] = { MBF_FLOAT32, MBF_FLOAT32, MBF_INT32, MBF_FLOAT32 };
		constexpr static int Components[Channels] = { 2, 1, 1, 3+3+1 };
		constexpr static int Offsets[Channels] = { 0, 8, 12, 16 };

		const int SemanticIndices[Channels] = { 0, 0, 0, 0 };

		FReshapeVertexBindingDataBufferDescriptor(int32 DataSetIndex)
			: SemanticIndices{ DataSetIndex, DataSetIndex, DataSetIndex, DataSetIndex }
		{
		}
	};

	struct FUsedIndicesBufferDescriptor
	{
		constexpr static int Channels = 1;
		constexpr static MESH_BUFFER_SEMANTIC Semantics[Channels] = { MBS_OTHER };
		constexpr static MESH_BUFFER_FORMAT Formats[Channels] = { MBF_INT32 };
		constexpr static int Components[Channels] = { 1 };
		constexpr static int Offsets[Channels] = { 0 };
		constexpr static int SemanticIndices[Channels] = { 0 };

		FUsedIndicesBufferDescriptor()
		{
		}
	};

	// Structure used for vertex bing data in vertexbuffers for Clip deform operations.
	struct FClipDeformVertexBindingData
	{
		// Barycentric coordinates on the shape triangle
		float S, T;

		// Index of the shape triangle.
		int32 Triangle;

		float Weight;
	};
	static_assert(sizeof(FClipDeformVertexBindingData) == 16);

	struct FClipDeformVertexBindingDataBufferDescriptor
	{
		constexpr static int Channels = 3;
		constexpr static MESH_BUFFER_SEMANTIC Semantics[Channels] = { MBS_BARYCENTRICCOORDS, MBS_TRIANGLEINDEX, MBS_OTHER };
		constexpr static MESH_BUFFER_FORMAT Formats[Channels] = { MBF_FLOAT32, MBF_INT32, MBF_FLOAT32 };
		constexpr static int Components[Channels] = { 2, 1, 1 };
		constexpr static int Offsets[Channels] = { 0, 8, 12 };

		const int SemanticIndices[Channels] = { 0, 0, 0 };

		FClipDeformVertexBindingDataBufferDescriptor(int32 DataSetIndex)
			: SemanticIndices{ DataSetIndex, DataSetIndex, DataSetIndex }
		{
		}
	};


	//---------------------------------------------------------------------------------------------
    //! Generate the mesh-shape binding data for Reshape operations
    //---------------------------------------------------------------------------------------------

	inline void BindReshapePoint(
			FShapeMeshTree& ShapeMeshTree,
			const FVector3f& Point, FReshapeVertexBindingData& OutBindData, 
			const float ValidityTolerance = UE_KINDA_SMALL_NUMBER)
	{

		const FShapeMeshDescriptorBind& ShapeMesh = ShapeMeshTree.GetMesh()->Mesh;
		
		OutBindData.S = 0.0f;
		OutBindData.T = 0.0f;
		OutBindData.D = 0.0f;
		OutBindData.Triangle = -1;
		OutBindData.ShapeNormal = FVector3f::Zero();
		OutBindData.AttachmentPoint = Point;
		float Weight = 1.0f;

		double DistSqr = 0.0;

		int32 FoundIndex = ShapeMeshTree.FindNearestTriangle( FVector3d(Point), DistSqr );
			
		if (FoundIndex < 0)
		{
			return;
		}

		// Calculate the binding data of the base mesh vertex to its bound shape triangle
		UE::Geometry::FIndex3i Triangle = ShapeMesh.Triangles[FoundIndex];

		// Project on the triangle, but using the vertex normals.
		// See reference implementation for details. 
		FVector3f TriangleA = (FVector3f)ShapeMesh.Positions[Triangle.A];
		FVector3f TriangleB = (FVector3f)ShapeMesh.Positions[Triangle.B];
		FVector3f TriangleC = (FVector3f)ShapeMesh.Positions[Triangle.C];

		FPlane4f TrianglePlane(TriangleA, TriangleB, TriangleC);
		FPlane4f VertexPlane(Point, TrianglePlane.GetNormal());
	
		// T1 = Triangle projected on the vertex plane along the triangle vertex normals
		FVector3f TriangleA_VertexPlane = FMath::RayPlaneIntersection(TriangleA, ShapeMesh.Normals[Triangle.A], VertexPlane);
		FVector3f TriangleB_VertexPlane = FMath::RayPlaneIntersection(TriangleB, ShapeMesh.Normals[Triangle.B], VertexPlane);
		FVector3f TriangleC_VertexPlane = FMath::RayPlaneIntersection(TriangleC, ShapeMesh.Normals[Triangle.C], VertexPlane);
	
		// Barycentric coordinates of the vertex on in T1
		FVector3f Barycentric = (FVector3f)FMath::ComputeBaryCentric2D((FVector)Point, (FVector)TriangleA_VertexPlane, (FVector)TriangleB_VertexPlane, (FVector)TriangleC_VertexPlane);

		FVector3f ProjectedPoint = TriangleA * Barycentric.X + TriangleB * Barycentric.Y + TriangleC * Barycentric.Z;

		FVector3f InterpolatedVertexNormal = ShapeMesh.Normals[Triangle.A] * Barycentric.X + ShapeMesh.Normals[Triangle.B] * Barycentric.Y + ShapeMesh.Normals[Triangle.C] * Barycentric.Z;
		FVector3f ProjectedToVertex = (Point - ProjectedPoint);

		// If the interpolated normal is not normalized the dot product gives the distance to the point times the length of the normal.
		// We don't want that since when deforming a point, using the interpolated normal, we'd multiply by the length twice. See GetDeform().
		// One option would be to normalize here (computing the distance) and when deforming the mesh.
		// We can also compensate the signed distance so that the same interpolated normal (not normalized) gives us the correct point
		// ( InterpolatedVertexNormal * d = ProjectedToVertex). That way modifications in the interpolated bound normal will affect the
		// resulting point. We are doing the later.
		const float InterpolatedNormalSizeSquared = InterpolatedVertexNormal.SizeSquared();
		const float InvInterpolatedNormalSizeSquared = InterpolatedNormalSizeSquared > UE_SMALL_NUMBER ? 1.0f / InterpolatedNormalSizeSquared : 0.0f;	
		float d = FVector3f::DotProduct(ProjectedToVertex, InterpolatedVertexNormal) * InvInterpolatedNormalSizeSquared;
	
		OutBindData.S = Barycentric.Y;
		OutBindData.T = Barycentric.Z;
		OutBindData.D = d;
		
		OutBindData.Triangle = FoundIndex;
		
		OutBindData.ShapeNormal = ((TriangleB - TriangleA) ^ (TriangleC - TriangleA)).GetSafeNormal();
		
		const FVector3f ReprojectedPoint = ProjectedPoint + InterpolatedVertexNormal * d;
		
		const FVector3f ReprojectedVector = ReprojectedPoint - Point;
		const float ErrorEstimate = (ReprojectedPoint - Point).GetAbsMax();
		
		// If within the tolerance, 1.0, otherwise linear falloff based on the tolerance

		// Arbitrary factor, a binding will be considered valid (with its corresponding weight) to ErrorFalloffFactor times the validity tolerance.
		constexpr float ErrorFalloffFactor = 4.0f;
		OutBindData.Weight = 1.0f - FMath::Clamp( (ErrorEstimate - ValidityTolerance) / (ValidityTolerance * ErrorFalloffFactor), 0.0f, 1.0f );
		
		OutBindData.Triangle = FMath::IsNearlyZero(OutBindData.Weight) ? -1 : OutBindData.Triangle;
	}

	inline void BindPointReference(
			const TArray<UE::Geometry::FIndex3i>& ShapeTriangles, const TArray<FVector3f>& ShapePositions, const TArray<FVector3f>& ShapeNormals,
			const FVector3f& Point, FReshapeVertexBindingData& BindData,
			const float ValidityTolerance = UE_KINDA_SMALL_NUMBER)
	{
		BindData.S = 0.0f;
		BindData.T = 0.0f;
		BindData.D = 0.0f;
		BindData.Triangle = -1;
		BindData.ShapeNormal = FVector3f::Zero();
		BindData.AttachmentPoint = Point;
		float Weight = 1.0f;

		// Find the best shape triangle for each base mesh vertex

		// Select the triangle closest to the vertex.
		// \TODO: review this strategy.
		int32 TriangleIndex = -1;
		float BestDistSquared = TNumericLimits<float>::Max();
		for (int ShapeTriangleIndex = 0; ShapeTriangleIndex < ShapeTriangles.Num(); ++ShapeTriangleIndex)
		{
			UE::Geometry::FIndex3i Triangle = ShapeTriangles[ShapeTriangleIndex];
			FVector3f TriangleA = ShapePositions[Triangle.A];
			FVector3f TriangleB = ShapePositions[Triangle.B];
			FVector3f TriangleC = ShapePositions[Triangle.C];
			FVector3f ClosestPoint = (FVector3f)FMath::ClosestPointOnTriangleToPoint((FVector)Point, (FVector)TriangleA, (FVector)TriangleB, (FVector)TriangleC);
			float DistSquared = (ClosestPoint - Point).SquaredLength();
			if (DistSquared < BestDistSquared)
			{
				TriangleIndex = ShapeTriangleIndex;
				BestDistSquared = DistSquared;
			}
		}

		if (TriangleIndex < 0)
		{
			//check(false);
			return;
		}

		// Calculate the binding data of the base mesh vertex to its bound shape triangle
		UE::Geometry::FIndex3i Triangle = ShapeTriangles[TriangleIndex];

		// Project on the triangle, but using the vertex normals.
		// See figure 3 in https://www.sciencedirect.com/science/article/pii/S1319157820303931
		FVector3f TriangleA = ShapePositions[Triangle.A];
		FVector3f TriangleB = ShapePositions[Triangle.B];
		FVector3f TriangleC = ShapePositions[Triangle.C];

		FPlane4f TrianglePlane(TriangleA, TriangleB, TriangleC);
		FPlane4f VertexPlane(Point, TrianglePlane.GetNormal());
	
		// T1 = Triangle projected on the vertex plane along the triangle vertex normals
		FVector3f TriangleA_VertexPlane = FMath::RayPlaneIntersection(TriangleA, ShapeNormals[Triangle.A], VertexPlane);
		FVector3f TriangleB_VertexPlane = FMath::RayPlaneIntersection(TriangleB, ShapeNormals[Triangle.B], VertexPlane);
		FVector3f TriangleC_VertexPlane = FMath::RayPlaneIntersection(TriangleC, ShapeNormals[Triangle.C], VertexPlane);
	
		// Barycentric coordinates of the vertex on in T1
		FVector3f Barycentric = (FVector3f)FMath::ComputeBaryCentric2D((FVector)Point, (FVector)TriangleA_VertexPlane, (FVector)TriangleB_VertexPlane, (FVector)TriangleC_VertexPlane);

		FVector3f ProjectedPoint = TriangleA * Barycentric.X + TriangleB * Barycentric.Y + TriangleC * Barycentric.Z;

		FVector3f InterpolatedVertexNormal = ShapeNormals[Triangle.A] * Barycentric.X + ShapeNormals[Triangle.B] * Barycentric.Y + ShapeNormals[Triangle.C] * Barycentric.Z;
		FVector3f ProjectedToVertex = (Point - ProjectedPoint);

		// If the interpolated normal is not normalized the dot product gives the distance to the point times the length of the normal.
		// We don't want that since when deforming a point, using the interpolated normal, we'd multiply by the length twice. See GetDeform().
		// One option would be to normalize here (computing the distance) and when deforming the mesh.
		// We can also compensate the signed distance so that the same interpolated normal (not normalized) gives us the correct point
		// ( InterpolatedVertexNormal * d = ProjectedToVertex). That way modifications in the interpolated bound normal will affect the
		// resulting point. We are doing the later.
		const float InterpolatedNormalSizeSquared = InterpolatedVertexNormal.SizeSquared();
		const float InvInterpolatedNormalSizeSquared = InterpolatedNormalSizeSquared > SMALL_NUMBER ? 1.0f / InterpolatedNormalSizeSquared : 0.0f;	
		float d = FVector3f::DotProduct(ProjectedToVertex, InterpolatedVertexNormal) * InvInterpolatedNormalSizeSquared;
	
		BindData.S = Barycentric.Y;
		BindData.T = Barycentric.Z;
		BindData.D = d;
		
		BindData.Triangle = TriangleIndex;
		
		BindData.ShapeNormal = ((TriangleB - TriangleA) ^ (TriangleC - TriangleA)).GetSafeNormal();
		
		const FVector3f ReprojectedPoint = ProjectedPoint + InterpolatedVertexNormal * d;
		
		const FVector3f ReprojectedVector = ReprojectedPoint - Point;
		const float ErrorEstimate = (ReprojectedPoint - Point).GetAbsMax();
		
		// If within the tolerance, 1.0, otherwise linear falloff based on the tolerance

		// Arbitrary factor, a binding will be considered valid (with its corresponding weight) to ErrorFalloffFactor times the validity tolerance.
		constexpr float ErrorFalloffFactor = 4.0f;
		BindData.Weight = 1.0f - FMath::Clamp( (ErrorEstimate - ValidityTolerance) / (ValidityTolerance * ErrorFalloffFactor), 0.0f, 1.0f );
		
		BindData.Triangle = FMath::IsNearlyZero(BindData.Weight) ? -1 : BindData.Triangle;
	}
	
    //---------------------------------------------------------------------------------------------
    //! Find mesh clusters.
	//! Colour {1,1,1} is reserved for the non rigid cluster
    //---------------------------------------------------------------------------------------------
	
	inline void FindRigidClusters( const Mesh* Mesh, TArray<TArray<int32>>& OutClusters, int32& OutNonRigidClusterIdx)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshFindRigidClusters);
		
		const FVector3f NonRigidColour = FVector3f(1.0f, 1.0f, 1.0f);

		OutNonRigidClusterIdx = -1;
		
		const UntypedMeshBufferIteratorConst ItColoursBase(Mesh->GetVertexBuffers(), MBS_COLOUR);
		if (ItColoursBase.ptr())
		{
			const int32 VertexCount = Mesh->GetVertexCount();

			// Check if all colors are the same. (maybe should only search for white as it indicates no rigidity)
			{
				int32 VertexIndex = 0;
				for (; VertexIndex < VertexCount; ++VertexIndex)
				{
					 // Colours are treated as ids, no need to use tolerance with the comparison.
					 if (NonRigidColour != (ItColoursBase + VertexIndex).GetAsVec3f())
					 {
						  break;
					 }
				}

				// If all equal to the non rigid we are done. 
				if (VertexIndex == VertexCount)
				{
					 return;
				}
			}
			
			OutClusters.Empty(16);
			TArray<TArray<int32>>& ClusterData = OutClusters;
			// Consider using NUINT8 colors for id and pack them in a uint32 as key.
			TMap<FVector3f, int32> ClusterSet;

			for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
			{
				FVector3f Colour = (ItColoursBase + VertexIndex).GetAsVec3f();
				int32& ClusterIdx = ClusterSet.FindOrAdd( Colour, INDEX_NONE );
				TArray<int32>& Cluster = ClusterIdx < 0 ? ClusterData.Emplace_GetRef() : ClusterData[ClusterIdx];
				if (ClusterIdx < 0)
				{
					ClusterIdx = ClusterData.Num() - 1;
					Cluster.Reserve(32);
				}

				Cluster.Push(VertexIndex);	
			}

			// Colour {1, 1, 1} is reserved for the nonrigid cluster. The choice if this value is not arbitrary,
			// meshes without colour will get white as default.
			const int32* NonRigidClusterIdxFound = ClusterSet.Find( NonRigidColour );

			// If not found, add an empty cluster for the non rigid id.
			OutNonRigidClusterIdx = NonRigidClusterIdxFound ? *NonRigidClusterIdxFound : ClusterData.Emplace();
		}
	}
	
	inline void FindBindingForCluster( 
			const Mesh* BaseMesh, FShapeMeshTree& ShapeMeshTree,
			const TArray<int32>& Cluster, FReshapeVertexBindingData& OutBindingData, float BindTolerance )
	{
		const UntypedMeshBufferIteratorConst ItPositionBase(BaseMesh->GetVertexBuffers(), MBS_POSITION);

		FBox3f ClusterBoundingBox(ForceInit);
		for ( const int32& V : Cluster)
		{
			ClusterBoundingBox += (ItPositionBase + V).GetAsVec3f();
		}

		FVector3f BoundPoint = ClusterBoundingBox.GetCenter();
		
		BindReshapePoint(ShapeMeshTree, BoundPoint, OutBindingData, BindTolerance);
	}

	inline TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> BindPhysicsBodies( 
			const PhysicsBody* PBody, FShapeMeshTree& ShapeMeshTree, const Mesh* pMesh, 
			bool bDeformAllPhysics, const TArray<string>& PhysicsToDeform )
	{
		TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> ReturnValue;

		TArray<int32>& BodiesToDeformIndices = ReturnValue.Get<1>();
	
		if (!bDeformAllPhysics)
		{ 
			BodiesToDeformIndices.Reserve(PhysicsToDeform.Num());
			const int32 NumBodies = PBody->GetBodyCount();
			for (int32 I = 0; I < NumBodies; ++I)
			{
				if ( PhysicsToDeform.Contains(string(PBody->GetBodyBoneName(I))) )
				{
					BodiesToDeformIndices.Add(I);
				}
			}
		}
		else
		{
			const int32 NumBodies = PBody->GetBodyCount();
			BodiesToDeformIndices.SetNumUninitialized(NumBodies);
			for (int32 I = 0; I < NumBodies; ++I)
			{
				BodiesToDeformIndices[I] = I;
			}
		}
		
		// Count how many poinst will be needed.
		const int32 NumBodiesToDeformIndices = BodiesToDeformIndices.Num();
		int32 NumPoints = 0;
		for ( const int32 I : BodiesToDeformIndices )
		{
			NumPoints += PBody->GetSphereCount(I) * 8;
			NumPoints += PBody->GetBoxCount(I) * 8;
			NumPoints += PBody->GetSphylCount(I) * 8;
			NumPoints += PBody->GetTaperedCapsuleCount(I) * 8;

			const int32 ConvexCount = PBody->GetConvexCount(I);
			for ( int C = 0; C < ConvexCount; ++C )
			{
				const FVector3f* VertData;
				int32 VertCount;
				const int32* IndicesData;
				int32 IndicesCount;
				FTransform3f Transform;
				PBody->GetConvex(I, C, VertData, VertCount, IndicesData, IndicesCount, Transform);

				NumPoints += VertCount;
			}
		}
		
		TArray<FVector3f> Points;
		Points.SetNumUninitialized(NumPoints);
	
		// Bone transform needs to be applied to the body's sample points so they are in mesh space.

		// Create a point soup to be deformed based on the shapes in the aggregate.
		// Currently using oriented bounding boxes corners for all shapes except convex which uses the deformed mesh directly.

		int32 AddedPoints = 0;
		for ( const int32 B : BodiesToDeformIndices )
		{
			const char* BoneName = PBody->GetBodyBoneName( B );
			int32 BoneIdx = pMesh->FindBonePose( BoneName );

			FTransform3f T = FTransform3f::Identity;
			if (BoneIdx > 0)
			{
				pMesh->GetBoneTransform(BoneIdx, T);
			}
			
			const int32 SphereCount = PBody->GetSphereCount(B);
			for ( int32 I = 0; I < SphereCount; ++I, AddedPoints += 8)
			{
				FVector3f P;
				float R;

				PBody->GetSphere(B, I, P, R);	

				Points[AddedPoints + 0] = T.TransformPosition( P + FVector3f(R, R, R)  );
				Points[AddedPoints + 1] = T.TransformPosition( P + FVector3f(-R, R, R)  );
				Points[AddedPoints + 2] = T.TransformPosition( P + FVector3f(-R, -R, R)  );
				Points[AddedPoints + 3] = T.TransformPosition( P + FVector3f(R, -R, R)  );

														  
				Points[AddedPoints + 4] = T.TransformPosition( P + FVector3f(R, R, -R) );
				Points[AddedPoints + 5] = T.TransformPosition( P + FVector3f(-R, R, -R) );
				Points[AddedPoints + 6] = T.TransformPosition( P + FVector3f(-R, -R, -R) );
				Points[AddedPoints + 7] = T.TransformPosition( P + FVector3f(R, -R, -R) );

			}

			const int32 BoxCount = PBody->GetBoxCount(B);
			for ( int32 I = 0; I < BoxCount; ++I, AddedPoints += 8)
			{
				FVector3f P;
				FQuat4f Q;
				FVector3f S;
			
				PBody->GetBox(B, I, P, Q, S);

				Points[AddedPoints + 0] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, S.Y, S.Z) ) );
				Points[AddedPoints + 1] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, S.Y, S.Z) ) );
				Points[AddedPoints + 2] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, -S.Y, S.Z) ) );
				Points[AddedPoints + 3] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, -S.Y, S.Z) ) );

														  
				Points[AddedPoints + 4] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, S.Y, -S.Z) ) );
				Points[AddedPoints + 5] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, S.Y, -S.Z) ) );
				Points[AddedPoints + 6] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, -S.Y, -S.Z) ) );
				Points[AddedPoints + 7] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, -S.Y, -S.Z) ) );
			}

			const int32 SphylCount = PBody->GetSphylCount(B);
			for ( int32 I = 0; I < SphylCount; ++I, AddedPoints += 8)
			{
				FVector3f P;
				FQuat4f Q;
				float R;
				float L;
			
				PBody->GetSphyl(B, I, P, Q, R, L);
				
				const FVector3f S = FVector3f(R, R, (L + R*2.0f) * 0.5f);

				Points[AddedPoints + 0] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, S.Y, S.Z) ) );
				Points[AddedPoints + 1] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, S.Y, S.Z) ) );
				Points[AddedPoints + 2] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, -S.Y, S.Z) ) );
				Points[AddedPoints + 3] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, -S.Y, S.Z) ) );

														  
				Points[AddedPoints + 4] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, S.Y, -S.Z) ) );
				Points[AddedPoints + 5] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, S.Y, -S.Z) ) );
				Points[AddedPoints + 6] = T.TransformPosition( P + Q.RotateVector(FVector3f(-S.X, -S.Y, -S.Z) ) );
				Points[AddedPoints + 7] = T.TransformPosition( P + Q.RotateVector(FVector3f(S.X, -S.Y, -S.Z) ) );
			}
	
			const int32 TaperedCapsuleCount = PBody->GetTaperedCapsuleCount(B);
			for ( int32 I = 0; I < TaperedCapsuleCount; ++I, AddedPoints += 8)
			{
				FVector3f P;
				FQuat4f Q;
				float R0;
				float R1;
				float L;
			
				PBody->GetTaperedCapsule(B, I, P, Q, R0, R1, L);
				
				const FVector3f ST = FVector3f(R0, R0, L*0.5f);
				const FVector3f SB = FVector3f(R1, R1, L*0.5f);
				
				Points[AddedPoints + 0] = T.TransformPosition( P + Q.RotateVector(FVector3f(ST.X, ST.Y, ST.Z + R0) ) );
				Points[AddedPoints + 1] = T.TransformPosition( P + Q.RotateVector(FVector3f(-ST.X, ST.Y, ST.Z + R0) ) );
				Points[AddedPoints + 2] = T.TransformPosition( P + Q.RotateVector(FVector3f(-ST.X, -ST.Y, ST.Z + R0) ) );
				Points[AddedPoints + 3] = T.TransformPosition( P + Q.RotateVector(FVector3f(ST.X, -ST.Y, ST.Z + R0) ) );

				Points[AddedPoints + 4] = T.TransformPosition( P + Q.RotateVector(FVector3f(SB.X, SB.Y, -SB.Z - R1) ) );
				Points[AddedPoints + 5] = T.TransformPosition( P + Q.RotateVector(FVector3f(-SB.X, SB.Y, -SB.Z - R1) ) );
				Points[AddedPoints + 6] = T.TransformPosition( P + Q.RotateVector(FVector3f(-SB.X, -SB.Y, -SB.Z - R1) ) );
				Points[AddedPoints + 7] = T.TransformPosition( P + Q.RotateVector(FVector3f(SB.X, -SB.Y, -SB.Z - R1) ) );
			}

			const int32 ConvexCount = PBody->GetConvexCount(B);
			for ( int32 I = 0; I < ConvexCount; ++I )
			{
				const FVector3f* VertexData;
				int32 VertexCount;
				const int32* IndicesData;
				int32 IndicesCount;
				FTransform3f ConvexT;
				
				PBody->GetConvex(B, I, VertexData, VertexCount, IndicesData, IndicesCount, ConvexT);
				
				TArrayView<const FVector3f> VertexDataView( VertexData, VertexCount );

				ConvexT = T * ConvexT;
				for ( const FVector3f& P : VertexDataView )
				{
					Points[AddedPoints++] = ConvexT.TransformPosition(P);
				}	
			}
		}
		
		TArray<FReshapeVertexBindingData>& PhysicsBodyBindData = ReturnValue.Get<0>();
		PhysicsBodyBindData.SetNumUninitialized( NumPoints );

		for (int32 I = 0; I < NumPoints; ++I)
		{
			BindReshapePoint(ShapeMeshTree, Points[I], PhysicsBodyBindData[I], 0.01f);
		}

		return ReturnValue;
	}

	
	inline TArray<FReshapeVertexBindingData> BindVerticesReshape(
			const Mesh* BaseMesh, FShapeMeshTree& ShapeMeshTree, bool bEnableRigidParts )	
	{
	
		UE::Geometry::FAxisAlignedBox3d ShapeAABBox = ShapeMeshTree.GetBoundingBox();

		const float BindValidityTolerance = ShapeAABBox.MaxDim() < 1.0 
										  ? UE_KINDA_SMALL_NUMBER
										  : static_cast<float>(ShapeAABBox.MaxDim()) * 1e-3f;

		TArray<TArray<int32>> VertexClusters;
		int32 NonRigidClusterIdx = -1;

		if (bEnableRigidParts)
		{
			FindRigidClusters( BaseMesh, VertexClusters, NonRigidClusterIdx );
		}
		
		// Find nearest shape triangle for each base mesh vertex		
		const int32 MeshVertexCount = BaseMesh->GetVertexCount();
		TArray<FReshapeVertexBindingData> BindData;
		{
			MUTABLE_CPUPROFILER_SCOPE(Project);

			BindData.SetNum( MeshVertexCount );

			UntypedMeshBufferIteratorConst ItPositionBase(BaseMesh->GetVertexBuffers(), MBS_POSITION);

			// Special case for non rigid parts

			// This indicates that we don't care about rigid parts,
			// only one clusters is found or there is no cluster data or rigid parts is disabled.
			if (NonRigidClusterIdx < 0)
			{
				const int32 VertexCount = BaseMesh->GetVertexCount();
				for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
				{
					FVector3f VertexPosition = (ItPositionBase + VertexIndex).GetAsVec3f();
					BindReshapePoint(ShapeMeshTree, VertexPosition, BindData[VertexIndex], BindValidityTolerance);
				}
			}
			else
			{
				TArray<int32>& NonRigidCluster = VertexClusters[NonRigidClusterIdx];
				const int32 NonRigidVertexCount = NonRigidCluster.Num();

				for (int32 I = 0; I < NonRigidVertexCount; ++I)
				{
					const int32 VertexIndex = NonRigidCluster[I];
					FVector3f VertexPosition = (ItPositionBase + VertexIndex).GetAsVec3f();
					BindReshapePoint(ShapeMeshTree, VertexPosition, BindData[VertexIndex], BindValidityTolerance);
				}

				// Remove data form the non rigid cluster so it ios not processed in the rigid parts binding step.
				NonRigidCluster.Reset();
			
				for (const TArray<int32>& RigidCluster : VertexClusters)
				{
					FReshapeVertexBindingData ClusterBinding;

					// Copy cluster binding to every vertex of the cluster.
					const int32 ClusterVertexCount = RigidCluster.Num();
					if (ClusterVertexCount)
					{
						FindBindingForCluster(BaseMesh, ShapeMeshTree, RigidCluster, ClusterBinding, BindValidityTolerance );
						for (int32 I = 0; I < ClusterVertexCount; ++I)
						{
							const int32 VertexIndex = RigidCluster[I];
							BindData[VertexIndex] = ClusterBinding;
						}
					}
				}
			}
		}

		return BindData;
	}

	inline TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> BindPose(
			const Mesh* Mesh, FShapeMeshTree& ShapeMeshTree, 
			bool bDeformAllBones, const TArray<string>& BonesToDeform )
	{
		UE::Geometry::FAxisAlignedBox3d ShapeAABBox = ShapeMeshTree.GetBoundingBox();

		const float BindValidityTolerance = ShapeAABBox.MaxDim() < 1.0 
										  ? UE_KINDA_SMALL_NUMBER
										  : static_cast<float>(ShapeAABBox.MaxDim()) * 1e-3f;
		
		TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> ReturnValue;

		TArray<FReshapeVertexBindingData>& SkeletonBindDataArray = ReturnValue.Get<0>();
		TArray<int32>& BoneIndices = ReturnValue.Get<1>();
		
		for ( int32 b = 0; b < Mesh->GetBonePoseCount(); ++b)
		{
			if (!bDeformAllBones)
			{
				bool bFound = false;
				for (int32 bd = 0; bd < BonesToDeform.Num(); ++bd)
				{
					if (Mesh->GetBonePoseName(b) == BonesToDeform[bd])
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					continue;
				}
			}

			const FTransform3f& t = Mesh->m_bonePoses[b].m_boneTransform;

			FVector3f Origin = t.GetLocation();

			FReshapeVertexBindingData SkeletonBindData;
			BindReshapePoint(ShapeMeshTree, Origin, SkeletonBindData, BindValidityTolerance);

			SkeletonBindDataArray.Add(SkeletonBindData);
			BoneIndices.Add(b);
		}

		return ReturnValue;
	}


	//---------------------------------------------------------------------------------------------
    //! Generate the mesh-shape binding data
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshBindShapeReshape(
			const Mesh* BaseMesh, const Mesh* ShapeMesh, 
			const TArray<string>& BonesToDeform, const TArray<string>& PhysicsToDeform,
			bool bDeformAllBones, bool bDeformAllPhyiscs,
			bool reshapeVertices, bool reshapeSkeleton, bool reshapePhysicsVolumes, bool bEnableRigidParts )
    {
		MUTABLE_CPUPROFILER_SCOPE(MeshBindShape);
		
		if (!BaseMesh)
		{
			return nullptr;
		}

		// Early out if nothing will be modified and the vertices discarted. return null in this
		// case indicating nothing has modified so the Base Mesh can be reused.
		const bool bSkeletonModification = BaseMesh->GetSkeleton() && reshapeSkeleton;
		const bool bPhysicsModification = BaseMesh->GetPhysicsBody() && reshapePhysicsVolumes;

		if (!reshapeVertices && !bSkeletonModification && !bPhysicsModification)
		{
			return nullptr;
		}

		if (!ShapeMesh)
		{
			return BaseMesh->Clone();
		}
	
		int32 ShapeVertexCount = ShapeMesh->GetVertexCount();
		int ShapeTriangleCount = ShapeMesh->GetFaceCount();
		if (!ShapeVertexCount || !ShapeTriangleCount)
		{
			return BaseMesh->Clone();
		}	

		FShapeMeshDescriptorBind ShapeMeshDescriptor;
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateVertexQueryData);	

			ShapeMeshDescriptor.Positions.SetNum(ShapeVertexCount);
			ShapeMeshDescriptor.Normals.SetNum(ShapeVertexCount);

			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItPosition(ShapeMesh->GetVertexBuffers(), MBS_POSITION);
			UntypedMeshBufferIteratorConst ItNormal(ShapeMesh->GetVertexBuffers(), MBS_NORMAL);
			for (int32 ShapeVertexIndex = 0; ShapeVertexIndex < ShapeVertexCount; ++ShapeVertexIndex)
			{
				FVector3f Position = ItPosition.GetAsVec3f();
				ShapeMeshDescriptor.Positions[ShapeVertexIndex] = static_cast<FVector3d>(Position);
				++ItPosition;
				
				FVector3f Normal = ItNormal.GetAsVec3f();
				ShapeMeshDescriptor.Normals[ShapeVertexIndex] = Normal;
				++ItNormal;
			}
		}
		// Generate the temp face query data for the shape
		// TODO: Index data copy may be saved in most cases.
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateTrianglesQueryData);	
			ShapeMeshDescriptor.Triangles.SetNum(ShapeTriangleCount);
			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItIndices(ShapeMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
			for (int32 TriangleIndex = 0; TriangleIndex < ShapeTriangleCount; ++TriangleIndex)
			{
				UE::Geometry::FIndex3i Triangle;
				Triangle.A = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.B = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.C = int(ItIndices.GetAsUINT32());
				++ItIndices;

				ShapeMeshDescriptor.Triangles[TriangleIndex] = Triangle;
			}	
		}

		FShapeMeshAdapter ShapeMeshAdapter(ShapeMeshDescriptor);

		constexpr bool bAutoBuildTree = false;
		FShapeMeshTree ShapeMeshTree(&ShapeMeshAdapter, bAutoBuildTree);	
		{	
			MUTABLE_CPUPROFILER_SCOPE(BuildShapeTree);
			
			ShapeMeshTree.Build();
		}
	
		Ptr<Mesh> Result;
		
		// If no vertices are needed, it is assumed we only want to reshape physics or skeleton
		// In that case, remove everything except physics bodies, the skeleton and pose.
		if (!reshapeVertices)
		{
			constexpr EMeshCloneFlags CloneFlags = 
					EMeshCloneFlags::WithSkeleton    | 
					EMeshCloneFlags::WithPhysicsBody | 
					EMeshCloneFlags::WithPoses;

			Result = BaseMesh->Clone(CloneFlags);
		}
		else
		{	
			Result = BaseMesh->Clone();
		}

		int32 BindingDataIndex = 0;
		if (reshapeVertices)
		{
			TArray<FReshapeVertexBindingData> VerticesBindData = BindVerticesReshape( BaseMesh, ShapeMeshTree, bEnableRigidParts );
			
			// Add the binding information to the mesh
			// \TODO: Check that there is no other binding data.
			// \TODO: Support specifying the binding data channel for multiple binding support.
			FMeshBufferSet& VB = Result->GetVertexBuffers();
			int NewBufferIndex = VB.GetBufferCount();
			VB.SetBufferCount(NewBufferIndex+1);

			FReshapeVertexBindingDataBufferDescriptor BufDesc(BindingDataIndex);
			VB.SetBuffer(NewBufferIndex, sizeof(FReshapeVertexBindingData), BufDesc.Channels, BufDesc.Semantics, BufDesc.SemanticIndices, BufDesc.Formats, BufDesc.Components);
			FMemory::Memcpy(VB.GetBufferData(NewBufferIndex), VerticesBindData.GetData(), VerticesBindData.Num() * sizeof(FReshapeVertexBindingData));
		}

		// Bind the skeleton bones
		// \TODO: Build bind data only for actually modified bones?
		if (reshapeSkeleton && (bDeformAllBones || BonesToDeform.Num()))
		{
			MUTABLE_CPUPROFILER_SCOPE(BindSkeleton);

			TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> SkeletonBindingData = 
					BindPose(Result.get(), ShapeMeshTree, bDeformAllBones, BonesToDeform);
			const TArray<FReshapeVertexBindingData>& SkeletonBindDataArray = SkeletonBindingData.Get<0>();
			const TArray<int32>& BoneIndices = SkeletonBindingData.Get<1>();

			check(BoneIndices.Num() == SkeletonBindDataArray.Num());

			const int32 NumBonesToDeform = SkeletonBindDataArray.Num();

			FMeshBufferSet SkeletonBuffer;
			SkeletonBuffer.SetBufferCount(2);
			SkeletonBuffer.SetElementCount(SkeletonBindDataArray.Num());

			FReshapeVertexBindingDataBufferDescriptor BufDesc(BindingDataIndex);
			SkeletonBuffer.SetBuffer(0, sizeof(FReshapeVertexBindingData), BufDesc.Channels, BufDesc.Semantics, BufDesc.SemanticIndices, BufDesc.Formats, BufDesc.Components);

			// Bone indices buffer
			MESH_BUFFER_SEMANTIC BoneSemantics[1] = { MBS_OTHER };
			MESH_BUFFER_FORMAT BoneFormats[1] = { MBF_INT32 };
			int BoneSemanticIndices[1] = { 0 };
			int BoneComponents[1] = { 1 };
			int BoneOffsets[1] = { 0 };

			SkeletonBuffer.SetBuffer(1, sizeof(int32), 1, BoneSemantics, BoneSemanticIndices, BoneFormats, BoneComponents, BoneOffsets);

			FMemory::Memcpy(SkeletonBuffer.GetBufferData(0), SkeletonBindDataArray.GetData(), NumBonesToDeform * sizeof(FReshapeVertexBindingData));
			FMemory::Memcpy(SkeletonBuffer.GetBufferData(1), BoneIndices.GetData(), NumBonesToDeform * sizeof(int32));

			Result->m_AdditionalBuffers.Emplace(EMeshBufferType::SkeletonDeformBinding, MoveTemp(SkeletonBuffer));
		}

		const PhysicsBody* PhysicsBody = Result->m_pPhysicsBody.get();
		if (reshapePhysicsVolumes && PhysicsBody && (bDeformAllPhyiscs || PhysicsToDeform.Num()))
		{
			MUTABLE_CPUPROFILER_SCOPE(BindPhysicsBody);

			TTuple<TArray<FReshapeVertexBindingData>, TArray<int32>> PhysicsBindingData = 
					BindPhysicsBodies(PhysicsBody, ShapeMeshTree, Result.get(), bDeformAllPhyiscs, PhysicsToDeform);
			const TArray<FReshapeVertexBindingData>& PhysicsBindDataArray = PhysicsBindingData.Get<0>();
			const TArray<int32>& DeformedBodyIndices = PhysicsBindingData.Get<1>();

			FMeshBufferSet PhysicsBodyBuffer;
			PhysicsBodyBuffer.SetBufferCount(1);
			PhysicsBodyBuffer.SetElementCount(PhysicsBindDataArray.Num());

			constexpr int32 BindDataIndex = 0; 
			FReshapeVertexBindingDataBufferDescriptor BufDesc(BindingDataIndex);
			PhysicsBodyBuffer.SetBuffer(0, sizeof(FReshapeVertexBindingData), BufDesc.Channels, BufDesc.Semantics, BufDesc.SemanticIndices, BufDesc.Formats, BufDesc.Components, BufDesc.Offsets);	
			FMemory::Memcpy(PhysicsBodyBuffer.GetBufferData(0), PhysicsBindDataArray.GetData(), PhysicsBindDataArray.Num() * sizeof(FReshapeVertexBindingData));

			// Bone indices buffer
			MESH_BUFFER_SEMANTIC BoneSemantics[1] = { MBS_OTHER };
			MESH_BUFFER_FORMAT BoneFormats[1] = { MBF_INT32 };
			int BoneSemanticIndices[1] = { 0 };
			int BoneComponents[1] = { 1 };
			int BoneOffsets[1] = { 0 };

			FMeshBufferSet PhysicsBodySelectionBuffer;
			PhysicsBodySelectionBuffer.SetBufferCount(1);
			PhysicsBodySelectionBuffer.SetElementCount(DeformedBodyIndices.Num());
			PhysicsBodySelectionBuffer.SetBuffer(0, sizeof(int32), 1, BoneSemantics, BoneSemanticIndices, BoneFormats, BoneComponents, BoneOffsets);	
			FMemory::Memcpy(PhysicsBodySelectionBuffer.GetBufferData(0), DeformedBodyIndices.GetData(), DeformedBodyIndices.Num() * sizeof(int32));

			Result->m_AdditionalBuffers.Emplace(EMeshBufferType::PhysicsBodyDeformBinding, MoveTemp(PhysicsBodyBuffer));
			Result->m_AdditionalBuffers.Emplace(EMeshBufferType::PhysicsBodyDeformSelection, MoveTemp(PhysicsBodySelectionBuffer));
		}
		
        return Result;
    }

	//---------------------------------------------------------------------------------------------
    //! Generate the mesh-shape binding data for ClipDeform operations
    //---------------------------------------------------------------------------------------------
	inline void BindClipDeformPointClosestProject(
			FShapeMeshTree& ShapeMeshTree,
			const FVector3f& Point, FClipDeformVertexBindingData& OutBindData, float ValidityTolerance )
	{
		const FShapeMeshDescriptorBind& ShapeMesh = ShapeMeshTree.GetMesh()->Mesh;
		
		OutBindData.S = 0.0f;
		OutBindData.T = 0.0f;
		OutBindData.Weight = 0.0f;
		OutBindData.Triangle = -1;

		double DistSqr = 0.0;

		int32 FoundIndex = ShapeMeshTree.FindNearestTriangle( FVector3d(Point), DistSqr );
			
		if (FoundIndex < 0)
		{
			return;
		}

		// Calculate the binding data of the base mesh vertex to its bound shape triangle
		UE::Geometry::FIndex3i TriangleIndices = ShapeMesh.Triangles[FoundIndex];

		// Project on the triangle, but using the vertex normals.
		// See reference implementation for details.
		UE::Geometry::FTriangle3f ShapeTriangle {
				(FVector3f)ShapeMesh.Positions[TriangleIndices.A],
				(FVector3f)ShapeMesh.Positions[TriangleIndices.B],
				(FVector3f)ShapeMesh.Positions[TriangleIndices.C] };

		const FVector3f ShapeTriangleNormal = ShapeTriangle.Normal();
		const FPlane4f VertexPlane(Point, ShapeTriangleNormal);
	
		const UE::Geometry::FTriangle3f TriangleVertexPlane {
				FMath::RayPlaneIntersection(ShapeTriangle.V[0], ShapeMesh.Normals[TriangleIndices.A], VertexPlane),
				FMath::RayPlaneIntersection(ShapeTriangle.V[1], ShapeMesh.Normals[TriangleIndices.B], VertexPlane),
				FMath::RayPlaneIntersection(ShapeTriangle.V[2], ShapeMesh.Normals[TriangleIndices.C], VertexPlane) };

		FVector3f Barycentric = TriangleVertexPlane.GetBarycentricCoords(Point);

		FVector3f InterpolatedShapeNormal = 
				ShapeMesh.Normals[TriangleIndices.A] * Barycentric.X + 
				ShapeMesh.Normals[TriangleIndices.B] * Barycentric.Y + 
				ShapeMesh.Normals[TriangleIndices.C] * Barycentric.Z;

		FVector3f BindPoint = ShapeTriangle.BarycentricPoint(Barycentric);
		FVector3f ProjectedToVertex = (Point - BindPoint);

		// Compute reprojection value to see if the binding is valid.
		const float InterpolatedNormalSizeSquared = InterpolatedShapeNormal.SizeSquared();
		const float InvInterpolatedNormalSizeSquared = InterpolatedNormalSizeSquared > SMALL_NUMBER ? 1.0f / InterpolatedNormalSizeSquared : 0.0f;	
		float D = FVector3f::DotProduct(ProjectedToVertex, InterpolatedShapeNormal) * InvInterpolatedNormalSizeSquared;
	
		const FVector3f ReprojectedPoint = BindPoint + InterpolatedShapeNormal * D;
		
		const FVector3f ReprojectedVector = ReprojectedPoint - Point;
		const float ErrorEstimate = (ReprojectedPoint - Point).GetAbsMax();
		
		// If within the tolerance, 1.0, otherwise linear falloff based on the tolerance

		// Arbitrary factor, a binding will be considered valid (with its corresponding weight) to ErrorFalloffFactor times the validity tolerance.
		constexpr float ErrorFalloffFactor = 4.0f;
		OutBindData.Weight = 1.0f - FMath::Clamp( (ErrorEstimate - ValidityTolerance) / (ValidityTolerance * ErrorFalloffFactor), 0.0f, 1.0f);
	
		OutBindData.S = Barycentric.Y;
		OutBindData.T = Barycentric.Z;

		// Only move points that bind outside the shape.
		OutBindData.Triangle = FVector3f::DotProduct(ShapeTriangleNormal, Point - BindPoint) < 0 ? -1 : FoundIndex;		
	}

	inline void BindClipDeformPointClosestToSurface( 
       		const FShapeMeshTree& ShapeMeshTree, 
			const FVector3f& Point, FClipDeformVertexBindingData& OutBindData)
	{
		const FShapeMeshDescriptorBind& ShapeMesh = ShapeMeshTree.GetMesh()->Mesh;
		
		OutBindData.S = 0.0f;
		OutBindData.T = 0.0f;
		OutBindData.Weight = 1.0f;
		OutBindData.Triangle = -1;

		const FVector3d P = FVector3d(Point);

		double DistSqr = 0;
		int32 FoundTriIndex = ShapeMeshTree.FindNearestTriangle(P, DistSqr);
	
		if (FoundTriIndex < 0)
		{
			return;
		}

		check( FoundTriIndex >= 0); 

		UE::Geometry::FIndex3i Triangle = ShapeMesh.Triangles[FoundTriIndex];
		UE::Geometry::FTriangle3d NearestShapeTriangle { 
				ShapeMesh.Positions[Triangle.A], 
				ShapeMesh.Positions[Triangle.B], 
				ShapeMesh.Positions[Triangle.C] };
	
		UE::Geometry::FDistPoint3Triangle3d Dist(P, NearestShapeTriangle);
		Dist.ComputeResult();

		const FVector3d BindPoint = NearestShapeTriangle.BarycentricPoint(Dist.TriangleBaryCoords);

		OutBindData.S = float(Dist.TriangleBaryCoords.Y);
		OutBindData.T = float(Dist.TriangleBaryCoords.Z); 

		// Only move points that bind outside the shape.
		OutBindData.Triangle = FVector3d::DotProduct(NearestShapeTriangle.Normal(), P - BindPoint) < 0 ? -1 : FoundTriIndex;
	}

	inline void BindClipDeformPointNormalProject( 
       		const FShapeMeshTree& ShapeMeshTree,
			const FVector3f& Point, const FVector3f& Normal, FClipDeformVertexBindingData& OutBindData)
	{
		const FShapeMeshDescriptorBind& ShapeMesh = ShapeMeshTree.GetMesh()->Mesh;

		OutBindData.S = 0.0f;
		OutBindData.T = 0.0f;
		OutBindData.Weight = 1.0f;
		OutBindData.Triangle = -1;
	
		const FRay3d NormalRay = FRay3d(FVector3d(Point), FVector3d(-Normal));
		double RayHitDist = 0.0;
		FVector3d RayHitBarycenticCoords = FVector3d::ZeroVector;
		int32 TriangleIndex = -1;

		const bool bHitFound = ShapeMeshTree.FindNearestHitTriangle(NormalRay, RayHitDist, TriangleIndex, RayHitBarycenticCoords);

		if (!bHitFound || TriangleIndex < 0)
		{
			return;
		}
		
		UE::Geometry::FIndex3i ShapeTriangleIndices = ShapeMesh.Triangles[TriangleIndex];
		UE::Geometry::FTriangle3d HitShapeTriangle {
				ShapeMesh.Positions[ShapeTriangleIndices.A],
				ShapeMesh.Positions[ShapeTriangleIndices.B],
				ShapeMesh.Positions[ShapeTriangleIndices.C] };
	
		OutBindData.S = float(RayHitBarycenticCoords.Y);
		OutBindData.T = float(RayHitBarycenticCoords.Z);

		OutBindData.Triangle = FVector3d::DotProduct(HitShapeTriangle.Normal(), -NormalRay.Direction) < 0 ? -1 : TriangleIndex;
	}
	
	inline TArray<FClipDeformVertexBindingData> BindVerticesClipDeform(
			const Mesh* BaseMesh, 
			FShapeMeshTree& ShapeMeshTree, 
			EShapeBindingMethod BindingMethod )
	{	
		UE::Geometry::FAxisAlignedBox3d ShapeAABBox = ShapeMeshTree.GetBoundingBox();

		const float BindValidityTolerance = ShapeAABBox.MaxDim() < 1.0 
										  ? UE_KINDA_SMALL_NUMBER
										  : static_cast<float>(ShapeAABBox.MaxDim()) * 1e-3f;

		// Find nearest shape triangle for each base mesh vertex		
		const int32 MeshVertexCount = BaseMesh->GetVertexCount();
		TArray<FClipDeformVertexBindingData> BindData;
		{
			MUTABLE_CPUPROFILER_SCOPE(ClipDeformBind);

			BindData.SetNum( MeshVertexCount );

			UntypedMeshBufferIteratorConst ItPositionBase(BaseMesh->GetVertexBuffers(), MBS_POSITION);
			UntypedMeshBufferIteratorConst ItNormalBase(BaseMesh->GetVertexBuffers(), MBS_NORMAL);

			EShapeBindingMethod ActualBindingMethod = BindingMethod == EShapeBindingMethod::ClipDeformNormalProject && !ItNormalBase.ptr() 
					? EShapeBindingMethod::ClipDeformClosestToSurface 
					: BindingMethod;

			switch ( ActualBindingMethod )
			{
			case EShapeBindingMethod::ClipDeformNormalProject:
			{
				check(ItNormalBase.ptr());
				for (int32 VertexIndex = 0; VertexIndex < MeshVertexCount; ++VertexIndex)
				{
					FVector3f VertexPosition = (ItPositionBase + VertexIndex).GetAsVec3f();
					FVector3f VertexNormal = (ItNormalBase + VertexIndex).GetAsVec3f();
					BindClipDeformPointNormalProject(ShapeMeshTree, VertexPosition, VertexNormal, BindData[VertexIndex]);
				}	

				break;
			}
			case EShapeBindingMethod::ClipDeformClosestToSurface:
			{
				for (int32 VertexIndex = 0; VertexIndex < MeshVertexCount; ++VertexIndex)
				{
					FVector3f VertexPosition = (ItPositionBase + VertexIndex).GetAsVec3f();
					BindClipDeformPointClosestToSurface(ShapeMeshTree, VertexPosition, BindData[VertexIndex]);
				}

				break;
			}
			case EShapeBindingMethod::ClipDeformClosestProject:
			{
				for (int32 VertexIndex = 0; VertexIndex < MeshVertexCount; ++VertexIndex)
				{
					FVector3f VertexPosition = (ItPositionBase + VertexIndex).GetAsVec3f();
					BindClipDeformPointClosestProject(ShapeMeshTree, VertexPosition, BindData[VertexIndex], BindValidityTolerance);
				}

				break;
			}

			default:
			{
				check(false);
			}
			};
		}

		return BindData;
	}

    inline MeshPtr MeshBindShapeClipDeform(const Mesh* BaseMesh, const Mesh* ShapeMesh, EShapeBindingMethod BindingMethod)
    {
		MUTABLE_CPUPROFILER_SCOPE(MeshBindShapeClipDeform);
		
		if (!BaseMesh)
		{
			return nullptr;
		}

		if (!ShapeMesh)
		{
			return BaseMesh->Clone();
		}
	
		int32 ShapeVertexCount = ShapeMesh->GetVertexCount();
		int ShapeTriangleCount = ShapeMesh->GetFaceCount();
		if (!ShapeVertexCount || !ShapeTriangleCount)
		{
			return BaseMesh->Clone();
		}	

		FShapeMeshDescriptorBind ShapeMeshDescriptor;
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateVertexQueryData);	

			ShapeMeshDescriptor.Positions.SetNum(ShapeVertexCount);
			ShapeMeshDescriptor.Normals.SetNum(ShapeVertexCount);

			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItPosition(ShapeMesh->GetVertexBuffers(), MBS_POSITION);
			UntypedMeshBufferIteratorConst ItNormal(ShapeMesh->GetVertexBuffers(), MBS_NORMAL);
			for (int32 ShapeVertexIndex = 0; ShapeVertexIndex < ShapeVertexCount; ++ShapeVertexIndex)
			{
				FVector3f Position = ItPosition.GetAsVec3f();
				ShapeMeshDescriptor.Positions[ShapeVertexIndex] = static_cast<FVector3d>(Position);
				++ItPosition;
				
				FVector3f Normal = ItNormal.GetAsVec3f();
				ShapeMeshDescriptor.Normals[ShapeVertexIndex] = Normal;
				++ItNormal;
			}
		}
		// Generate the temp face query data for the shape
		// TODO: Index data copy may be saved in most cases.
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateTrianglesQueryData);	
			ShapeMeshDescriptor.Triangles.SetNum(ShapeTriangleCount);
			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItIndices(ShapeMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
			for (int32 TriangleIndex = 0; TriangleIndex < ShapeTriangleCount; ++TriangleIndex)
			{
				UE::Geometry::FIndex3i Triangle;
				Triangle.A = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.B = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.C = int(ItIndices.GetAsUINT32());
				++ItIndices;

				ShapeMeshDescriptor.Triangles[TriangleIndex] = Triangle;
			}	
		}

		FShapeMeshAdapter ShapeMeshAdapter(ShapeMeshDescriptor);

		constexpr bool bAutoBuildTree = false;
		FShapeMeshTree ShapeMeshTree(&ShapeMeshAdapter, bAutoBuildTree);	
		{	
			MUTABLE_CPUPROFILER_SCOPE(BuildShapeTree);
			
			ShapeMeshTree.Build();
		}
	
		Ptr<Mesh> Result = BaseMesh->Clone();
		TArray<FClipDeformVertexBindingData> VerticesBindData = BindVerticesClipDeform(BaseMesh, ShapeMeshTree, BindingMethod);
			
		// Add the binding information to the mesh
		// \TODO: Check that there is no other binding data.
		// \TODO: Support specifying the binding data channel for multiple binding support.
		FMeshBufferSet& VB = Result->GetVertexBuffers();
		int NewBufferIndex = VB.GetBufferCount();
		VB.SetBufferCount(NewBufferIndex + 1);

		// \TODO: Multiple binding dataset support
		int32 BindingDataIndex = 0;
			
		FClipDeformVertexBindingDataBufferDescriptor BufDesc(BindingDataIndex);
		VB.SetBuffer(NewBufferIndex, sizeof(FClipDeformVertexBindingData), BufDesc.Channels, BufDesc.Semantics, BufDesc.SemanticIndices, BufDesc.Formats, BufDesc.Components, BufDesc.Offsets);
		FMemory::Memcpy(VB.GetBufferData(NewBufferIndex), VerticesBindData.GetData(), VerticesBindData.Num()*sizeof(FClipDeformVertexBindingData));
		
        return Result;
    }
}
