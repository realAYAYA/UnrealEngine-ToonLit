// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Collision/CollisionDetector.h"
#include "Chaos/Collision/CollisionContext.h"
#include "Chaos/Collision/SpatialAccelerationBroadPhase.h"
#include "Chaos/EvolutionResimCache.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "ProfilingDebugging/CsvProfiler.h"

namespace Chaos
{
	class CHAOS_API FSpatialAccelerationCollisionDetector : public FCollisionDetector
	{
	public:
		FSpatialAccelerationCollisionDetector(FSpatialAccelerationBroadPhase& InBroadPhase, FPBDCollisionConstraints& InCollisionContainer)
			: FCollisionDetector(InCollisionContainer)
			, BroadPhase(InBroadPhase)
		{
		}

		FSpatialAccelerationBroadPhase& GetBroadPhase()
		{ 
			return BroadPhase;
		}

		virtual void DetectCollisions(const FReal Dt, FEvolutionResimCache* ResimCache) override
		{
			SCOPE_CYCLE_COUNTER(STAT_Collisions_Detect);
			CHAOS_SCOPED_TIMER(DetectCollisions);
			CSV_SCOPED_TIMING_STAT(Chaos, DetectCollisions);

			if (!GetCollisionContainer().GetCollisionsEnabled())
			{
				return;
			}

			CollisionContainer.BeginDetectCollisions();

			// Collision detection pipeline: BroadPhase -[parallel]-> NarrowPhase -[parallel]-> CollisionAllocator -[serial]-> Container
			BroadPhase.ProduceOverlaps(Dt, &CollisionContainer.GetConstraintAllocator(), Settings, ResimCache);

			CollisionContainer.EndDetectCollisions();

			// If we have a resim cache restore and save contacts
			if(ResimCache)
			{
				// Ensure we have at least one allocator
				CollisionContainer.GetConstraintAllocator().SetMaxContexts(1);

				FCollisionContext Context;
				Context.SetSettings(Settings);
				Context.SetAllocator(CollisionContainer.GetConstraintAllocator().GetContextAllocator(0));

				CollisionContainer.GetConstraintAllocator().AddResimConstraints(ResimCache->GetAndSanitizeConstraints(), Context);

				ResimCache->SaveConstraints(CollisionContainer.GetConstraints());
			}
		}

	private:
		FSpatialAccelerationBroadPhase& BroadPhase;
	};
}
