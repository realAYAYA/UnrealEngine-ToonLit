// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Island/IslandGroup.h"

namespace Chaos
{
	class FPBDConstraintContainer;
	class FPBDIsland;
	class FPBDIslandManager;

#if CSV_PROFILER
	#define CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(Stat) CSV_CUSTOM_STAT(PhysicsVerbose, Stat, Stats[Stat] * 1000.0, ECsvCustomStatOp::Set);
	#define CSV_SCOPED_ISLANDGROUP_TIMING_STAT(Stat, ThreadIndex) FScopedDurationTimer Timer_##Stat(GetThreadStatAccumulator(ThreadIndex, FIslandGroupStats::Stat))

	/**
	* Accumulate various stats per thread for later aggregation into a global cost.
	* Using aligned to avoid false sharing during perf capture
	*/
	struct alignas(64) FIslandGroupStats
	{
		enum EPerIslandStat
		{
			/** Any stat added to this enum must be added to ReportStats. Can use inl if it's a problem, but seems fine to just copy paste here*/
			PerIslandSolve_GatherTotalSerialized,
			PerIslandSolve_ApplyTotalSerialized,
			PerIslandSolve_ApplyPushOutTotalSerialized,
			PerIslandSolve_ApplyProjectionTotalSerialized,
			PerIslandSolve_ScatterTotalSerialized,
			NumStats
		};

		void ReportStats() const
		{
			CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(PerIslandSolve_GatherTotalSerialized);
			CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(PerIslandSolve_ApplyTotalSerialized);
			CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(PerIslandSolve_ApplyPushOutTotalSerialized);
			CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(PerIslandSolve_ApplyProjectionTotalSerialized);
			CSV_CUSTOM_STAT_ISLANDGROUP_HELPER(PerIslandSolve_ScatterTotalSerialized);
		}

		FIslandGroupStats()
		{
			for (double& Stat : Stats) { Stat = 0.0; }
		}

		static FIslandGroupStats Flatten(const TArray<FIslandGroupStats>& PerGroupStats)
		{
			FIslandGroupStats Flat;
			for (const FIslandGroupStats& GroupStats : PerGroupStats)
			{
				for (int32 Stat = 0; Stat < NumStats; ++Stat)
				{
					Flat.Stats[Stat] += GroupStats.Stats[Stat];
				}
			}

			return Flat;
		}

		double Stats[EPerIslandStat::NumStats];
	};

#else // !CSV_PROFILER
	
	#define CSV_SCOPED_ISLANDGROUP_TIMING_STAT(Stat, ThreadIndex)

#endif


	/**
	 * Assigns Islands to IslandGroups, attempting to have a roughly equal number of constraints per IslandGroup.
	 * Each Island Group may be solved in parallel with the others and contains its own set of ConstraintContainerSolvers
	 * (one for each type of constraint) so that solver data access may be cache efficient during the solver phases.
	*/
	class CHAOS_API FPBDIslandGroupManager
	{
	public:
		UE_NONCOPYABLE(FPBDIslandGroupManager)

		FPBDIslandGroupManager(FPBDIslandManager& InIslandManager);
		~FPBDIslandGroupManager();

		/**
		 * Register a constraint type with the manager. This will create a ConstraintContainerSolver for the ConstraintsContainer.
		 * Constraints are solved in Priority order with lower priorities first, so higher priorities "win".
		*/
		void AddConstraintContainer(FPBDConstraintContainer& ConstraintContainer, const int32 Priority = 0);

		/**
		 * Set the priority for the specified container (that must have been pre-registered with AddConstraintContainer).
		*/
		void SetConstraintContainerPriority(const int32 ContainerId, const int32 Priority);

		/**
		 * The number of groups with at least one constraint in them
		*/
		inline int32 GetNumActiveGroups() const { return NumActiveGroups; }

		/**
		 * Get the specified group
		*/
		inline FPBDIslandConstraintGroupSolver* GetGroup(const int32 GroupIndex) { return IslandGroups[GroupIndex].Get(); }

		/**
		 * Pull all the active islands from the IslandManager and assign to groups.
		 * @return 
		*/
		int32 BuildGroups();

		/**
		 * Set the number of iterations to perform in the Solve step.
		*/
		void SetNumIterations(const int32 InNumPositionIterations, const int32 InNumVelocityIterations, const int32 InNumProjectionIterations);

		/**
		 * Solve all constraints.
		*/
		void Solve(const FReal Dt);


	private:
		// Used to specify a range of bodies or constraints in an island group for more even parallelization of Gather and Scatter
		struct FIslandGroupRange
		{
			FIslandGroupRange(FPBDIslandConstraintGroupSolver* InIslandGroup, const int32 InBeginIndex, const int32 InEndIndex)
				: BeginIndex(InBeginIndex)
				, EndIndex(InEndIndex)
				, IslandGroup(InIslandGroup)
			{
			}

			// Ranges are sorted by range size
			friend bool operator<(const FIslandGroupRange& L, const FIslandGroupRange& R)
			{
				return (L.EndIndex - L.BeginIndex) > (R.EndIndex - R.BeginIndex);
			}

			int32 BeginIndex;
			int32 EndIndex;
			FPBDIslandConstraintGroupSolver* IslandGroup;
		};

		void SolveSerial(const FReal Dt);
		void SolveParallelFor(const FReal Dt);
		void SolveParallelTasks(const FReal Dt);

		void BuildGatherBatches(TArray<FIslandGroupRange>& BodyRanges, TArray<FIslandGroupRange>& ConstraintRanges);
		void SolveGroupConstraints(const int32 GroupIndex, const FReal Dt);


		FPBDIslandManager& IslandManager;
		TArray<TUniquePtr<FPBDIslandConstraintGroupSolver>> IslandGroups;
		int32 NumActiveGroups;
		int32 NumWorkerThreads;
		int32 TargetNumBodiesPerTask;
		int32 TargetNumConstraintsPerTask;
		int32 NumPositionIterations;
		int32 NumVelocityIterations;
		int32 NumProjectionIterations;

#if CSV_PROFILER
		double& GetThreadStatAccumulator(const int32 ThreadIndex, const FIslandGroupStats::EPerIslandStat StatId)
		{
			if (!GroupStats.IsValidIndex(ThreadIndex))
			{
				GroupStats.SetNum(ThreadIndex + 1);
			}
			return GroupStats[ThreadIndex].Stats[StatId];
		}
		TArray<FIslandGroupStats> GroupStats;
#endif
	};

}