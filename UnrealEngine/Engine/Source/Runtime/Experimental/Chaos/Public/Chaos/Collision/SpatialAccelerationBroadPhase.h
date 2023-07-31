// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/CollisionConstraintFlags.h"
#include "Chaos/Collision/CollisionContext.h"
#include "Chaos/Collision/StatsData.h"
#include "Chaos/ISpatialAccelerationCollection.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/Capsule.h"
#include "ChaosStats.h"
#include "Chaos/EvolutionResimCache.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/AABBTree.h"

namespace Chaos
{
	template <typename TPayloadType, typename T, int d>
	class ISpatialAcceleration;
	class IResimCacheBase;

	// Data collected from the spatial partitioning system
	class FSimOverlapVisitorEntry
	{
	public:
		FSimOverlapVisitorEntry()
			: Particle(nullptr)
		{}

		FSimOverlapVisitorEntry(const FAccelerationStructureHandle& Handle)
			: Particle(Handle.GetGeometryParticleHandle_PhysicsThread())
		{
		}

		FGeometryParticleHandle* Particle;
	};

	/**
	 * A visitor for the spatial partitioning system used to build the set of objects overlapping a bounding box.
	 */
	struct FSimOverlapVisitor
	{
		TArray<FSimOverlapVisitorEntry>& Intersections;

		FSimOverlapVisitor(FGeometryParticleHandle* ParticleHandle, const FCollisionFilterData& InSimFilterData, TArray<FSimOverlapVisitorEntry>& InIntersections)
			: Intersections(InIntersections)
			, SimFilterData(InSimFilterData)
			, ParticleUniqueIdx(ParticleHandle ? ParticleHandle->UniqueIdx() : FUniqueIdx(0))
			, AccelerationHandle(ParticleHandle)
		{
		}

		bool VisitOverlap(const TSpatialVisitorData<FAccelerationStructureHandle>& Instance)
		{
			Intersections.Emplace(FSimOverlapVisitorEntry(Instance.Payload));
			return true;
		}

		bool VisitSweep(TSpatialVisitorData<FAccelerationStructureHandle>, FQueryFastData& CurData)
		{
			check(false);
			return false;
		}

		bool VisitRaycast(TSpatialVisitorData<FAccelerationStructureHandle>, FQueryFastData& CurData)
		{
			check(false);
			return false;
		}

		const void* GetQueryData() const { return nullptr; }

		const void* GetSimData() const { return &SimFilterData; }

		bool ShouldIgnore(const TSpatialVisitorData<FAccelerationStructureHandle>& Instance) const
		{
			return Instance.Payload.UniqueIdx() == ParticleUniqueIdx;
		}
		/** Return a pointer to the payload on which we are querying the acceleration structure */
		const void* GetQueryPayload() const
		{
			return &AccelerationHandle;
		}

		bool HasBlockingHit() const
		{
			return false;
		}

	private:
		FCollisionFilterData SimFilterData;
		FUniqueIdx ParticleUniqueIdx; // unique id of the particle visiting, used to skip self intersection as early as possible

		/** Handle to be stored to retrieve the payload on which we are querying the acceleration structure*/
		FAccelerationStructureHandle AccelerationHandle;
	};

	/**
	 * A broad phase that iterates over particle and uses a spatial acceleration structure to output
	 * potentially overlapping SpatialAccelerationHandles.
	 */
	class FSpatialAccelerationBroadPhase
	{
	public:
		using FAccelerationStructure = ISpatialAcceleration<FAccelerationStructureHandle, FReal, 3>;

		FSpatialAccelerationBroadPhase(const FPBDRigidsSOAs& InParticles)
			: Particles(InParticles)
			, SpatialAcceleration(nullptr)
		{
		}

		void SetSpatialAcceleration(const FAccelerationStructure* InSpatialAcceleration)
		{
			SpatialAcceleration = InSpatialAcceleration;
		}

