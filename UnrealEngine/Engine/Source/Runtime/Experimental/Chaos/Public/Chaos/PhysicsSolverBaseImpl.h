// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GCObject.h"

#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/PullPhysicsDataImp.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/JointConstraintProxy.h"
#include "Chaos/Framework/ChaosResultsManager.h"

namespace Chaos
{
	// Pulls physics state for each dirty particle and allows caller to do additional work if needed
	template <typename RigidLambda, typename ConstraintLambda>
	void FPhysicsSolverBase::PullPhysicsStateForEachDirtyProxy_External(const RigidLambda& RigidFunc, const ConstraintLambda& ConstraintFunc)
	{
		using namespace Chaos;

		FPullPhysicsData* LatestData = nullptr;
		if (IsUsingAsyncResults() && UseAsyncInterpolation)
		{
			const FReal ResultsTime = GetPhysicsResultsTime_External();
			//we want to interpolate between prev and next. There are a few cases to consider:
			//case 1: dirty data exists in both prev and next. In this case continuous data is interpolated, state data is a step function from prev to next
			//case 2: prev has dirty data and next doesn't. in this case take prev as it means nothing to interpolate, just a constant value
			//case 3: prev has dirty data and next has overwritten data. In this case we do nothing as the overwritten data wins (came from GT), and also particle may be deleted
			//case 4: prev has no dirty data and next does. In this case interpolate from gt data to next
			//case 5: prev has no dirty data and next was overwritten. In this case do nothing as the overwritten data wins, and also particle may be deleted

			TArray<const FChaosInterpolationResults*> ResultsPerChannel = PullResultsManager->PullAsyncPhysicsResults_External(ResultsTime);
			for(int32 ChannelIdx = 0; ChannelIdx < ResultsPerChannel.Num(); ++ChannelIdx)
			{
				const FChaosInterpolationResults& Results = *ResultsPerChannel[ChannelIdx];
				LatestData = Results.Next;
				//todo: go wide
				const int32 SolverTimestamp = Results.Next ? Results.Next->SolverTimestamp : INDEX_NONE;

				// single particles
				for (const FChaosRigidInterpolationData& RigidInterp : Results.RigidInterpolations)
				{
					if (FSingleParticlePhysicsProxy* Proxy = RigidInterp.Prev.GetProxy())
					{
						if (Proxy->PullFromPhysicsState(RigidInterp.Prev, SolverTimestamp, &RigidInterp.Next, &Results.Alpha))
						{
							RigidFunc(Proxy);
						}
					}
				}
				
				// geometry collections
				for (const FChaosGeometryCollectionInterpolationData& GCInterp : Results.GeometryCollectionInterpolations)
				{
					if (FGeometryCollectionPhysicsProxy* Proxy = GCInterp.Prev.GetProxy())
					{
						Proxy->PullFromPhysicsState(GCInterp.Prev, SolverTimestamp, &GCInterp.Next, &Results.Alpha);
					}
				}
			}
		}
		else
		{
			//no interpolation so just use latest, in non-substepping modes this will just be the next result
			// available in the queue - however if we substepped externally we need to consume the whole
			// queue by telling the sync pull that we expect multiple results.

			const FChaosInterpolationResults& Results = PullResultsManager->PullSyncPhysicsResults_External();
			LatestData = Results.Next;
			//todo: go wide
			const int32 SolverTimestamp = Results.Next ? Results.Next->SolverTimestamp : INDEX_NONE;

			// Single particles
			for (const FChaosRigidInterpolationData& RigidInterp : Results.RigidInterpolations)
				{
					if (FSingleParticlePhysicsProxy* Proxy = RigidInterp.Prev.GetProxy())
					{
						if (Proxy->PullFromPhysicsState(RigidInterp.Next, SolverTimestamp))
						{
							RigidFunc(Proxy);
						}
					}
				}

			// geometry collections
			for (const FChaosGeometryCollectionInterpolationData& GCInterp : Results.GeometryCollectionInterpolations)
			{
				if (FGeometryCollectionPhysicsProxy* Proxy = GCInterp.Prev.GetProxy())
				{
					Proxy->PullFromPhysicsState(GCInterp.Next, SolverTimestamp);
				}
			}
			
		}

		//no interpolation for GC or joints at the moment
		if(LatestData)
		{
			 const int32 SyncTimestamp = LatestData->SolverTimestamp;

			//
			// @todo(chaos) : Add Dirty Constraints Support
			//
			// This is temporary constraint code until the DirtyParticleBuffer
			// can be updated to support constraints. In summary : The 
			// FDirtyPropertiesManager is going to be updated to support a 
			// FDirtySet that is specific to a TConstraintProperties class.
			//
			for (const FDirtyJointConstraintData& DirtyData : LatestData->DirtyJointConstraints)
			{
				if (auto Proxy = DirtyData.GetProxy())
				{
					if (Proxy->PullFromPhysicsState(DirtyData, SyncTimestamp))
					{
						ConstraintFunc(Proxy);
					}

				}
			}

			//latest data may be used multiple times during interpolation, so for non interpolated joints we clear it
			LatestData->DirtyJointConstraints.Reset();
		}
	}

}
