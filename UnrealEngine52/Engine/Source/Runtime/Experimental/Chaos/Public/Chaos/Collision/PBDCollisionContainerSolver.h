// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Collision/PBDCollisionSolver.h"
#include "Chaos/Collision/PBDCollisionSolverSettings.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"

namespace Chaos
{
	namespace Private
	{
		class FPBDIsland;
		class FPBDIslandConstraint;
	}
	class FPBDCollisionConstraint;
	class FPBDCollisionConstraints;
	class FSolverBody;
	class FSolverBodyContainer;

	/**
	 * The solver for a set of collision constraints. This collects all the data required to solve a set of collision
	 * constraints into a contiguous, ordered buffer.
	*/
	class FPBDCollisionContainerSolver : public FConstraintContainerSolver
	{
	public:
		UE_NONCOPYABLE(FPBDCollisionContainerSolver);

		FPBDCollisionContainerSolver(const FPBDCollisionConstraints& InConstraintContainer, const int32 InPriority);
		~FPBDCollisionContainerSolver();

		int32 NumSolvers() const { return CollisionConstraints.Num(); }

		virtual void Reset(const int32 InMaxCollisions) override final;

		virtual int32 GetNumConstraints() const override final { return NumSolvers(); }

		//
		// IslandGroup API
		//
		virtual void AddConstraints() override final;
		virtual void AddConstraints(const TArrayView<Private::FPBDIslandConstraint>& ConstraintHandles) override final;
		virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final;
		virtual void GatherInput(const FReal Dt) override final;
		virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
		virtual void ScatterOutput(const FReal Dt) override final;
		virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
		virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
		virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
		virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

	private:
		void CachePrefetchSolver(const int32 ConstraintIndex) const;
		void AddConstraint(FPBDCollisionConstraint& Constraint);
		void UpdatePositionShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		void UpdateVelocityShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		bool SolvePositionImpl(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		bool SolveVelocityImpl(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings);
		void UpdateCollisions(const FReal InDt, const int32 BeginIndex, const int32 EndIndex);

		const FPBDCollisionConstraints& ConstraintContainer;
		TArray<Private::FPBDCollisionSolver> CollisionSolvers;
		TArray<Private::FPBDCollisionSolverManifoldPoint> CollisionSolverManifoldPoints;
		TArray<FPBDCollisionConstraint*> CollisionConstraints;
		TArray<bool> bCollisionConstraintPerIterationCollisionDetection;
		bool bPerIterationCollisionDetection;
	};
}
