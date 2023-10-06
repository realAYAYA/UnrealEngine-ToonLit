// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/CollisionOneShotManifoldsMeshShapes.h"

#include "Chaos/CollisionOneShotManifolds.h"
#include "Chaos/CollisionResolution.h"
#include "Chaos/Collision/CapsuleTriangleContactPoint.h"
#include "Chaos/Collision/ContactPointsMiscShapes.h"
#include "Chaos/Collision/ContactTriangles.h"
#include "Chaos/Collision/ConvexTriangleContactPoint.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Collision/SphereTriangleContactPoint.h"
#include "Chaos/Convex.h"
#include "Chaos/Defines.h"
#include "Chaos/Framework/UncheckedArray.h"
#include "Chaos/GJK.h"
#include "Chaos/HeightField.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/Transform.h"
#include "Chaos/Triangle.h"
#include "Chaos/Utilities.h"
#include "ChaosStats.h"

//UE_DISABLE_OPTIMIZATION

namespace Chaos
{
	extern bool bChaos_Collision_OneSidedTriangleMesh;
	extern bool bChaos_Collision_OneSidedHeightField;
	extern FRealSingle Chaos_Collision_TriMeshDistanceTolerance;
	extern FRealSingle Chaos_Collision_TriMeshPhiToleranceScale;

	extern bool bChaos_Collision_UseCapsuleTriMesh2;
	extern bool bChaos_Collision_UseConvexTriMesh2;

	namespace Collisions
	{

		inline FReal CalculateTriMeshPhiTolerance(const FReal CullDistance)
		{
			return Chaos_Collision_TriMeshPhiToleranceScale * CullDistance;
		}

		template <typename TriMeshType>
		void ConstructSphereTriangleMeshOneShotManifold(const TSphere<FReal, 3>& Sphere, const FRigidTransform3& SphereWorldTransform, const TriMeshType& TriangleMesh, const FRigidTransform3& TriMeshWorldTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// We only build one shot manifolds once
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(SphereWorldTransform.GetScale3D() == FVec3(1.0f, 1.0f, 1.0f));
			ensure(TriMeshWorldTransform.GetScale3D() == FVec3(1.0f, 1.0f, 1.0f));

			// @todo(chaos): support manifold maintenance
			Constraint.ResetActiveManifoldContacts();

			FContactPoint ContactPoint = SphereTriangleMeshContactPoint(Sphere, SphereWorldTransform, TriangleMesh, TriMeshWorldTransform, Constraint.GetCullDistance());
			if (ContactPoint.Phi < Constraint.GetCullDistance())
			{
				Constraint.AddOneshotManifoldContact(ContactPoint);
			}
		}

		void ConstructSphereHeightFieldOneShotManifold(const TSphere<FReal, 3>& Sphere, const FRigidTransform3& SphereTransform, const FHeightField& Heightfield, const FRigidTransform3& HeightfieldTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// We only build one shot manifolds once
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(SphereTransform.GetScale3D() == FVec3(1.0f, 1.0f, 1.0f));
			ensure(HeightfieldTransform.GetScale3D() == FVec3(1.0f, 1.0f, 1.0f));

			FContactPoint ContactPoint = SphereHeightFieldContactPoint(Sphere, SphereTransform, Heightfield, HeightfieldTransform, Constraint.GetCullDistance());
			if (ContactPoint.Phi < Constraint.GetCullDistance())
			{
				Constraint.AddOneshotManifoldContact(ContactPoint);
			}
		}

		/**
		 * @brief Generate a manifold between a convex shape and a single triangle
		 * Templated so we can specialize for some shape types
		*/
		template<typename ConvexType>
		void GenerateConvexTriangleOneShotManifold(const ConvexType& Convex, const FTriangle& Triangle, const FReal CullDistance, FContactPointManifold& OutContactPoints)
		{
			if (bChaos_Collision_UseConvexTriMesh2)
			{
				ConstructConvexTriangleOneShotManifold2(Convex, Triangle, CullDistance, OutContactPoints);
			}
			else
			{
				ConstructPlanarConvexTriangleOneShotManifold(Convex, Triangle, CullDistance, OutContactPoints);
			}
		}

