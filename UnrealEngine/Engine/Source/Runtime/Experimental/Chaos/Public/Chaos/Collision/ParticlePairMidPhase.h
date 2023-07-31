// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"

#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Collision/CollisionKeys.h"
#include "Chaos/Collision/CollisionVisitor.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/ObjectPool.h"
#include "Chaos/ParticleHandleFwd.h"
#include "ProfilingDebugging/CsvProfiler.h"

	// Whether to use a pool for MidPhases
#define CHAOS_COLLISION_OBJECTPOOL_ENABLED 1

namespace Chaos
{
	class FCollisionContext;
	class FCollisionConstraintAllocator;
	class FParticlePairMidPhase;
	class FPBDCollisionConstraints;
	class FPerShapeData;
	class FSingleShapePairCollisionDetector;

#if CHAOS_COLLISION_OBJECTPOOL_ENABLED 
	// Collision and MidPhases are stored in an ObjectPool (if CHAOS_COLLISION_OBJECTPOOL_ENABLED is set). @see FCollisionConstraintAllocator
	using FPBDCollisionConstraintPool = TObjectPool<FPBDCollisionConstraint>;
	using FParticlePairMidPhasePool = TObjectPool<FParticlePairMidPhase>;
	using FPBDCollisionConstraintDeleter = TObjectPoolDeleter<FPBDCollisionConstraintPool>;
	using FPBDCollisionConstraintPtr = TUniquePtr<FPBDCollisionConstraint, FPBDCollisionConstraintDeleter>;
	using FParticlePairMidPhaseDeleter = TObjectPoolDeleter<FParticlePairMidPhasePool>;
	using FParticlePairMidPhasePtr = TUniquePtr<FParticlePairMidPhase, FParticlePairMidPhaseDeleter>;
#else 
	using FPBDCollisionConstraintPtr = TUniquePtr<FPBDCollisionConstraint>;
	using FParticlePairMidPhasePtr = TUniquePtr<FParticlePairMidPhase>;
#endif

	/**
	 * @brief Handles collision detection for a pair of simple shapes (i.e., not compound shapes)
	 * 
	 * @note this is not used for collisions involving Unions that require a recursive collision test.
	 * @see FMultiShapePairCollisionDetector
	*/
	class CHAOS_API FSingleShapePairCollisionDetector
	{
	public:
		using FCollisionsArray = TArray<FPBDCollisionConstraint*, TInlineAllocator<1>>;

		FSingleShapePairCollisionDetector(
			FGeometryParticleHandle* InParticle0,
			const FPerShapeData* InShape0,
			FGeometryParticleHandle* InParticle1,
			const FPerShapeData* InShape1,
			const EContactShapesType InShapePairType, 
			FParticlePairMidPhase& MidPhase);
		FSingleShapePairCollisionDetector(FSingleShapePairCollisionDetector&& R);
		FSingleShapePairCollisionDetector(const FSingleShapePairCollisionDetector& R) = delete;
		FSingleShapePairCollisionDetector& operator=(const FSingleShapePairCollisionDetector& R) = delete;
		~FSingleShapePairCollisionDetector();

		const FPBDCollisionConstraint* GetConstraint() const{ return Constraint.Get(); }
		FPBDCollisionConstraint* GetConstraint() { return Constraint.Get(); }
		const FGeometryParticleHandle* GetParticle0() const { return Particle0; }
		FGeometryParticleHandle* GetParticle0() { return Particle0; }
		const FGeometryParticleHandle* GetParticle1() const { return Particle1; }
		FGeometryParticleHandle* GetParticle1() { return Particle1; }
		const FPerShapeData* GetShape0() const { return Shape0; }
		const FPerShapeData* GetShape1() const { return Shape1; }

		/**
		 * @brief Have we run collision detection since this Epoch (inclusive)
		*/
		inline bool IsUsedSince(const int32 Epoch) const
		{
			return Constraint.IsValid() && (LastUsedEpoch >= Epoch);
		}

		/**
		 * @brief Perform a bounds check and run the narrow phase if necessary
		 * @return The number of collisions constraints that were activated
		*/
		int32 GenerateCollision(
			const FReal CullDistance,
			const FReal Dt,
			const FCollisionContext& Context);

		/**
		 * @brief Generate a SweptConstraint as long as AABBs overlap
		 * @return The number of collisions constraints that were activated
		*/
		int32 GenerateCollisionCCD(
			const bool bEnableCCDSweep,
			const FReal CullDistance,
			const FReal Dt,
			const FCollisionContext& Context);