		/**
		 * Generate all overlapping pairs and pass them to the narrow phase.
		 */
		void ProduceOverlaps(
			FReal Dt, 
			FCollisionConstraintAllocator* Allocator,
			const FCollisionDetectorSettings& Settings,
			IResimCacheBase* ResimCache
			)
		{
			SCOPE_CYCLE_COUNTER(STAT_Collisions_SpatialBroadPhase);

			if (!ensure(SpatialAcceleration))
			{
				// Must call SetSpatialAcceleration
				return;
			}

			if (const auto AABBTree = SpatialAcceleration->template As<TAABBTree<FAccelerationStructureHandle, TAABBTreeLeafArray<FAccelerationStructureHandle>>>())
			{
				ProduceOverlaps(Dt, *AABBTree, Allocator, Settings, ResimCache);
			}
			else if (const auto BV = SpatialAcceleration->template As<TBoundingVolume<FAccelerationStructureHandle>>())
			{
				ProduceOverlaps(Dt, *BV, Allocator, Settings, ResimCache);
			}
			else if (const auto AABBTreeBV = SpatialAcceleration->template As<TAABBTree<FAccelerationStructureHandle, TBoundingVolume<FAccelerationStructureHandle>>>())
			{
				ProduceOverlaps(Dt, *AABBTreeBV, Allocator, Settings, ResimCache);
			}
			else if (const auto Collection = SpatialAcceleration->template As<ISpatialAccelerationCollection<FAccelerationStructureHandle, FReal, 3>>())
			{
				Collection->PBDComputeConstraintsLowLevel(Dt, *this, Allocator, Settings, ResimCache);
			}
			else
			{
				check(false);  //question: do we want to support a dynamic dispatch version?
			}
		}
		
		/** @brief This function is the outer loop of collision detection. It loops over the
		 * particles view and do the broadphase + narrowphase collision detection
		 * @param OverlapView View to consider for the outer loop
		 * @param Dt Current simulation time step
		 * @param InSpatialAcceleration Spatial acceleration (AABB, bounding volumes...) to be used for broadphase collision detection
		 * @param NarrowPhase Narrowphase collision detection that will be executed on each potential pairs coming from the broadphase detection
		 * */

		template<bool bNeedsResim, bool bOnlyRigid, typename ViewType, typename SpatialAccelerationType>
		void ComputeParticlesOverlaps(
			ViewType& OverlapView, 
			FReal Dt,
			const SpatialAccelerationType& InSpatialAcceleration, 
			FCollisionConstraintAllocator* Allocator,
			const FCollisionDetectorSettings& Settings)
		{
			// A set of contexts, one for each worker thread, containing the allocation buffers etc.
			// NOTE: Ideally contexts would not resize after the initial creation, but actually it might because
			// of the way the ParticleView parallel-for is implemented. If the view is a set of sub-views, we
			// will get a parallel-for per sub-view which will have different numbers of particles and may
			// therefore require a different number of workers.
			// Fortunately, we only get calls to ContextCreator for a sub-view (with a possibly different NumWorkers)
			// after the previous sub-view's parallel-for has completed, so we don't need to worry about context addresses 
			// changing (we could use TChunkedArray, or allocate contexts on the heap if we ever need to handle that).
			TArray<FCollisionContext> Contexts;
			auto ContextCreator = [Allocator, &Settings, &Contexts](const int32 WorkerIndex, const int32 NumWorkers) -> int32
			{
				Contexts.SetNum(NumWorkers);

				// Retrieve the midphase/collision allocator for this context
				Allocator->SetMaxContexts(NumWorkers);
				FCollisionContextAllocator* ContextAllocator = Allocator->GetContextAllocator(WorkerIndex);

				// Build the context for the worker
				Contexts[WorkerIndex].SetSettings(Settings);
				Contexts[WorkerIndex].SetAllocator(ContextAllocator);

				return WorkerIndex;
			};

			// Detect collisions for a single particle versus all others
			auto ParticleCollisionGenerator = [this, Dt, &InSpatialAcceleration, &Contexts](auto& Particle1, const int32 ActiveIdxIdx, const int32 ContextIndex)
			{
				ProduceParticleOverlaps<bNeedsResim, bOnlyRigid>(Dt, Particle1.Handle(), InSpatialAcceleration, Contexts[ContextIndex]);
			};

			OverlapView.ParallelFor(ContextCreator, ParticleCollisionGenerator, bDisableCollisionParallelFor);
		}

