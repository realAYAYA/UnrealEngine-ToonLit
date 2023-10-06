// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Evolution/ConstraintGroupSolver.h"
#include "Chaos/ConstraintHandle.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"
#include "Chaos/PBDConstraintContainer.h"

namespace Chaos
{
	namespace Private
	{
		FPBDConstraintGroupSolver::FPBDConstraintGroupSolver()
			: TotalNumConstraints(0)
		{
		}

		FPBDConstraintGroupSolver::~FPBDConstraintGroupSolver()
		{
		}
	
		void FPBDConstraintGroupSolver::SetConstraintSolver(const int32 ContainerId, TUniquePtr<FConstraintContainerSolver>&& Solver)
		{
			if (ContainerId >= ConstraintContainerSolvers.Num())
			{
				ConstraintContainerSolvers.SetNum(ContainerId + 1);
			}

			ConstraintContainerSolvers[ContainerId] = MoveTemp(Solver);

			SetConstraintSolverImpl(ContainerId);

			SortSolverContainers();
		}

		void FPBDConstraintGroupSolver::SetConstraintSolverPriority(const int32 ContainerId, const int32 Priority)
		{
			if (ConstraintContainerSolvers[ContainerId] != nullptr)
			{
				if (ConstraintContainerSolvers[ContainerId]->GetPriority() != Priority)
				{
					ConstraintContainerSolvers[ContainerId]->SetPriority(Priority);
					SortSolverContainers();
				}
			}
		}

		void FPBDConstraintGroupSolver::SortSolverContainers()
		{
			PrioritizedConstraintContainerSolvers.Reset(ConstraintContainerSolvers.Num());
			for (TUniquePtr<FConstraintContainerSolver>& SolverContainer : ConstraintContainerSolvers)
			{
				if (SolverContainer != nullptr)
				{
					PrioritizedConstraintContainerSolvers.Add(SolverContainer.Get());
				}
			}

			PrioritizedConstraintContainerSolvers.StableSort(
				[](const FConstraintContainerSolver& L, const FConstraintContainerSolver& R)
				{
					return L.GetPriority() < R.GetPriority();
				});
		}

		void FPBDConstraintGroupSolver::Reset()
		{
			SolverBodyContainer.Reset(0);

			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->Reset(0);
				}
			}

			TotalNumConstraints = 0;

