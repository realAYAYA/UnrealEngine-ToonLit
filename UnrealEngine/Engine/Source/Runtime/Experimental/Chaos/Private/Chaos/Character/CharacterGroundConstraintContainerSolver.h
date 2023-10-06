// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Evolution/SolverConstraintContainer.h"
#include "Chaos/Character/CharacterGroundConstraintContainer.h"
#include "CharacterGroundConstraintSolver.h"

namespace Chaos
{
	namespace Private
	{
		class FCharacterGroundConstraintContainerSolver : public FConstraintContainerSolver
		{
		public:
			FCharacterGroundConstraintContainerSolver(FCharacterGroundConstraintContainer& InConstraintContainer, const int32 InPriority);
			~FCharacterGroundConstraintContainerSolver();

			//////////////////////////////////////////////////////////////////////////
			// FConstraintContainerSolver impl

			virtual int32 GetNumConstraints() const override final { return SolvedConstraints.Num(); }

			virtual void Reset(const int32 InMaxConstraints) override final;

			/// Add all constraints from the constraint container in the order that they appear there
			virtual void AddConstraints() override final;

			/// Add all constraints from an island (guaranteed to be from the same constraint container)
			virtual void AddConstraints(const TArrayView<Private::FPBDIslandConstraint*>& IslandConstraints) override final;

			/// Set the bodies on the constraint from the constraint data solver
			virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final;

			/// Initialize the solver from the constraint data
			virtual void GatherInput(const FReal Dt) override final;
			virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;

			/// Get the solver output that will be stored per constraint - applied force and torque
			virtual void ScatterOutput(const FReal Dt) override final;
			virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;

			/// Apply a single position solve iteration for all constraints
			virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

			/// Apply a single velocity solve iteration for all constraints
			/// Note: Currently not used for character ground constraints
			virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

			/// Apply a single projection solve iteration for all constraints
			/// Note: Currently not used for character ground constraints
			virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

		private:
			void AddConstraint(FCharacterGroundConstraintHandle* Constraint);

			FCharacterGroundConstraintContainer& ConstraintContainer;

			TArray<FCharacterGroundConstraintHandle*> SolvedConstraints;

			// Array of single constraint solvers
			TArray<FCharacterGroundConstraintSolver> Solvers;
		};

	} // namespace Private
} // namespace Chaos