		/**
		 * @brief Reactivate the constraint
		 * @parame SleepEpoch The tick on which the particle went to sleep.
		 * Only constraints that were active when the particle went to sleep should be reactivated.
		*/
		void WakeCollision(const int32 SleepEpoch, const int32 CurrentEpoch);

		/**
		 * @brief Set the collision from the parameter and activate it
		 * This is used by the Resim restore functionality
		*/
		void SetCollision(const FPBDCollisionConstraint& Constraint, const FCollisionContext& Context);

	private:
		int32 GenerateCollisionImpl(
			const FReal CullDistance, 
			const FReal Dt,
			const FCollisionContext& Context);
			
		int32 GenerateCollisionCCDImpl(
			const bool bEnableCCDSweep,
			const FReal CullDistance,
			const FReal Dt,
			const FCollisionContext& Context);

		int32 GenerateCollisionProbeImpl(
			const FReal CullDistance, 
			const FReal Dt,
			const FCollisionContext& Context);

		/**
		 * @brief Whether the two shapes are separated by less than CullDistance (i.e., we should run the narrow phase).
		 * Also returns true if bounds checking is disabled (globally or for this pair)
		*/
		bool DoBoundsOverlap(const FReal CullDistance, const int32 CurrentEpoch);

		/**
		 * @brief Create a constraint
		*/
		void CreateConstraint(const FReal CullDistance, const FCollisionContext& Context);

		FParticlePairMidPhase& MidPhase;
		FPBDCollisionConstraintPtr Constraint;
		FGeometryParticleHandle* Particle0;
		FGeometryParticleHandle* Particle1;
		const FPerShapeData* Shape0;
		const FPerShapeData* Shape1;
		FRealSingle SphereBoundsCheckSize;
		int32 LastUsedEpoch;
		EContactShapesType ShapePairType;
		union FFlags
		{
			FFlags() : Bits(0) {}
			struct 
			{
				uint8 bEnableAABBCheck : 1;
				uint8 bEnableOBBCheck0 : 1;
				uint8 bEnableOBBCheck1 : 1;
				uint8 bEnableManifoldUpdate : 1;
				uint8 bIsProbe : 1;
			};
			uint8 Bits;
		} Flags;
	};


	/**
	 * @brief A collision detector for shape pairs which are containers of other shapes
	 * This is primarily used by clustered particles that leave their shapes in a Union
	 * rather than flattening into the particle's ShapesArray.
	*/
	class CHAOS_API FMultiShapePairCollisionDetector
	{
	public:
		FMultiShapePairCollisionDetector(
			FGeometryParticleHandle* InParticle0,
			const FPerShapeData* InShape0,
			FGeometryParticleHandle* InParticle1,
			const FPerShapeData* InShape1,
			FParticlePairMidPhase& MidPhase);
		FMultiShapePairCollisionDetector(FMultiShapePairCollisionDetector&& R);
		FMultiShapePairCollisionDetector(const FMultiShapePairCollisionDetector& R) = delete;
		FMultiShapePairCollisionDetector& operator=(const FMultiShapePairCollisionDetector& R) = delete;
		~FMultiShapePairCollisionDetector();

		/**
		 * @brief Perform a bounds check and run the narrow phase if necessary
		 * @return The number of collisions constraints that were activated
		*/
		int32 GenerateCollisions(
			const FReal CullDistance,
			const FReal Dt,
			const FCollisionContext& Context);

		/**
		 * @brief Callback from the narrow phase to create a collision constraint for this particle pair.
		 * We should never be asked for a collision for a different particle pair, but the 
		 * implicit objects may be children of the root shape.
		*/
		FPBDCollisionConstraint* FindOrCreateConstraint(
			FGeometryParticleHandle* InParticle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* BVHParticles0,
			const FRigidTransform3& ShapeRelativeTransform0,
			FGeometryParticleHandle* InParticle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* BVHParticles1,
			const FRigidTransform3& ShapeRelativeTransform1,
			const FReal CullDistance,
			const EContactShapesType ShapePairType,
			const bool bUseManifold,
			const FCollisionContext& Context);

		/**
		 * @brief FindOrCreateConstraint for swept constraints 
		*/
		FPBDCollisionConstraint* FindOrCreateSweptConstraint(
			FGeometryParticleHandle* InParticle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* BVHParticles0,
			const FRigidTransform3& ShapeRelativeTransform0,
			FGeometryParticleHandle* InParticle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* BVHParticles1,
			const FRigidTransform3& ShapeRelativeTransform1,
			const FReal CullDistance,
			const EContactShapesType ShapePairType,
			const FCollisionContext& Context);