		template<>
		void GenerateConvexTriangleOneShotManifold<FImplicitCapsule3>(const FImplicitCapsule3& Capsule, const FTriangle& Triangle, const FReal CullDistance, FContactPointManifold& OutContactPoints)
		{
			if (bChaos_Collision_UseCapsuleTriMesh2)
			{
				ConstructCapsuleTriangleOneShotManifold2(Capsule, Triangle, CullDistance, OutContactPoints);
			}
			else
			{
				ConstructCapsuleTriangleOneShotManifold(Capsule, Triangle, CullDistance, OutContactPoints);
			}
		}

		template<>
		void GenerateConvexTriangleOneShotManifold<FImplicitSphere3>(const FImplicitSphere3& Sphere, const FTriangle& Triangle, const FReal CullDistance, FContactPointManifold& OutContactPoints)
		{
			ConstructSphereTriangleOneShotManifold(Sphere, Triangle, CullDistance, OutContactPoints);
		}

		/**
		* @brief Create a minimized set of contact points between a convex polyhedron (box, convex) and a non-convex mesh (trimesh, heightfield)
		*
		* @tparam ConvexType any convex type (Sphere, Capsule, Box, Convex, possibly with a scaled or instanced)
		* @tparam MeshType any triangle mesh type (HeightField or TriangleMesh, without a scaled or instanced wrapper)
		* @param MeshQueryBounds Triangles overlapping this box will be tested. Should be in the space of the mesh.
		* @param MeshToConvexTransform The transform from Mesh space to Convex space. This low-level convex-triangle collision detection is performed in Convex space.
		*
		* @see ConstructPlanarConvexTriMeshOneShotManifoldImp, ConstructPlanarConvexHeightfieldOneShotManifoldImp
		*/
		template<typename ConvexType, typename MeshType>
		void GenerateConvexMeshContactPoints(const ConvexType& Convex, const MeshType& Mesh, const FAABB3& MeshQueryBounds, const FRigidTransform3& MeshToConvexTransform, const FReal CullDistance, FContactTriangleCollector& MeshContacts)
		{
			FContactPointManifold TriangleManifoldPoints;

			// Loop over all the triangles, build a manifold and add the points to the total manifold
			// NOTE: contact points will be in the space of the convex until the end of the function when we convert into shape local space
			Mesh.VisitTriangles(MeshQueryBounds, MeshToConvexTransform, [&](const FTriangle& Triangle, const int32 TriangleIndex, const int32 VertexIndex0, const int32 VertexIndex1, const int32 VertexIndex2)
			{
				// Generate the manifold for this triangle
				TriangleManifoldPoints.Reset();
				GenerateConvexTriangleOneShotManifold(Convex, Triangle, CullDistance, TriangleManifoldPoints);

				if (TriangleManifoldPoints.Num() > 0)
				{
					// Add the points into the main contact array
					// NOTE: The Contacts' FaceIndices will be an index into the ContactTriangles not the original tri mesh (this will get mapped back to the mesh index below)
					MeshContacts.AddTriangleContacts(MakeArrayView(TriangleManifoldPoints.begin(), TriangleManifoldPoints.Num()), Triangle, TriangleIndex, VertexIndex0, VertexIndex1, VertexIndex2, CullDistance);
				}
			});

			// Reduce contacts to a minimum manifold and transform contact data back into shape-local space
			MeshContacts.ProcessContacts(MeshToConvexTransform);
		}

		/**
		 * @brief Used by all the convex types to generate a manifold against any mesh type
		*/
		template<typename ConvexType, typename MeshType>
		void ConstructConvexMeshOneShotManifold(const ConvexType& Convex, const FRigidTransform3& ConvexTransform, const MeshType& Mesh, const FRigidTransform3& MeshTransform, const FVec3& MeshScale, const FReal CullDistance, FContactTriangleCollector& MeshContacts)
		{
			FRigidTransform3 MeshToConvexTransform = MeshTransform.GetRelativeTransformNoScale(ConvexTransform);
			MeshToConvexTransform.SetScale3D(MeshScale);

			// @todo(chaos): add Convex.CalculateInverseTransformed bounds with scale support (to optimize sphere and capsule)
			const FAABB3 MeshQueryBounds = Convex.BoundingBox().InverseTransformedAABB(MeshToConvexTransform).Thicken(CullDistance);

			// Create the minimal manifold from all the overlapping triangles
			GenerateConvexMeshContactPoints(Convex, Mesh, MeshQueryBounds, MeshToConvexTransform, CullDistance, MeshContacts);
		}

