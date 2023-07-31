// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"

namespace Chaos
{
	class FPBDIslandConstraint;
	class FPBDCollisionConstraint;
	class FPBDCollisionConstraints;
	class FPBDCollisionSolver;
	class FPBDCollisionSolverAdapter;
	class FPBDCollisionSolverManifoldPoint;
	class FPBDIsland;
	class FSolverBody;
	class FSolverBodyContainer;

	/**
	 * @brief Settings to control the low-level collision solver behaviour
	*/
	class FPBDCollisionSolverSettings
	{
	public:
		FPBDCollisionSolverSettings();

		// Maximum speed at which two objects can depenetrate (actually, how much relative velocity can be added
		// to a contact per frame when depentrating. Stacks and deep penetrations can lead to larger velocities)
		FReal MaxPushOutVelocity;

		// How many of the position iterations should run static/dynamic friction
		int32 NumPositionFrictionIterations;

		// How many of the velocity iterations should run dynamic friction
		// @todo(chaos): if NumVelocityFrictionIterations > 1, then dynamic friction in the velocity phase will be iteration 
		// count dependent (velocity-solve friction is currentlyused by quadratic shapes and RBAN)
		int32 NumVelocityFrictionIterations;

		// How many position iterations should have shock propagation enabled
		int32 NumPositionShockPropagationIterations;

		// How many velocity iterations should have shock propagation enabled
		int32 NumVelocityShockPropagationIterations;
	};

	/**
	 * The solver for a set of collision constraints. This collects all the data required to solve a set of collision
	 * constraints into a contiguous, ordered buffer.
	*/
	class FPBDCollisionContainerSolver : public FConstraintContainerSolver
	{
	public:
		FPBDCollisionContainerSolver(const FPBDCollisionConstraints& InConstraintContainer, const int32 InPriority);
		~FPBDCollisionContainerSolver();

		int32 NumSolvers() const { return CollisionSolvers.Num(); }

		virtual void Reset(const int32 InMaxCollisions) override final;

		virtual int32 GetNumConstraints() const override final
		{
			return NumSolvers();
		}

		// Set whether we are using deferred collision detection
		void SetIsDeferredCollisionDetection(const bool bDeferred) { bDeferredCollisionDetection = bDeferred; }

		//
		// IslandGroup API
		//
		virtual void AddConstraints() override final;
		virtual void AddConstraints(const TArrayView<FPBDIslandConstraint>& ConstraintHandles) override final;
		virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final;
		virtual void GatherInput(const FReal Dt) override final;
		virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
		virtual void ScatterOutput(const FReal Dt) override final;
		virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
		virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
		virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
		virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

	private:
		void AddConstraint(FPBDCollisionConstraint& Constraint);
		void UpdatePositionShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		void UpdateVelocityShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		bool SolvePositionImpl(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		bool SolvePositionWithFrictionImpl(const FReal Dt, const int32 BeginIndex, const int32 EndIndex, const FReal MaxPushOut);
		bool SolvePositionNoFrictionImpl(const FReal Dt, const int32 BeginIndex, const int32 EndIndex, const FReal MaxPushOut);
		bool SolveVelocityImpl(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		void UpdateCollisions(const FReal InDt, const int32 BeginIndex, const int32 EndIndex);

		const FPBDCollisionConstraints& ConstraintContainer;
		TArray<FPBDCollisionSolverAdapter> CollisionSolvers;
		bool bPerIterationCollisionDetection;
		bool bDeferredCollisionDetection;
	};
}
