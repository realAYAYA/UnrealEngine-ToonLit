// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Island/IslandGroup.h"

#include "Chaos/ConstraintHandle.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/Island/SolverIsland.h"
#include "Chaos/PBDConstraintContainer.h"

namespace Chaos
{
	FPBDIslandConstraintGroupSolver::FPBDIslandConstraintGroupSolver(FPBDIslandManager& InIslandManager)
		: FPBDConstraintGroupSolver()
		, IslandManager(InIslandManager)
		, NumParticles(0)
		, NumConstraints(0)
	{
	}

	void FPBDIslandConstraintGroupSolver::SetConstraintSolverImpl(int32 ContainerId)
	{
		if (ContainerId >= NumContainerConstraints.Num())
		{
			NumContainerConstraints.SetNumZeroed(ContainerId + 1);
		}
	}

	void FPBDIslandConstraintGroupSolver::ResetImpl()
	{
		NumParticles = 0;
		NumConstraints = 0;
		for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
		{
			NumContainerConstraints[ContainerIndex] = 0;
		}

		Islands.Reset();
	}

	void FPBDIslandConstraintGroupSolver::AddIsland(FPBDIsland* Island)
	{
		if (Island)
		{
			Islands.Add(Island);

			NumParticles += Island->GetNumParticles();

			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				const int32 NumIslandConstraints = Island->GetConstraints(ContainerIndex).Num();
				NumContainerConstraints[ContainerIndex] += NumIslandConstraints;
				NumConstraints += NumIslandConstraints;
			}
		}
	}

	void FPBDIslandConstraintGroupSolver::AddConstraintsImpl()
	{
		// Initialize buffer sizes for solver particles and constraint solvers
		SolverBodyContainer.Reset(NumParticles);
		for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
		{
			if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
			{
				ConstraintContainerSolvers[ContainerIndex]->Reset(NumContainerConstraints[ContainerIndex]);
			}
		}

		// Add all the constraints to the solvers, but do not collect data from the constraints yet (each constraint type has its own solver). 
		// This also sets up the SolverBody array, but does not gather the body data.
		for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
		{
			if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
			{
				for (FPBDIsland* Island : Islands)
				{
					// const_cast is because MakeArrayView doesn't handle a const array of pointers-to-non-const.
					TArrayView<FPBDIslandConstraint> IslandConstraints = MakeArrayView(Island->GetConstraints(ContainerIndex));
					ConstraintContainerSolvers[ContainerIndex]->AddConstraints(IslandConstraints);
				}
			}
		}
	}

	void FPBDIslandConstraintGroupSolver::GatherBodiesImpl(const FReal Dt, const int32 BeginBodyIndex, const int32 EndBodyIndex)
	{
		// @todo(chaos): optimize
		for (int32 SolverBodyIndex = BeginBodyIndex; SolverBodyIndex < EndBodyIndex; ++SolverBodyIndex)
		{
			FSolverBodyAdapter& SolverBody = SolverBodyContainer.GetItem(SolverBodyIndex);
			const int32 Level = IslandManager.GetParticleLevel(SolverBody.GetParticle()->Handle());
			SolverBody.GetSolverBody().SetLevel(Level);
		}
	}


}