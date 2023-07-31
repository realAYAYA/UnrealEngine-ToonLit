// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/CollisionResolutionUtil.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/ParticleHandleFwd.h"


namespace Chaos
{
	template <typename T, int d>
	class TSphere;

	class FCapsule;

	class FConvex;

	template <typename T, int d>
	class TPlane;

	class FCollisionContext;
	class FHeightField;
	class FImplicitObject;
	class FPBDCollisionConstraint;
	class FTriangleMeshImplicitObject;


	namespace Collisions
	{

		//
		// Constraint API
		//

		/**
		 * @brief Update the contact manifold on the constraint
		 * @note Transforms are shape world-space transforms (not particle transforms)
		*/
		void CHAOS_API UpdateConstraint(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ShapeWorldTransform0, const FRigidTransform3& ShapeWorldTransform1, const FReal Dt);

		/**
		 * @brief Update the contact manifold on the constraint
		 * @note Transforms are shape world-space transforms (not particle transforms) at the start of the sweep. The end of the sweep are the transforms stored in the constraint as ShapeWorldTransform0/1
		*/
		bool CHAOS_API UpdateConstraintSwept(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ShapeStartWorldTransform0, const FRigidTransform3& ShapeStartWorldTransform1, const FReal Dt);

		/**
		 * @brief Determine the shape pair type for use in UpdateConstraints
		*/
		EContactShapesType CHAOS_API CalculateShapePairType(const FImplicitObject* Implicit0, const FBVHParticles* BVHParticles0, const FImplicitObject* Implicit1, const FBVHParticles* BVHParticles1, bool& bOutSwap);
		EContactShapesType CHAOS_API CalculateShapePairType(const EImplicitObjectType Implicit0Type, const EImplicitObjectType Implicit1Type, const bool bIsConvex0, const bool bIsConvex1, const bool bIsBVH0, const bool bIsBVH1, bool& bOutSwap);

		/**
		 * @brief Whether CCD should be enabled for a contact given the current particle velocities etc
		*/
		bool CHAOS_API ShouldUseCCD(const FGeometryParticleHandle* Particle0, const FVec3& DeltaX0, const FGeometryParticleHandle* Particle1, const FVec3& DeltaX1, FVec3& Dir, FReal& Length);

		// Update the constraint by re-running collision detection on the shape pair.
		// @todo(chaos): remove this and use UpdateConstraint instead
		template<ECollisionUpdateType UpdateType>
		void CHAOS_API UpdateConstraintFromGeometry(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ParticleTransform0, const FRigidTransform3& ParticleTransform1, const FReal Dt);
		// Create constraints for the particle pair. This could create multiple constraints: one for each potentially colliding shape pair in multi-shape particles.
		void CHAOS_API ConstructConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FBVHParticles* Simplicial0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FBVHParticles* Simplicial1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& Transform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& Transform1, const FReal CullDistance, const FReal Dt,const FCollisionContext& Context);

		// @todo(chaos): this is only called in tests - should it really be exposed?
		template<ECollisionUpdateType UpdateType>
		void UpdateLevelsetLevelsetConstraint(const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint);

		// Reset per-frame collision stat counters
		void CHAOS_API ResetChaosCollisionCounters();
	}
}