		template<typename T_SPATIALACCELERATION>
		void ProduceOverlaps(
			FReal Dt, 
			const T_SPATIALACCELERATION& InSpatialAcceleration, 
			FCollisionConstraintAllocator* Allocator,
			const FCollisionDetectorSettings& Settings,
			IResimCacheBase* ResimCache
			)
		{
			// Select the set of particles that we loop over in the outer collision detection loop.
			// The goal is to detection all required collisions (dynamic-vs-everything) while not
			// visiting pairs that cannot collide (e.g., kinemtic-kinematic, or kinematic-sleeping for
			// stationary kinematics)
			const bool bDisableParallelFor = bDisableCollisionParallelFor;
			auto* EvolutionResimCache = static_cast<FEvolutionResimCache*>(ResimCache);
			const bool bResimSkipCollision = ResimCache && ResimCache->IsResimming();
			if(!bResimSkipCollision)
			{
				const TParticleView<TPBDRigidParticles<FReal, 3>>& DynamicSleepingView = Particles.GetNonDisabledDynamicView();
				const TParticleView<TKinematicGeometryParticles<FReal, 3>>& DynamicMovingKinematicView = Particles.GetActiveDynamicMovingKinematicParticlesView();

				// Usually we ignore sleeping particles in the outer loop and iterate over awake-dynamics and moving-kinematics. 
				// However, for scenes with a very large number of moving kinematics, it is faster to loop over awake-dynamics
				// and sleeping-dynamics, even though this means we visit sleeping pairs.
				if(DynamicSleepingView.Num() < DynamicMovingKinematicView.Num())
				{
					ComputeParticlesOverlaps<false, true>(DynamicSleepingView, Dt, InSpatialAcceleration, Allocator, Settings);
				}
				else
				{
					ComputeParticlesOverlaps<false, false>(DynamicMovingKinematicView, Dt, InSpatialAcceleration, Allocator, Settings);
				}
			}
			else
			{
				const TParticleView<TGeometryParticles<FReal, 3>>& DesyncedView = ResimCache->GetDesyncedView();
				
				ComputeParticlesOverlaps<true,false>(DesyncedView, Dt, InSpatialAcceleration, Allocator, Settings);
			}
		}

		FIgnoreCollisionManager& GetIgnoreCollisionManager() { return IgnoreCollisionManager; }

	private:

		template<bool bIsResimming, bool bOnlyRigid, typename T_SPATIALACCELERATION>
		void ProduceParticleOverlaps(
		    FReal Dt,
		    FGeometryParticleHandle* Particle1,
		    const T_SPATIALACCELERATION& InSpatialAcceleration,
			const FCollisionContext& Context)
		{
			TArray<FSimOverlapVisitorEntry> PotentialIntersections;

			bool bIsKinematic1 = true;
			bool bIsDynamicAwake1 = false;
			bool bIsDynamicAsleep1 = false;
			bool bUseIgnoreCollisionManager1 = false;
			bool bDisabled1 = false;
			int32 CollisionGroup1 = 0;
			const FPBDRigidParticleHandle* Rigid1 = Particle1->CastToRigidParticle();
			if (Rigid1 != nullptr)
			{
				bIsKinematic1 = Rigid1->IsKinematic();
				bIsDynamicAsleep1 = !bIsKinematic1 && Rigid1->IsSleeping();
				bIsDynamicAwake1 = !bIsKinematic1 && !bIsDynamicAsleep1;
				bUseIgnoreCollisionManager1 = Rigid1->HasCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
				bDisabled1 = Rigid1->Disabled();
				CollisionGroup1 = Rigid1->CollisionGroup();
			}

			// @todo(chaos): We should not be getting disabled particles, but we do from GeometryCollections
			//check(!bDisabled1);
			if (bDisabled1)
			{
				return;
			}

			// Skip particles we are not interested in
			// @todo(chaos): This should already be handled by selecting the appropriate particle view in the calling function. GeometryCollections?
			const bool bHasValidState = bOnlyRigid ? (bIsDynamicAwake1 || bIsDynamicAsleep1) : (bIsDynamicAwake1 || bIsKinematic1);
			if (!bHasValidState && !bIsResimming)
			{
				return;
			}
			
			const bool bBody1Bounded = Particle1->HasBounds();
			if (bBody1Bounded)
			{
				//SCOPE_CYCLE_COUNTER(STAT_Collisions_AABBTree);
				// @todo(chaos): cache this on the particle?
				FCollisionFilterData ParticleSimData;
				FAccelerationStructureHandle::ComputeParticleSimFilterDataFromShapes(*Particle1, ParticleSimData);

				const FAABB3 Box1 = Particle1->WorldSpaceInflatedBounds();

				{
					PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, DetectCollisions_BroadPhase);
					FSimOverlapVisitor OverlapVisitor(Particle1, ParticleSimData, PotentialIntersections);
					InSpatialAcceleration.Overlap(Box1, OverlapVisitor);
				}
			}
			else
			{
				//SCOPE_CYCLE_COUNTER(STAT_Collisions_AABBTree);
				const auto& GlobalElems = InSpatialAcceleration.GlobalObjects();
				PotentialIntersections.Reserve(GlobalElems.Num());

				for (auto& Elem : GlobalElems)
				{
					PotentialIntersections.Add(Elem.Payload);
				}
			}

