// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"

#include "Chaos/Evolution/ConstraintGroupSolver.h"

namespace Chaos
{
	class FPBDConstraintContainer;
	class FPBDIsland;
	class FPBDIslandManager;

	/**
	* A set of constraints that will ne solved in sequence on a single thread. This will usually be the constraints
	* from several islands, but may be a sub set of constraints of a single island when coloring is enabled for
	* that island.
	*/
	class CHAOS_API FPBDIslandConstraintGroupSolver : public FPBDConstraintGroupSolver
	{
	public:
		UE_NONCOPYABLE(FPBDIslandConstraintGroupSolver);

		/**
		* Init the island group 
		*/
		FPBDIslandConstraintGroupSolver(FPBDIslandManager& InIslandManager);

		/**
		* Add island to the group
		* @param Island Island to be added
		*/
		void AddIsland(FPBDIsland* Island);

		/**
		* Return the islands within the group
		*/
		FORCEINLINE const TArray<FPBDIsland*>& GetIslands() { return Islands; }

		/**
		 * The number of particles (dynamics, kinematic and static) in the island.
		*/
		inline int32 GetNumParticles() const { return NumParticles; }

		/**
		 * The number of constraints of all types in the island.
		*/
		inline int32 GetNumConstraints() const { return NumConstraints; }

		/**
		 * The number of constraints of the specific container type in the island.
		*/
		inline int32 GetNumConstraints(const int32 ContainerId) const { return NumContainerConstraints[ContainerId]; }

	protected:
		// Base class overrides
		virtual void SetConstraintSolverImpl(const int32 ContainerId) override final;
		virtual void ResetImpl() override final;
		virtual void AddConstraintsImpl() override final;
		virtual void GatherBodiesImpl(const FReal Dt, const int32 BeginBodyIndex, const int32 EndBodyIndex) override final;

	private:
		FPBDIslandManager& IslandManager;

		// Item counters used to initialize the solver containers
		static constexpr int32 NumExpectedConstraintTypes = 5;
		int32 NumParticles;
		int32 NumConstraints;
		TArray<int32, TInlineAllocator<NumExpectedConstraintTypes>> NumContainerConstraints;	// Per ContainerId

		TArray<FPBDIsland*> Islands;
	};
}