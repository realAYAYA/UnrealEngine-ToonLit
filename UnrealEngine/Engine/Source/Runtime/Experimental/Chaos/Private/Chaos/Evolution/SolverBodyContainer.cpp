// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/Particle/ParticleUtilities.h"

namespace Chaos
{
	void FSolverBodyAdapter::ScatterOutput()
	{
		if (Particle.IsValid())
		{
			// Set the particle state
			if (SolverBody.IsDynamic())
			{
				// Apply DP and DQ to P and Q
				SolverBody.ApplyCorrections();

				FParticleUtilities::SetCoMWorldTransform(Particle, SolverBody.P(), SolverBody.Q());
				Particle->SetV(SolverBody.V());
				Particle->SetW(SolverBody.W());
			}

			// Reset SolverBodyIndex cookie every step - it will be reassigned next step
			Particle->SetSolverBodyIndex(INDEX_NONE);
		}
	}

	int32 FSolverBodyContainer::AddParticle(FGenericParticleHandle InParticle)
	{
		// No array resizing allowed (we want fixed pointers)
		check(NumItems() < MaxItems());

		return SolverBodies.AddElement(FSolverBodyAdapter(InParticle));
	}

	FSolverBody* FSolverBodyContainer::FindOrAdd(FGenericParticleHandle InParticle)
	{
		// For dynamic bodies, we store a cookie on the Particle that holds the solver body index
		// For kinematics we cannot do this because the kinematic may be in multiple islands and 
		// would require a different index for each island, so we use a local map instead. 
		int32 ItemIndex = InParticle->SolverBodyIndex();
	
		if (ItemIndex == INDEX_NONE)
		{
			if (InParticle->IsDynamic())
			{
				// First time we have seen this particle, so add it
				check(!bLocked);
				ItemIndex = AddParticle(InParticle);
				InParticle->SetSolverBodyIndex(ItemIndex);
			}
			else // Not Dynamic
			{
				int32* ItemIndexPtr = ParticleToIndexMap.Find(InParticle);
				if (ItemIndexPtr != nullptr)
				{
					ItemIndex = *ItemIndexPtr;
				}
				else
				{
					// First time we have seen this particle, so add it
					check(!bLocked);
					ItemIndex = AddParticle(InParticle);
					ParticleToIndexMap.Add(InParticle, ItemIndex);
				}
			}			
		}
	
		check(ItemIndex != INDEX_NONE);
		
		FSolverBody* SolverBody = &SolverBodies[ItemIndex].GetSolverBody();

		return SolverBody;
	}

	void FSolverBodyContainer::GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex)
	{
		check((BeginIndex >= 0) && (BeginIndex <= EndIndex));
		check((EndIndex >= 0) && (EndIndex <= SolverBodies.Num()));

		for (int32 SolverBodyIndex = BeginIndex; SolverBodyIndex < EndIndex; ++SolverBodyIndex)
		{
			FSolverBodyAdapter& SolverBody = SolverBodies[SolverBodyIndex];
			SolverBody.GatherInput(Dt);
		}
	}

	void FSolverBodyContainer::ScatterOutput()
	{
		for (FSolverBodyAdapter& SolverBody : SolverBodies)
		{
			SolverBody.ScatterOutput();
		}
	}

	void FSolverBodyContainer::ScatterOutput(const int32 BeginIndex, const int32 EndIndex)
	{
		check((BeginIndex >= 0) && (BeginIndex <= EndIndex));
		check((EndIndex >= 0) && (EndIndex <= SolverBodies.Num()));

		for (int32 SolverBodyIndex = BeginIndex; SolverBodyIndex < EndIndex; ++SolverBodyIndex)
		{
			FSolverBodyAdapter& SolverBody = SolverBodies[SolverBodyIndex];
			SolverBody.ScatterOutput();
		}
	}

	void FSolverBodyContainer::SetImplicitVelocities(FReal Dt)
	{
		for (FSolverBodyAdapter& SolverBody : SolverBodies)
		{
			SolverBody.GetSolverBody().SetImplicitVelocity(Dt);
		}
	}

	void FSolverBodyContainer::ApplyCorrections()
	{
		for (FSolverBodyAdapter& SolverBody : SolverBodies)
		{
			SolverBody.GetSolverBody().ApplyCorrections();
		}
	}

	void FSolverBodyContainer::UpdateRotationDependentState()
	{
		for (FSolverBodyAdapter& SolverBody : SolverBodies)
		{
			SolverBody.GetSolverBody().UpdateRotationDependentState();
		}
	}

}