			//SCOPE_CYCLE_COUNTER(STAT_Collisions_Filtering);
			const int32 NumPotentials = PotentialIntersections.Num();
			TArray<FParticlePairMidPhase*> MidPhases;
			MidPhases.Reserve(NumPotentials);
			for (int32 PotentialIndex = 0; PotentialIndex < NumPotentials; ++PotentialIndex)
			{
				FGeometryParticleHandle* Particle2 = PotentialIntersections[PotentialIndex].Particle;
				if (Particle1 == Particle2)
				{
					continue;
				}

				bool bIsKinematic2 = true;
				bool bIsDynamicAwake2 = false;
				bool bIsDynamicAsleep2 = false;
				bool bUseIgnoreCollisionManager2 = false;
				bool bDisabled2 = false;
				int32 CollisionGroup2 = 0;
				const FPBDRigidParticleHandle* Rigid2 = Particle2->CastToRigidParticle();
				if (Rigid2 != nullptr)
				{
					bIsKinematic2 = Rigid2->IsKinematic();
					bIsDynamicAsleep2 = !bIsKinematic2 && Rigid2->IsSleeping();
					bIsDynamicAwake2 = !bIsKinematic2 && !bIsDynamicAsleep2;
					bUseIgnoreCollisionManager2 = Rigid2->HasCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
					bDisabled2 = Rigid2->Disabled();
					CollisionGroup2 = Rigid2->CollisionGroup();
				}

				// @todo(chaos): This should not be happening if the disabled particles are removed from the active particles list, but GeometryCollection particles leave there there
				//check(!bDisabled2);
				if (bDisabled2)
				{
					continue;
				}

				// At least one particle needs to be dynamic to generate a collision response
				if (bIsKinematic1 && bIsKinematic2)
				{
					continue;
				}
				check((Rigid1 != nullptr) || (Rigid2 != nullptr));

				// Is this particle interaction governed by the IgnoreCollisionManager? If so, check to see if interaction is allowed
				if (bUseIgnoreCollisionManager1 || bUseIgnoreCollisionManager2)
				{
					if (IgnoreCollisionManager.IgnoresCollision(Particle1->UniqueIdx(), Particle2->UniqueIdx()))
					{
						continue;
					}
				}

				// CollisionGroups are used by geometry collections for high-level collision filtering
				// CollisionGroup == 0 : Collide_With_Everything
				// CollisionGroup == INDEX_NONE : Disabled collisions
				// CollisionGroup1 != CollisionGroup2 : Disabled collisions (if other conditions not met)
				if (CollisionGroup1 == INDEX_NONE || CollisionGroup2 == INDEX_NONE)
				{
					continue;
				}
				if ((CollisionGroup1 != 0) && (CollisionGroup2 != 0) && (CollisionGroup1 != CollisionGroup2))
				{
					continue;
				}

				// In both cases (resim or not) we will generate (1) dynamic-(sleeping,kinematic(steady+moving),static) pairs + (2) sleeping-moving kinematic ones
				// Sleeping particles could collide with dynamic ones but these collisions are already handled in case 1
				// Sleeping particles won't collide with static or steady kinematic particles since neither are moving
				// Sleeping particles will collide against moving kinematic particles
				bool bAcceptParticlePair = false;
				if ((bIsDynamicAwake1 && !bIsDynamicAwake2) || (bIsDynamicAsleep1 && bIsKinematic2))
				{
					bAcceptParticlePair = true;
				}
					
				// Used to determine a winner in cases where we will visit particle pairs in both orders
				const bool bIsParticle1Preferred = AreParticlesInPreferredOrder(Particle1, Particle2);
					
				if(!bIsResimming)
				{
					// Normally (not resimming) we iterate over dynamic and asleep|kinematic particles, so:
					// - Particle1 is dynamic, asleep OR kinematic
					// - Particle2 may be static, kinematic, dynamic, asleep

					// If Particle1 is non dynamic but particle 2 is dynamic, the case should already be handled by (1)
					if(!bIsDynamicAwake1 && bIsDynamicAwake2)
					{
						bAcceptParticlePair = false;
					}
					// If Particle1 and Particle2 are dynamic we validate the pair if particle1 has higher ID to discard duplicates since we will visit twice the pairs
					else if(bIsDynamicAwake1 && bIsDynamicAwake2)
					{
						bAcceptParticlePair = bIsParticle1Preferred;
					}
					// If Particle1 is kinematic we should in theory discard the pairs against sleeping particles
					// since the sleeping-kinematic case has been validated in (2). But Particle1 is asleep OR kinematic so
					// when entering this condition we are sure that we enver entered (2). It is why we validate the kinematic-sleeping pairs as well
					else if(bIsKinematic1 && bIsDynamicAsleep2)
					{
						bAcceptParticlePair = true;
					}
				}
				else
				{
					// When resimming we iterate over "desynced" particles which may be kinematic so:
					// - Particle1 is always desynced
					// - Particle2 may also be desynced, in which case we will also visit the opposite ordering regardless of dynamic/kinematic status
					// - Particle1 may be static, kinematic, dynamic, asleep
					// - Particle2 may be static, kinematic, dynamic, asleep
					// 
					// Even though Particle1 may be kinematic when resimming, we want to create the contacts in the original order (i.e., dynamic first)
					// 
					const bool bIsParticle2Desynced = bIsResimming && (Particle2->SyncState() == ESyncState::HardDesync);

					// If Particle1 is non dynamic but particle 2 is dynamic, the case should already be handled by (1) for
					// the desynced dynamic - synced/desynced (static,kinematic,asleep) pairs. But we still need to process
					// the desynced (static,kinematic,asleep) against the synced dynamic since this case have not been handled by (1)
					if(!bIsDynamicAwake1 && bIsDynamicAwake2)
					{
						bAcceptParticlePair = !bIsParticle2Desynced;
					}
					// If Particle1 and Particle2 are dynamic we validate the pair if particle1 has higher ID to discard duplicates since we will visit twice the pairs
					// We validate the pairs as well if the particle 2 is synced since  we will never visit the opposite order (we only iterate over desynced particles)
					else if(bIsDynamicAwake1 && bIsDynamicAwake2)
					{
						bAcceptParticlePair = bIsParticle1Preferred || !bIsParticle2Desynced;
					}
					// If Particle1 is kinematic we are discarding the pairs against sleeping desynced particles since the case has been handled by (2)
					else if(bIsKinematic1 && bIsDynamicAsleep2)
					{
						bAcceptParticlePair = !bIsParticle2Desynced;
					}
				}
					
				if (!bAcceptParticlePair)
				{
					continue;
				}

				// Since particles can change type (between Kinematic and Dynamic) we may visit them in different orders at different times, but if we allow
				// that it would break Resim and constraint re-use. Also, if only one particle is Dynamic, we want it in first position. This isn't a strtct 
				// requirement but some downstream systems assume this is true (e.g., CCD, TriMesh collision).
				const bool bSwapOrder = ShouldSwapParticleOrder((bIsDynamicAwake1 || bIsDynamicAsleep1), (bIsDynamicAwake2 || bIsDynamicAsleep2), bIsParticle1Preferred);
				FGeometryParticleHandle* ParticleA = bSwapOrder ? Particle2 : Particle1;
				FGeometryParticleHandle* ParticleB = bSwapOrder ? Particle1 : Particle2;

				// NOTE: This searches the array of existing midphases on the particle specified in the last parameter. The assumption
				// here is that Particle1 is more likely to have less collision on it because it is dynamic. Kinematics can often be
				// large and may collide with many objects. Maybe we should consider using size instead.
				FParticlePairMidPhase* MidPhase = Context.GetAllocator()->GetMidPhase(ParticleA, ParticleB, Particle1, Context);
				if (MidPhase != nullptr)
				{
					MidPhases.Add(MidPhase);
				}
			}

			// Prefetch initial set of MidPhases
			const int32 PrefetchLookahead = 4;
			const int32 NumMidPhases = MidPhases.Num();
			for (int32 Index = 0; Index < NumMidPhases && Index < PrefetchLookahead; Index++)
			{
				MidPhases[Index]->CachePrefetch();
			}

			for (int32 Index = 0; Index < NumMidPhases; Index++)
			{
				// Prefetch next MidPhase
				if (Index + PrefetchLookahead < NumMidPhases)
				{
					MidPhases[Index + PrefetchLookahead]->CachePrefetch();
				}

				// Run MidPhase + NarrowPhase
				MidPhases[Index]->GenerateCollisions(Context.GetSettings().BoundsExpansion, Dt, Context);
			}

			PHYSICS_CSV_CUSTOM_EXPENSIVE(PhysicsCounters, NumFromBroadphase, NumPotentials, ECsvCustomStatOp::Accumulate);
		}

		const FPBDRigidsSOAs& Particles;
		const FAccelerationStructure* SpatialAcceleration;

		FIgnoreCollisionManager IgnoreCollisionManager;
	};
}