		void ConstructQuadraticConvexTriMeshOneShotManifold(const FImplicitObject& Quadratic, const FRigidTransform3& QuadraticTransform, const FImplicitObject& InMesh, const FRigidTransform3& MeshTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(QuadraticTransform.GetScale3D() == FVec3(1));
			ensure(MeshTransform.GetScale3D() == FVec3(1));

			// Unwrap the tri mesh (remove Scaled or Instanced) and get the scale
			FVec3 MeshScale;
			FReal MeshMargin;	// Not used - will be zero for meshes
			const FTriangleMeshImplicitObject* Mesh = UnwrapImplicit<FTriangleMeshImplicitObject>(InMesh, MeshScale, MeshMargin);
			check(Mesh != nullptr);

			const FReal CullDistance = Constraint.GetCullDistance();
			const FReal PhiTolerance = CalculateTriMeshPhiTolerance(CullDistance);
			const FReal DistanceTolerance = Chaos_Collision_TriMeshDistanceTolerance;
			FContactTriangleCollector MeshContacts(bChaos_Collision_OneSidedTriangleMesh, PhiTolerance, DistanceTolerance, QuadraticTransform);

			if (const FImplicitSphere3* Sphere = Quadratic.template GetObject<FImplicitSphere3>())
			{
				ConstructConvexMeshOneShotManifold(*Sphere, QuadraticTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const FImplicitCapsule3* Capsule = Quadratic.template GetObject<FImplicitCapsule3>())
			{
				ConstructConvexMeshOneShotManifold(*Capsule, QuadraticTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else
			{
				check(false);
			}

			Constraint.SetOneShotManifoldContacts(MeshContacts.GetContactPoints());
		}

		void ConstructQuadraticConvexHeightFieldOneShotManifold(const FImplicitObject& Quadratic, const FRigidTransform3& QuadraticTransform, const FHeightField& Mesh, const FRigidTransform3& MeshTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// We only build one shot manifolds once
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(QuadraticTransform.GetScale3D() == FVec3(1));
			ensure(MeshTransform.GetScale3D() == FVec3(1));

			const FReal CullDistance = Constraint.GetCullDistance();
			const FReal PhiTolerance = CalculateTriMeshPhiTolerance(CullDistance);
			const FReal DistanceTolerance = Chaos_Collision_TriMeshDistanceTolerance;
			FContactTriangleCollector MeshContacts(bChaos_Collision_OneSidedHeightField, PhiTolerance, DistanceTolerance, QuadraticTransform);

			if (const FImplicitSphere3* Sphere = Quadratic.template GetObject<FImplicitSphere3>())
			{
				ConstructConvexMeshOneShotManifold(*Sphere, QuadraticTransform, Mesh, MeshTransform, FVec3(1), CullDistance, MeshContacts);
			}
			else if (const FImplicitCapsule3* Capsule = Quadratic.template GetObject<FImplicitCapsule3>())
			{
				ConstructConvexMeshOneShotManifold(*Capsule, QuadraticTransform, Mesh, MeshTransform, FVec3(1), CullDistance, MeshContacts);
			}
			else
			{
				check(false);
			}

			Constraint.SetOneShotManifoldContacts(MeshContacts.GetContactPoints());
		}

		/**
		* @brief Populate the Constraint with a manifold of contacts between a Convex and a TriangleMesh
		* @param Convex A convex polyhedron (Box, Convex) that may be wrapped in Scaled or Instanced
		* @param InMesh A TriangleMesh ImplicitObject that may be wrapped in Scaled or Instaned
		*/
		void ConstructPlanarConvexTriMeshOneShotManifold(const FImplicitObject& Convex, const FRigidTransform3& ConvexTransform, const FImplicitObject& InMesh, const FRigidTransform3& MeshTransform, FPBDCollisionConstraint& Constraint)
		{
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(ConvexTransform.GetScale3D() == FVec3(1));
			ensure(MeshTransform.GetScale3D() == FVec3(1));

			// Unwrap the tri mesh (remove Scaled or Instanced) and get the scale
			FVec3 MeshScale;
			FReal MeshMargin;	// Not used - will be zero for meshes
			const FTriangleMeshImplicitObject* Mesh = UnwrapImplicit<FTriangleMeshImplicitObject>(InMesh, MeshScale, MeshMargin);
			check(Mesh != nullptr);

			const FReal CullDistance = Constraint.GetCullDistance();
			const FReal PhiTolerance = CalculateTriMeshPhiTolerance(CullDistance);
			const FReal DistanceTolerance = Chaos_Collision_TriMeshDistanceTolerance;
			FContactTriangleCollector MeshContacts(bChaos_Collision_OneSidedTriangleMesh, PhiTolerance, DistanceTolerance, ConvexTransform);

			if (const FImplicitBox3* RawBox = Convex.template GetObject<FImplicitBox3>())
			{
				ConstructConvexMeshOneShotManifold(*RawBox, ConvexTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const TImplicitObjectScaled<FImplicitConvex3>* ScaledConvex = Convex.template GetObject<TImplicitObjectScaled<FImplicitConvex3>>())
			{
				ConstructConvexMeshOneShotManifold(*ScaledConvex, ConvexTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const TImplicitObjectInstanced<FImplicitConvex3>* InstancedConvex = Convex.template GetObject<TImplicitObjectInstanced<FImplicitConvex3>>())
			{
				ConstructConvexMeshOneShotManifold(*InstancedConvex, ConvexTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const FImplicitConvex3* RawConvex = Convex.template GetObject<FImplicitConvex3>())
			{
				ConstructConvexMeshOneShotManifold(*RawConvex, ConvexTransform, *Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else
			{
				check(false);
			}

			Constraint.SetOneShotManifoldContacts(MeshContacts.GetContactPoints());
		}

		/**
		* @brief Populate the Constraint with a manifold of contacts between a Convex and a HeightField
		* @param Convex A convex polyhedron (Box, Convex) that may be wrapped in Scaled or Instanced
		*/
		void ConstructPlanarConvexHeightFieldOneShotManifold(const FImplicitObject& Convex, const FRigidTransform3& ConvexTransform, const FHeightField& Mesh, const FRigidTransform3& MeshTransform, FPBDCollisionConstraint& Constraint)
		{
			// All convexes are pre-scaled, or wrapped in TImplicitObjectScaled
			ensure(ConvexTransform.GetScale3D() == FVec3(1));
			ensure(MeshTransform.GetScale3D() == FVec3(1));

			const FVec3 MeshScale = FVec3(1);	// Scale is built into heightfield

			const FReal CullDistance = Constraint.GetCullDistance();
			const FReal PhiTolerance = CalculateTriMeshPhiTolerance(CullDistance);
			const FReal DistanceTolerance = Chaos_Collision_TriMeshDistanceTolerance;
			FContactTriangleCollector MeshContacts(bChaos_Collision_OneSidedHeightField, PhiTolerance, DistanceTolerance, ConvexTransform);

			if (const FImplicitBox3* RawBox = Convex.template GetObject<FImplicitBox3>())
			{
				ConstructConvexMeshOneShotManifold(*RawBox, ConvexTransform, Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const TImplicitObjectScaled<FImplicitConvex3>* ScaledConvex = Convex.template GetObject<TImplicitObjectScaled<FImplicitConvex3>>())
			{
				ConstructConvexMeshOneShotManifold(*ScaledConvex, ConvexTransform, Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const TImplicitObjectInstanced<FImplicitConvex3>* InstancedConvex = Convex.template GetObject<TImplicitObjectInstanced<FImplicitConvex3>>())
			{
				ConstructConvexMeshOneShotManifold(*InstancedConvex, ConvexTransform, Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else if (const FImplicitConvex3* RawConvex = Convex.template GetObject<FImplicitConvex3>())
			{
				ConstructConvexMeshOneShotManifold(*RawConvex, ConvexTransform, Mesh, MeshTransform, MeshScale, CullDistance, MeshContacts);
			}
			else
			{
				check(false);
			}

			Constraint.SetOneShotManifoldContacts(MeshContacts.GetContactPoints());
		}
	}
}

