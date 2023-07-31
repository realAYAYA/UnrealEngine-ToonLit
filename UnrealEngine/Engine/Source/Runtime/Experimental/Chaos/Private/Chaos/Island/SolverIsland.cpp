// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Island/SolverIsland.h"
#include "Chaos/PBDConstraintContainer.h"
#include "Chaos/PBDRigidsSOAs.h"

namespace Chaos
{
	
	FPBDIsland::FPBDIsland(const int32 MaxConstraintContainers)
	: IslandIndex(INDEX_NONE)
	, bIsSleeping(false)
	, bNeedsResim(false)
	, bIsPersistent(true)
	, SleepCounter(0)
	, IslandParticles()
	, IslandConstraintsByType()
	, NumConstraints(0)
{
	IslandConstraintsByType.SetNum(MaxConstraintContainers);
}

void FPBDIsland::Reuse()
{
	check(GetNumParticles() == 0);
	check(GetNumConstraints() == 0);

	SetSleepCounter(0);
	SetIsSleepingChanged(false);
}

void FPBDIsland::UpdateParticles()
{
	for (FPBDIslandParticle& IslandParticle : IslandParticles)
	{
		FGenericParticleHandle ParticleHandle = IslandParticle.GetParticle();
		if (ParticleHandle.IsValid() && ParticleHandle->IsDynamic())
		{
			ParticleHandle->SetIslandIndex(IslandIndex);
		}
	}
}
	
void FPBDIsland::ClearParticles()
{
	IslandParticles.Reset();
}

void FPBDIsland::AddParticle(FGenericParticleHandle ParticleHandle)
{
	if (ParticleHandle.IsValid())
	{
		if (ParticleHandle->IsDynamic())
		{
			ParticleHandle->SetIslandIndex(IslandIndex);
		}
		IslandParticles.Add(ParticleHandle->Handle());
	}
}

void FPBDIsland::ReserveParticles(const int32 NumParticles)
{
	ClearParticles();
	IslandParticles.Reserve(NumParticles);
}

void FPBDIsland::AddConstraint(FConstraintHandle* ConstraintHandle, const int32 Level, const int32 Color, const uint32 SubSortKey)
{
	if (ConstraintHandle)
	{
		const int32 ContainerId = ConstraintHandle->GetContainerId();

		// Array should already be appropriately sized for all containers
		IslandConstraintsByType[ContainerId].Emplace(ConstraintHandle, Level, Color, SubSortKey);

		++NumConstraints;
	}
}

void FPBDIsland::ClearConstraints()
{
	for (TArray<FPBDIslandConstraint>& Constraints : IslandConstraintsByType)
	{
		Constraints.Reset();
	}
	NumConstraints = 0;
}

void FPBDIsland::SortConstraints()
{
	for (TArray<FPBDIslandConstraint>& IslandConstraints : IslandConstraintsByType)
	{
		if (IslandConstraints.Num() > 1)
		{
			Algo::Sort(IslandConstraints,
				[](const FPBDIslandConstraint& L, const FPBDIslandConstraint& R) -> bool
				{
					return L.GetSortKey() < R.GetSortKey();
				});
		}
	}
}

void FPBDIsland::PropagateSleepState(FPBDRigidsSOAs& ParticleSOAs)
{
	bool bNeedRebuild = false;
	for (FPBDIslandParticle& IslandParticle : IslandParticles)
	{
		FGeometryParticleHandle* Particle = IslandParticle.GetParticle();
		if (Particle->CastToRigidParticle() && !Particle->CastToRigidParticle()->Disabled())
		{
			// If not sleeping we activate the sleeping particles
			if (!bIsSleeping && Particle->Sleeping())
			{
				ParticleSOAs.ActivateParticle(Particle, true);

				// When we wake particles, we have skipped their integrate step which causes some issues:
				//	- we have zero velocity (no gravity or external forces applied)
				//	- the world transforms cached in the ShapesArray will be at the last post-integrate positions
				//	  which doesn't match what the velocity is telling us
				// This causes problems for the solver - essentially we have an "initial overlap" situation.
				// @todo(chaos): We could just run (partial) integrate here for this particle, but we don't know about the Evolution - fix this
				for (const TUniquePtr<FPerShapeData>& Shape : Particle->ShapesArray())
				{
					Shape->UpdateLeafWorldTransform(Particle);
				}

				bNeedRebuild = true;
			}
			// If sleeping we deactivate the dynamic particles
			else if (bIsSleeping && !Particle->Sleeping())
			{
				ParticleSOAs.DeactivateParticle(Particle, true);
				bNeedRebuild = true;
			}
		}
	}

	if (bNeedRebuild)
	{
		ParticleSOAs.RebuildViews();
	}

	// Island constraints are updating their sleeping flag + awaken one 
	for (TArray<FPBDIslandConstraint>& TypedIslandConstraints : IslandConstraintsByType)
	{
		for (FPBDIslandConstraint& IslandConstraint : TypedIslandConstraints)
		{
			IslandConstraint.GetConstraint()->SetIsSleeping(bIsSleeping);
		}
	}
}

void FPBDIsland::UpdateSyncState(FPBDRigidsSOAs& Particles)
{
	bNeedsResim = false;
	for (FPBDIslandParticle& IslandParticle : IslandParticles)
	{
		// If even one particle is soft/hard desync we must resim the entire island (when resim is used)
		// seems cheap enough so just always do it, if slow pass resim template in here
		FGeometryParticleHandle* Particle = IslandParticle.GetParticle();
		if (Particle->SyncState() != ESyncState::InSync)
		{
			bNeedsResim = true;
			break;
		}
	}
}

}