		/**
		 * @brief Reactivate the constraint
		 * @parame SleepEpoch The tick on which the particle went to sleep.
		 * Only constraints that were active when the particle went to sleep should be reactivated.
		*/
		void WakeCollisions(const int32 SleepEpoch, const int32 CurrentEpoch);

		/**
		 * @brief Visit all the collisions
		*/
		template<typename TLambda>
		ECollisionVisitorResult VisitCollisions(const TLambda& Visitor, const bool bOnlyActive = true);

		/**
		 * @brief Visit all the collisions
		*/
		template<typename TLambda>
		ECollisionVisitorResult VisitConstCollisions(const TLambda& Visitor, const bool bOnlyActive = true) const;

	private:
		FPBDCollisionConstraint* FindConstraint(const FCollisionParticlePairConstraintKey& Key);

		FPBDCollisionConstraint* CreateConstraint(
			FGeometryParticleHandle* Particle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* BVHParticles0,
			const FRigidTransform3& ShapeRelativeTransform0,
			FGeometryParticleHandle* Particle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* BVHParticles1,
			const FRigidTransform3& ShapeRelativeTransform1,
			const FReal CullDistance,
			const EContactShapesType ShapePairType,
			const bool bInUseManifold,
			const FCollisionParticlePairConstraintKey& Key,
			const FCollisionContext& Context);

		int32 ProcessNewConstraints(const FCollisionContext& Context);
		void PruneConstraints(const int32 CurrentEpoch);

		FParticlePairMidPhase& MidPhase;
		TMap<uint32, FPBDCollisionConstraintPtr> Constraints;
		TArray<FPBDCollisionConstraint*> NewConstraints;
		FGeometryParticleHandle* Particle0;
		FGeometryParticleHandle* Particle1;
		const FPerShapeData* Shape0;
		const FPerShapeData* Shape1;
	};

	/**
	 * @brief Produce collisions for a particle pair
	 * A FParticlePairMidPhase object is created for every particle pair whose bounds overlap. It is 
	 * responsible for building a set of potentially colliding shape pairs and running collision
	 * detection on those pairs each tick.
	 * 
	 * Most particles have a array of shapes, but not all shapes participate in collision detection
	 * (some are query-only). The cached shape pair list prevents us from repeatesdly testing the
	 * filters of shape pairs that can never collide.
	 * 
	 * @note Geometry collections and clusters do not have arrays of simple shapes. Clustered particles
	 * typically have a Union as one of the root shapes. In this case we do not attempt to cache the
	 * potentially colliding shape pair set, and must process the unions every tick.
	 * 
	 * @note The lifetime of these objects is handled entirely by the CollisionConstraintAllocator. 
	 * Nothing outside of the CollisionConstraintAllocator should hold a pointer to the detector 
	 * or any constraints it creates for more than the duration of the tick.
	*/
	class CHAOS_API FParticlePairMidPhase
	{
	public:
		FParticlePairMidPhase();

		UE_NONCOPYABLE(FParticlePairMidPhase);

		~FParticlePairMidPhase();

		/**
		 * @brief Set up the midphase based on the SHapesArrays of the two particles
		 * Only intended to be called once right after constructor. We don't do this work in
		 * the constructor so that we can reduce the time that the lock is held when allocating
		 * new MidPhases.
		*/
		void Init(
			FGeometryParticleHandle* InParticle0,
			FGeometryParticleHandle* InParticle1,
			const FCollisionParticlePairKey& InKey,
			const FCollisionContext& Context);

		inline FGeometryParticleHandle* GetParticle0() { return Particle0; }

		inline FGeometryParticleHandle* GetParticle1() { return Particle1; }

		inline const FCollisionParticlePairKey& GetKey() const { return Key; }

		inline bool IsValid() const { return (Particle0 != nullptr) && (Particle1 != nullptr); }

		// Prefetch the memory in this class for later use
		inline void CachePrefetch()
		{
			FPlatformMisc::PrefetchBlock(this, sizeof(*this));
		}

		/**
		 * @brief Have we run collision detection since this Epoch (inclusive)
		*/
		inline bool IsUsedSince(const int32 Epoch) const
		{
			return (LastUsedEpoch >= Epoch);
		}


		/**
		 * @brief Whether the particle pair is sleeping and therefore contacts should not be culled (they will be reused on wake)
		*/
		inline bool IsSleeping() const { return Flags.bIsSleeping; }

