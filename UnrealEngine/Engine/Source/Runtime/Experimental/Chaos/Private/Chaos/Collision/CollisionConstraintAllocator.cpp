// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/PBDCollisionConstraints.h"

namespace Chaos
{
	namespace Private
	{
		FPBDCollisionConstraintPtr FCollisionContextAllocator::CreateConstraint(
			FGeometryParticleHandle* Particle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* Simplicial0,
			const FRigidTransform3& ShapeRelativeTransform0,
			FGeometryParticleHandle* Particle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* Simplicial1,
			const FRigidTransform3& ShapeRelativeTransform1,
			const FReal CullDistance,
			const bool bUseManifold,
			const EContactShapesType ShapePairType)
		{
			FPBDCollisionConstraintPtr Constraint = CreateConstraint();

			if (Constraint.IsValid())
			{
				Constraint->SetContainer(CollisionContainer);
				FPBDCollisionConstraint::Make(Particle0, Implicit0, Shape0, Simplicial0, ShapeRelativeTransform0, Particle1, Implicit1, Shape1, Simplicial1, ShapeRelativeTransform1, CullDistance, bUseManifold, ShapePairType, *Constraint);
			}

			return Constraint;
		}

		void FCollisionContextAllocator::ProcessNewMidPhases(FCollisionConstraintAllocator* Allocator)
		{
			// NOTE: Called from the physics thread, and never from a physics task/parallel-for. No need for locks.

			// Register the midphases with each of their particles and add the midphase to the central list
			for (FParticlePairMidPhase* MidPhase : NewMidPhases)
			{
				FGeometryParticleHandle* Particle0 = MidPhase->GetParticle0();
				FGeometryParticleHandle* Particle1 = MidPhase->GetParticle1();

				// NOTE: It may look weird that we are passing the particle's self to the AddMidPhase method. 
				// The midphase represents two particles and it needs to know which one we're currently adding it to.
				// We could make this API nicer but providing a helper on the Particle, but it's a bit awkard to do so
				// and since AddMidPahse is only called from here we'll leave it like this for now.
				Particle0->ParticleCollisions().AddMidPhase(Particle0, MidPhase);
				Particle1->ParticleCollisions().AddMidPhase(Particle1, MidPhase);

#if CHAOS_MIDPHASE_OBJECTPOOL_ENABLED 
				Allocator->AddMidPhase(FParticlePairMidPhasePtr(MidPhase, FParticlePairMidPhaseDeleter(MidPhasePool)));
#else
				Allocator->AddMidPhase(FParticlePairMidPhasePtr(MidPhase));
#endif
			}
			NewMidPhases.Reset();
		}

		void FCollisionContextAllocator::ProcessNewConstraints(FCollisionConstraintAllocator* Allocator)
		{
			// NOTE: Called from the physics thread, and never from a physics task/parallel-for. No need for locks.

			for (FPBDCollisionConstraint* NewConstraint : NewActiveConstraints)
			{
				Allocator->ActivateConstraintImp(NewConstraint);
			}
			NewActiveConstraints.Reset();
		}

		void FCollisionConstraintAllocator::PruneExpiredMidPhases()
		{
			// NOTE: Called from the physics thread, and never from a physics task/parallel-for. No need for locks.

			// Determine which particle pairs are no longer overlapping
			// Prune all pairs which were not updated this tick as part of the collision
			// detection loop and are not asleep
			for (int32 Index = ParticlePairMidPhases.Num() - 1; Index >= 0; --Index)
			{
				FParticlePairMidPhasePtr& MidPhase = ParticlePairMidPhases[Index];

				// We could also add !MidPhase->IsInConstraintGraph() here, but we know that we will not be in the graph if we were
				// not active this tick and were not asleep. The constraint graph ejects all non-sleeping constraints each tick.
				// (There is a check in the collision destructor that verified this).
				if (!MidPhase->IsUsedSince(CurrentEpoch) && !MidPhase->IsSleeping())
				{
					// Remove from the particles' lists of contacts
					DetachParticlePairMidPhase(MidPhase.Get());

					// Destroy the midphase and its collisions
					MidPhase.Reset();

					// Remove the midphase from the list. 
					// ParticlePairMidPhases can get large so we allow it to shrink from time to time
					const int32 MaxSlack = 1000;
					const int32 Slack = ParticlePairMidPhases.Max() - ParticlePairMidPhases.Num();
					const bool bAllowShrink = (Slack > MaxSlack);
					ParticlePairMidPhases.RemoveAtSwap(Index, 1, bAllowShrink);
				}
			}
		}

		void FCollisionConstraintAllocator::RemoveActiveConstraint(FPBDCollisionConstraint& Constraint)
		{
			FPBDCollisionConstraintContainerCookie& Cookie = Constraint.GetContainerCookie();

			// ConstraintIndex is only valid for one frame, so make sure we
			// were actually activated during the most recent tick
			if (Cookie.LastUsedEpoch != CurrentEpoch)
			{
				return;
			}

			// Remove from the active list
			if (Cookie.ConstraintIndex != INDEX_NONE)
			{
				check(ActiveConstraints[Cookie.ConstraintIndex] == &Constraint);
				ActiveConstraints[Cookie.ConstraintIndex] = nullptr;
				Cookie.ConstraintIndex = INDEX_NONE;
			}

			// Remove from the active CCD list
			if (Cookie.CCDConstraintIndex != INDEX_NONE)
			{
				check(ActiveCCDConstraints[Cookie.CCDConstraintIndex] == &Constraint);
				ActiveCCDConstraints[Cookie.CCDConstraintIndex] = nullptr;
				Cookie.CCDConstraintIndex = INDEX_NONE;
			}
		}

		void FCollisionConstraintAllocator::RemoveParticle(FGeometryParticleHandle* Particle)
		{
			// Removal not supported during the (parallel) collision detection phase
			check(!bInCollisionDetectionPhase);

			// Loop over all particle pairs involving this particle and tell each MidPhase 
			// that one of its particles is gone. It will get pruned at the next collision detection phase.
			// Also remove its collisions from the Active lists.
			Particle->ParticleCollisions().VisitMidPhases([this, Particle](FParticlePairMidPhase& MidPhase)
				{
					MidPhase.VisitCollisions([this](FPBDCollisionConstraint& Constraint)
						{
							RemoveActiveConstraint(Constraint);
							return ECollisionVisitorResult::Continue;
						});

					MidPhase.DetachParticle(Particle);

					return ECollisionVisitorResult::Continue;
				});
		}

	}	// namespace Private
}	// namespace Chaos