			ResetImpl();
		}

		void FPBDConstraintGroupSolver::AddConstraintsAndBodies()
		{
			AddConstraintsImpl();

			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->AddBodies(SolverBodyContainer);

					TotalNumConstraints += ConstraintContainerSolvers[ContainerIndex]->GetNumConstraints();
				}
			}

			SolverBodyContainer.Lock();
		}

		void FPBDConstraintGroupSolver::GatherBodies(const FReal Dt)
		{
			SolverBodyContainer.GatherInput(Dt, 0, SolverBodyContainer.Num());
		}

		void FPBDConstraintGroupSolver::GatherConstraints(const FReal Dt)
		{
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->GatherInput(Dt);
				}
			}
		}
		void FPBDConstraintGroupSolver::GatherBodies(const FReal Dt, const int32 BeginIndex, const int32 EndIndex)
		{
			SolverBodyContainer.GatherInput(Dt, BeginIndex, EndIndex);

			// Allow derived class to set up some extra data (level, color, etc)
			GatherBodiesImpl(Dt, BeginIndex, EndIndex);
		}

		template<typename LambdaType>
		void FPBDConstraintGroupSolver::ApplyToConstraintRange(const int32 BeginConstraintIndex, const int32 EndConstraintIndex, const LambdaType& Lambda)
		{
			// Loop over all the solver containers until we find the one that contains BeginConstraintIndex
			int32 BeginIndex = BeginConstraintIndex;
			int32 EndIndex = EndConstraintIndex;
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (FConstraintContainerSolver* ConstraintSolver = ConstraintContainerSolvers[ContainerIndex].Get())
				{
					const int32 NumSolverConstraints = ConstraintSolver->GetNumConstraints();
					if (BeginIndex < NumSolverConstraints)
					{
						// The current range start is in this container. 
						// Calculate how many of the constraints we should process and call the lambda
						const int32 BeginSolverIndex = BeginIndex;
						const int32 EndSolverIndex = FMath::Min(EndIndex, NumSolverConstraints);

						Lambda(ConstraintSolver, BeginSolverIndex, EndSolverIndex);
					}

					// Remove the constraints we just processed from the range. The range begin end is now relative to the next container (or empty)
					BeginIndex = FMath::Max(0, BeginIndex - NumSolverConstraints);
					EndIndex = FMath::Max(0, EndIndex - NumSolverConstraints);
					if (EndIndex <= 0)
					{
						break;
					}
				}
			}

			// We should have processed the whole range
			check(EndIndex == 0);
		}


		void FPBDConstraintGroupSolver::GatherConstraints(const FReal Dt, const int32 BeginConstraintIndex, const int32 EndConstraintIndex)
		{
			ApplyToConstraintRange(BeginConstraintIndex, EndConstraintIndex,
				[this, Dt](FConstraintContainerSolver* ConstraintSolver, const int32 BeginSolverIndex, const int32 EndSolverIndex)
				{
					ConstraintSolver->GatherInput(Dt, BeginSolverIndex, EndSolverIndex);
				});
		}

		void FPBDConstraintGroupSolver::ScatterBodies(const FReal Dt)
		{
			SolverBodyContainer.ScatterOutput(0, SolverBodyContainer.Num());
		}

		void FPBDConstraintGroupSolver::ScatterConstraints(const FReal Dt)
		{
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->ScatterOutput(Dt);
				}
			}
		}

		void FPBDConstraintGroupSolver::ScatterBodies(const FReal Dt, const int32 BeginBodyIndex, const int32 EndBodyIndex)
		{
			SolverBodyContainer.ScatterOutput(BeginBodyIndex, EndBodyIndex);
		}

		void FPBDConstraintGroupSolver::ScatterConstraints(const FReal Dt, const int32 BeginConstraintIndex, const int32 EndConstraintIndex)
		{
			ApplyToConstraintRange(BeginConstraintIndex, EndConstraintIndex,
				[this, Dt](FConstraintContainerSolver* ConstraintSolver, const int32 BeginSolverIndex, const int32 EndSolverIndex)
				{
					ConstraintSolver->ScatterOutput(Dt, BeginSolverIndex, EndSolverIndex);
				});
		}

		void FPBDConstraintGroupSolver::PreApplyPositionConstraints(const FReal Dt)
		{
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->PreApplyPositionConstraints(Dt);
				}
			}
		}

		void FPBDConstraintGroupSolver::ApplyPositionConstraints(const FReal Dt)
		{
			const int32 NumIts = Iterations.GetNumPositionIterations();

			// NOTE: We loop over prioritized solvers here
			for (int32 It = 0; It < NumIts; ++It)
			{
				for (int32 ContainerIndex = 0; ContainerIndex < PrioritizedConstraintContainerSolvers.Num(); ++ContainerIndex)
				{
					if (PrioritizedConstraintContainerSolvers[ContainerIndex] != nullptr)
					{
						PrioritizedConstraintContainerSolvers[ContainerIndex]->ApplyPositionConstraints(Dt, It, NumIts);
					}
				}
			}
		}

		void FPBDConstraintGroupSolver::PreApplyVelocityConstraints(const FReal Dt)
		{
			// Calculate the velocity from the net change in position after applying position constraints
			SolverBodyContainer.SetImplicitVelocities(Dt);

			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->PreApplyVelocityConstraints(Dt);
				}
			}
		}

		void FPBDConstraintGroupSolver::ApplyVelocityConstraints(const FReal Dt)
		{
			const int32 NumIts = Iterations.GetNumVelocityIterations();

			// NOTE: We loop over prioritized solvers here
			for (int32 It = 0; It < NumIts; ++It)
			{
				for (int32 ContainerIndex = 0; ContainerIndex < PrioritizedConstraintContainerSolvers.Num(); ++ContainerIndex)
				{
					if (PrioritizedConstraintContainerSolvers[ContainerIndex] != nullptr)
					{
						PrioritizedConstraintContainerSolvers[ContainerIndex]->ApplyVelocityConstraints(Dt, It, NumIts);
					}
				}
			}
		}

		void FPBDConstraintGroupSolver::PreApplyProjectionConstraints(const FReal Dt)
		{
			// Update the body transforms from the deltas calculated in the constraint solve phases 1 and 2
			// NOTE: deliberately not updating the world-space inertia as it is not used by joint projection
			// and no other constraints currently implement projection
			SolverBodyContainer.ApplyCorrections();

			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->PreApplyProjectionConstraints(Dt);
				}
			}
		}

		void FPBDConstraintGroupSolver::ApplyProjectionConstraints(const FReal Dt)
		{
			const int32 NumIts = Iterations.GetNumProjectionIterations();

			// NOTE: We loop over prioritized solvers here
			for (int32 It = 0; It < NumIts; ++It)
			{
				for (int32 ContainerIndex = 0; ContainerIndex < PrioritizedConstraintContainerSolvers.Num(); ++ContainerIndex)
				{
					if (PrioritizedConstraintContainerSolvers[ContainerIndex] != nullptr)
					{
						PrioritizedConstraintContainerSolvers[ContainerIndex]->ApplyProjectionConstraints(Dt, It, NumIts);
					}
				}
			}
		}

		//
		//
		//
		//
		//

		void FPBDSceneConstraintGroupSolver::AddConstraintsImpl()
		{
			// The Scene Solver solves all constraints, so just add everything
			for (int32 ContainerIndex = 0; ContainerIndex < ConstraintContainerSolvers.Num(); ++ContainerIndex)
			{
				if (ConstraintContainerSolvers[ContainerIndex] != nullptr)
				{
					ConstraintContainerSolvers[ContainerIndex]->AddConstraints();
				}
			}
		}

	}	// namespace Private
}	// namespace Chaos
