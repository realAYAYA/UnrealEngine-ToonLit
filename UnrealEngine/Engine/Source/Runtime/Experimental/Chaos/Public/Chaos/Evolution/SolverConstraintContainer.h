// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ConstraintHandle.h"

namespace Chaos
{
	class FConstraintHandleHolder;
	class FPBDIslandConstraint;
	class FSolverBodyContainer;
	class FPBDIsland;

	/** 
	 * Base class for all the solver for a set of constraints of a specific type.
	 * A SolverContainer is used to solve a set constraints in sequential order. There will
	 * be one solver container for each thread on which we solve constraints (see FPBDIslandConstraintGroupSolver). 
	 * How constraints are assigned to groups depends on the constraint type and settings, but
	 * usually a group contains all constraints from one or more islands (unless we are coloring).
	*/
	class CHAOS_API FConstraintContainerSolver
	{
	public:

		FConstraintContainerSolver(const int32 InPriority)
			: Priority(InPriority)
		{}

		virtual ~FConstraintContainerSolver() {}
	
		/**
		 * Set the solver priority.
		 * Solvers are sorted by priority. Lower values are solved first so solvers with higher
		 * priorty values with "win" over lower ones.
		 * @see FPBDConstraintGroupSolver
		*/
		void SetPriority(const int32 InPriority)
		{
			Priority = InPriority;
		}

		/**
		 * Get the solver priority
		*/
		int32 GetPriority() const
		{
			return Priority;
		}

		/** 
		 * Set the maximum number of constraints the solver will have to handle. 
		 * This will be called only once per tick, so containers resized here will not have to resize again this tick
		 * so that pointers to elements in the container will remain valid for the tick (but not beyond).
		*/
		virtual void Reset(const int32 MaxConstraints) = 0;

		virtual int32 GetNumConstraints() const = 0;

		virtual void AddConstraints() = 0;

		/**
		 * Add a set of constraints to the solver container. This can be called multiple times: once for each island a group, but
		 * there will never be more constraints added than specified in Reset().
		 * NOTE: this should not do any actual data gathering - it should just add to the list of constraints in this group. All data
		 * gathering is handled in GatherInput.
		*/
		virtual void AddConstraints(const TArrayView<FPBDIslandConstraint>& Constraints) = 0;

		/**
		 * Add all the required bodies to the body container (required for the constraints added with AddConstraints)
		*/
		virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) = 0;

		virtual void GatherInput(const FReal Dt) = 0;
		virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) = 0;
		virtual void ScatterOutput(const FReal Dt) = 0;
		virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) = 0;

		/**
		 * Apply the position solve to all constraints in the container
		*/
		virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) = 0;

		/**
		 * Apply the velocity solve to all constraints in the container
		*/
		virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) = 0;

		/**
		 * Apply the projection solve to all constraints in the container
		*/
		virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) = 0;

	private:
		int32 Priority;
	};
	
	/**
	 * A constraint solver for use with a simple (non-graph-based) evolution (RBAN) and constraint containers
	 * with builtin-in solvers (Joints, Suspension, but not Collisions).
	 * @see TIndexedConstraintContainerSolver, FPBDCollisionContainerSolver
	 * @todo(chaos): really we should split the base class into group and scene versions. See CreateSceneSolver CreateGroupSolver
	*/
	template<typename ConstraintContainerType>
	class CHAOS_API TSimpleConstraintContainerSolver : public FConstraintContainerSolver
	{
	public:
		using FConstraintContainerType = ConstraintContainerType;
		using FConstraintHandleType = typename FConstraintContainerType::FConstraintContainerHandle;

		TSimpleConstraintContainerSolver(FConstraintContainerType& InConstraintContainer, const int32 InPriority)
			: FConstraintContainerSolver(InPriority)
			, ConstraintContainer(InConstraintContainer)
		{
		}

		virtual void Reset(const int32 MaxConstraints) override final
		{
		}

		virtual int32 GetNumConstraints() const override final
		{
			return ConstraintContainer.GetNumConstraints();
		}

		virtual void AddConstraints() override final
		{
			// We solve all constraints in the container in the order it prefers so nothing to do here
		}

		virtual void AddConstraints(const TArrayView<FPBDIslandConstraint>& Constraints) override final
		{
			// This solver container is for use with the a non-graph evolution. It will not call this function.
			ensure(false);
		}

		virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final
		{
			ConstraintContainer.AddBodies(SolverBodyContainer);
		}

		virtual void GatherInput(const FReal Dt) override final
		{
			ConstraintContainer.GatherInput(Dt);
		}

		virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final
		{
			// This solver container is for use with the a non-graph evolution. It will not call this function.
			ensure(false);
		}

		virtual void ScatterOutput(const FReal Dt) override final
		{
			ConstraintContainer.ScatterOutput(Dt);
		}

		virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final
		{
			// This solver container is for use with the a non-graph evolution. It will not call this function.
			ensure(false);
		}

		virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final
		{
			ConstraintContainer.ApplyPositionConstraints(Dt, It, NumIts);
		}

		virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final
		{
			ConstraintContainer.ApplyVelocityConstraints(Dt, It, NumIts);
		}

		virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final
		{
			ConstraintContainer.ApplyProjectionConstraints(Dt, It, NumIts);
		}

	protected:
		FConstraintContainerType& ConstraintContainer;
	};
}
