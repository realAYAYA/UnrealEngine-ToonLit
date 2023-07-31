// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Convex.h"
#include "Chaos/Defines.h"
#include "Chaos/Transform.h"

namespace Chaos
{
	class FHeightField;
	namespace Collisions
	{
		void ConstructSphereSphereOneShotManifold(
			const TSphere<FReal, 3>& Sphere1,
			const FRigidTransform3& Convex1Transform, //world
			const TSphere<FReal, 3>& Sphere2,
			const FRigidTransform3& Convex2Transform, //world
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructSpherePlaneOneShotManifold(
			const TSphere<FReal, 3>& Sphere,
			const FRigidTransform3& SphereTransform,
			const TPlane<FReal, 3>& Plane,
			const FRigidTransform3& PlaneTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructSphereBoxOneShotManifold(
			const TSphere<FReal, 3>& Sphere,
			const FRigidTransform3& SphereTransform,
			const FImplicitBox3& Box,
			const FRigidTransform3& BoxTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructSphereCapsuleOneShotManifold(
			const TSphere<FReal, 3>& Sphere,
			const FRigidTransform3& SphereTransform,
			const FCapsule& Capsule,
			const FRigidTransform3& CapsuleTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructSphereConvexManifold(
			const TSphere<FReal, 3>& Sphere,
			const FRigidTransform3& SphereTransform,
			const FImplicitObject3& Convex,
			const FRigidTransform3& ConvexTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		template <typename TriMeshType>
		void ConstructSphereTriangleMeshOneShotManifold(
			const TSphere<FReal, 3>& Sphere,
			const FRigidTransform3& SphereWorldTransform,
			const TriMeshType& TriangleMesh,
			const FRigidTransform3& TriMeshWorldTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructSphereHeightFieldOneShotManifold(
			const TSphere<FReal, 3>& Sphere, 
			const FRigidTransform3& SphereTransform, 
			const FHeightField& Heightfield, 
			const FRigidTransform3& HeightfieldTransform, 
			const FReal Dt, FPBDCollisionConstraint& Constraint);

		void ConstructCapsuleCapsuleOneShotManifold(
			const FCapsule& CapsuleA,
			const FRigidTransform3& CapsuleATransform,
			const FCapsule& CapsuleB,
			const FRigidTransform3& CapsuleBTransform,
			const FReal Dt,
			FPBDCollisionConstraint& Constraint);

		void ConstructCapsuleTriMeshOneShotManifold(
			const FCapsule& Capsule, 
			const FRigidTransform3& CapsuleWorldTransform, 
			const FImplicitObject& TriangleMesh, 
			const FRigidTransform3& TriMeshWorldTransform, 
			const FReal Dt, 
			FPBDCollisionConstraint& Constraint);

		void ConstructCapsuleHeightFieldOneShotManifold(
			const FCapsule& Capsule, 
			const FRigidTransform3& CapsuleTransform, 
			const FHeightField& HeightField, 
			const FRigidTransform3& HeightFieldTransform, 
			const FReal Dt, 
			FPBDCollisionConstraint& Constraint);

		/**
		 * @brief Build the contact manifold between a Planar Convex shape and a TriMesh
		 * @param Convex an ImplicitObject that must be a box, convex, or a wrapper around it. Asserts if not
		 * @param TriMesh an ImplicitObject that must be FTriangleMeshImplicitObject or a wrapper around it. Asserts if not
		*/
		void ConstructPlanarConvexTriMeshOneShotManifold(
			const FImplicitObject& Convex, 
			const FRigidTransform3& ConvexTransform, 
			const FImplicitObject& TriangleMesh, 
			const FRigidTransform3& TriangleMeshTransform, 
			FPBDCollisionConstraint& Constraint);


		/**
		 * @brief Build the contact manifold between a Planar Convex shape and a HeightField
		 * @param Convex an ImplicitObject that must be a box, convex, or a wrapper around it. Asserts if not
		*/
		void ConstructPlanarConvexHeightFieldOneShotManifold(
			const FImplicitObject& Convex, 
			const FRigidTransform3& ConvexTransform, 
			const FHeightField& HeightField, 
			const FRigidTransform3& TriangleMeshTransform, 
			FPBDCollisionConstraint& Constraint);

	}
}