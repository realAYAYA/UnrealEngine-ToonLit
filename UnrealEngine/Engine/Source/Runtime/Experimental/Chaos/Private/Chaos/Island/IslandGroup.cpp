// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Island/IslandGroup.h"

#include "Chaos/ConstraintHandle.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/PBDConstraintContainer.h"

namespace Chaos
{
	namespace Private
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
					const int32 NumIslandConstraints = Island->GetNumContainerConstraints(ContainerIndex);
					NumContainerConstraints[ContainerIndex] += NumIslandConstraints;
					NumConstraints += NumIslandConstraints;
				}
			}
		}

		void FPBDIslandConstraintGroupSolver::AddConstraintsImpl()
		{
			// Initialize buffer sizes for solver particles
			SolverBodyContainer.Reset(NumParticles);

			// Add all the constraints to the solvers, but do not collect data from the constraints yet (each constraint type has its own solver). 
			// This also sets up the SolverBody array, but does not gather the body data.
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					// Initialize buffer sizes for constraint solvers
					ConstraintContainerSolvers[ContainerIndex]->Reset(NumContainerConstraints[ContainerIndex]);

					for (FPBDIsland* Island : Islands)
					{
						TArrayView<FPBDIslandConstraint*> IslandConstraints = Island->GetConstraints(ContainerIndex);
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
				FSolverBody& SolverBody = SolverBodyContainer.GetSolverBody(SolverBodyIndex);
				FGeometryParticleHandle* Particle = SolverBodyContainer.GetParticle(SolverBodyIndex);
				const int32 Level = IslandManager.GetParticleLevel(Particle);
				SolverBody.SetLevel(Level);
			}
		}

		void FPBDIslandConstraintGroupSolver::SetIterationSettings(const FIterationSettings& InDefaultIterations)
		{
			Iterations = InDefaultIterations;
			//for (FPBDIsland* Island : Islands)
			//{
			//	Iterations = FIterationSettings::Merge(Iterations, Island->GetIterationSettings());
			//}
		}

	}	// namespace Private
}	// namespace Chaos