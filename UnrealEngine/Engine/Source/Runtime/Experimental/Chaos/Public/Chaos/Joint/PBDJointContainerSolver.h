// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"
#include "Chaos/Joint/PBDJointSolverGaussSeidel.h"
#include "Chaos/Joint/PBDJointCachedSolverGaussSeidel.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/PBDJointConstraintTypes.h"

namespace Chaos
{
	namespace Private
	{
		/**
		 * Runs the solvers for a set of constraints belonging to a JointConstraints container.
		 * 
		 * For the main scene, each IslandGroup owns a FPBDJointContainerSolver and the list of constraints to be solved
		 * and the order in which they are solved is determined by the constraint graph.
		 * 
		 * For RBAN, there is one FPBDJointContainerSolver that solves all joints in the simulation in the order that
		 * they occur in the container.
		*/
		class FPBDJointContainerSolver : public FConstraintContainerSolver
		{
		public:
			FPBDJointContainerSolver(FPBDJointConstraints& InConstraintContainer, const int32 InPriority);
			~FPBDJointContainerSolver();

			// FConstraintContainerSolver impl
			virtual int32 GetNumConstraints() const override final { return ContainerConstraintIndices.Num(); }
			virtual void Reset(const int32 InMaxCollisions) override final;
			virtual void AddConstraints() override final;
			virtual void AddConstraints(const TArrayView<Private::FPBDIslandConstraint*>& IslandConstraints) override final;
			virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final;
			virtual void GatherInput(const FReal Dt) override final;
			virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
			virtual void ScatterOutput(const FReal Dt) override final;
			virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
			virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
			virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
			virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

			FPBDJointConstraints& GetContainer() const { return ConstraintContainer; }
			const FPBDJointSolverSettings& GetSettings() const { return ConstraintContainer.GetSettings(); }
			const FPBDJointSettings& GetConstraintSettings(const int32 InConstraintIndex) const { return ConstraintContainer.GetConstraintSettings(ContainerConstraintIndices[InConstraintIndex]); }
			int32 GetContainerConstraintIndex(const int32 InConstraintIndex) const { return ContainerConstraintIndices[InConstraintIndex]; }

		private:
			bool UseLinearSolver() const;
			void AddConstraint(const int32 InContainerConstraintIndex);
			void ResizeSolverArrays();
			void ApplyLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts);
			void ApplyNonLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts);

			FPBDJointConstraints& ConstraintContainer;

			// Index remapping from internal index [0,NumToSolve) to index in the joint container [0,NumJointsInWorld)
			TArray<int32> ContainerConstraintIndices;

			// The linear and non-linear joint solvers. One for each joint we wish to solve in the order that they are solved
			TArray<FPBDJointCachedSolver> LinearConstraintSolvers;
			TArray<FPBDJointSolver> NonLinearConstraintSolvers;
		};

	}
}