		/**
		 * @brief Update the sleeping state
		 * If this switches the state to Awake, it will reactivate any collisions between the particle pair that
		 * were active when they went to sleep.
		*/
		void SetIsSleeping(const bool bInIsSleeping, const int32 CurrentEpoch);

		bool IsInConstraintGraph() const;

		/**
		 * @brief Destroy all collisions and prevent this midphasae from being used any more. Called when one of its particles is destoyed.
		 * It will be culled at the next Prune in the CollisionConstraintAllocator. We don't delete it immediately so that we don't
		 * have to remove it from either Particle's ParticleCollisions array (which is O(N) and unnecessary when the particles are being destroyed)
		*/
		void DetachParticle(FGeometryParticleHandle* Particle);

		/**
		 * @brief Delete all cached data and collisions. Should be called when a particle changes its shapes
		*/
		void Reset();

		/**
		 * @brief Create collision constraints for all colliding shape pairs
		*/
		void GenerateCollisions(
			const FReal CullDistance,
			const FReal Dt,
			const FCollisionContext& Context);

		/**
		 * @brief Copy a collision and activate it
		 * This is used by the Resim system to restore saved colisions. If there is already a matching constraint
		 * it will be overwritten, otherwise a new constraint will be added.
		*/
		void InjectCollision(const FPBDCollisionConstraint& Constraint, const FCollisionContext& Context);

		/**
		 * @brief Call a lambda on each active collision constraint
		 * This includes sleeping constraints, but skips constraints that are were not used on the last awake tick
		 * but are still kept around as an optimization.
		*/
		template<typename TLambda>
		ECollisionVisitorResult VisitCollisions(const TLambda& Visitor, const bool bOnlyActive = true);


		/**
		 * @brief Call a lambda on each active collision constraint
		 */
		template<typename TLambda>
		ECollisionVisitorResult VisitConstCollisions(const TLambda& Visitor, const bool bOnlyActive = true) const;

		/**
		 * @brief Cookie for use by FParticleCollisions
		*/
		int32 GetParticleCollisionsIndex(FGeometryParticleHandle* InParticle) const
		{
			check((InParticle == Particle0) || (InParticle == Particle1));
			if (InParticle == Particle0)
			{
				return ParticleCollisionsIndex0;
			}
			else
			{
				return ParticleCollisionsIndex1;
			}
		}

		/**
		 * @brief Cookie for use by FParticleCollisions
		*/
		void SetParticleCollisionsIndex(FGeometryParticleHandle* InParticle, const int32 InIndex)
		{
			check((InParticle == Particle0) || (InParticle == Particle1));
			if (InParticle == Particle0)
			{
				ParticleCollisionsIndex0 = InIndex;
			}
			else
			{
				ParticleCollisionsIndex1 = InIndex;
			}
		}

	private:
		/**
		 * @brief Build the list of potentially colliding shape pairs.
		 * This is all the shape pairs in the partilces' shapes arrays that pass the collision filter.
		*/
		void BuildDetectors();

		/**
		 * @brief Add the shape pair to the list of potentially colliding pairs
		*/
		void TryAddShapePair(
			const FPerShapeData* Shape0, 
			const FPerShapeData* Shape1);

		/**
		 * @brief Decide whether we should have CCD enabled on this constraint
		 * @return true if CCD is enabled this tick, false otherwise
		 * This may return false, even for collisions on CCD-enabled bodies when the bodies are moving slowly
		*/
		bool ShouldEnableCCD(const FReal Dt);

		void InitThresholds();

		FGeometryParticleHandle* Particle0; // 8 bytes
		FGeometryParticleHandle* Particle1; // 8 bytes
		// A number based on the size of the dynamic objects used to scale cull distance
		FRealSingle CullDistanceScale; // 4 bytes

		union FFlags
		{
			FFlags() : Bits(0) {}
			struct
			{
				uint32 bIsCCD : 1;
				uint32 bIsInitialized : 1;
				uint32 bIsSleeping : 1;
			};
			uint32 Bits;
		} Flags; // 4 bytes

		FCollisionParticlePairKey Key; //8 bytes

		int32 LastUsedEpoch;  //4 bytes
		int32 NumActiveConstraints; // 4 bytes

		// Indices into the arrays of collisions on the particles. This is a cookie for use by FParticleCollisions
		int32 ParticleCollisionsIndex0; // 4 bytes
		int32 ParticleCollisionsIndex1; // 4 bytes

		TArray<FMultiShapePairCollisionDetector> MultiShapePairDetectors; // 16 bytes
		TArray<FSingleShapePairCollisionDetector, TInlineAllocator<1>> ShapePairDetectors; //88 bytes
	};
}
