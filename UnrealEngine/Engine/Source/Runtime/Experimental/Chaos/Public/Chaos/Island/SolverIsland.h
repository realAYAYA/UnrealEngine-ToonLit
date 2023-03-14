// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ConstraintHandle.h"

namespace Chaos
{
	
	/** Forward Declaration */
	class FPBDIslandManager;
	class FPBDRigidsSOAs;

	/**
	 * Data for a particle in an Island
	*/
	class CHAOS_API FPBDIslandParticle
	{
	public:
		FPBDIslandParticle(FGeometryParticleHandle* InParticleHandle)
			: ParticleHandle(InParticleHandle)
		{
		}

		const FGeometryParticleHandle* GetParticle() const { return ParticleHandle; }
		FGeometryParticleHandle* GetParticle() { return ParticleHandle; }

	private:
		FGeometryParticleHandle* ParticleHandle;
	};

	/**
	* Data for a constraint in an island. This includes the Level and a sort key (which is build from the level and some 
	* externally generated key that should be consistent between frames).
	*/
	class CHAOS_API FPBDIslandConstraint
	{
	public:
		FPBDIslandConstraint(FConstraintHandle* InConstraintHandle, const int32 InLevel, const int32 InColor, const uint32 InSubSortKey)
			: ConstraintHandle(InConstraintHandle)
		{
			// Sort by level, then sort key
			SortKey = (uint64(InLevel) << 32) | uint64(InSubSortKey);
		}

		const FConstraintHandle* GetConstraint() const { return ConstraintHandle; }
		FConstraintHandle* GetConstraint() { return ConstraintHandle; }

		int32 GetLevel() const { return int32((SortKey >> 32) & 0xFFFFF); }
		
		int32 GetColor() const { return INDEX_NONE; }
		
		uint64 GetSortKey() const { return SortKey; }

	private:
		uint64 SortKey;
		FConstraintHandle* ConstraintHandle;
	};

	/**
	* A set of constraints and particles that form a connected set in the constraint graph. These constraints
	* must be solved sequentially (if not colored). FPBDIsland objects are collected into 
	*/
	class CHAOS_API FPBDIsland
	{
	public:

		/**
		* Init the solver island with an island manager and an index
		* @param IslandManager Island manager that is storing the solver islands
		* @param IslandIndex Index of the solver island within that list
		*/
		FPBDIsland(const int32 MaxConstraintContainers);

		/**
		 * This island is about to be reused as a new island. It should be empty, but we need to reset
		 * sleep state and other persistent state.
		 */
		void Reuse();

		/**
		 * The island index as seen by the systems external to the IslandManager.
		 * The IslandManager uses this to index into the IslandIndexing array to get the
		 * internal island index.
		*/
		int32 GetIslandIndex() const { return IslandIndex; }

		/**
		 * Set the external island index.
		 * @see GetIslandIndex.
		*/
		void SetIslandIndex(const int32 InIslandIndex) { IslandIndex = InIslandIndex; }

		/**
		* Clear all the particles from the island
		*/
		void ClearParticles();

		/**
		* Add particle to the island
		* @param ParticleHandle ParticleHandle to be added
		*/
		void AddParticle(FGenericParticleHandle ParticleHandle);

		/**
		* Reset the particles list and reserve a number of particles in memory
		* @param NumParticles Number of particles to be reserved
		*/
		void ReserveParticles(const int32 NumParticles);

		/**
		* Update the particles island index to match the graph index
		*/
		void UpdateParticles();
	
		/**
		* Remove all the constraints from the solver island
		*/
		void ClearConstraints();

		/**
		* Add constraint to the island
		* @param ConstraintHandle ConstraintHandle to be added
		*/
		void AddConstraint(FConstraintHandle* ConstraintHandle, const int32 Level, const int32 Color, const uint32 SubSortKey);

		/**
		* Sort the islands constraints
		*/
		void SortConstraints();

		/**
		* Return the list of particles within the solver island
		*/
		FORCEINLINE TArrayView<const FPBDIslandParticle> GetParticles() const { return MakeArrayView(IslandParticles); }
		FORCEINLINE TArrayView<FPBDIslandParticle> GetParticles() { return MakeArrayView(IslandParticles); }

		/**
		* Return the list of constraints of the specified type within the island
		*/
		TArrayView<const FPBDIslandConstraint> GetConstraints(const int32 ContainerId) const { return MakeArrayView(IslandConstraintsByType[ContainerId]); }
		TArrayView<FPBDIslandConstraint> GetConstraints(const int32 ContainerId) { return MakeArrayView(IslandConstraintsByType[ContainerId]); }

		const int32 GetNumConstraintContainers() const { return IslandConstraintsByType.Num(); }

		/**
		* Get the number of particles within the island
		*/
		FORCEINLINE int32 GetNumParticles() const { return IslandParticles.Num(); }

		/**
		* Get the number of constraints (from the specified container) within the island
		*/
		FORCEINLINE int32 GetNumConstraints(const int32 ContainerId) const { return IslandConstraintsByType[ContainerId].Num(); }

		/**
		* Get the number of constraints (of all types) within the island
		*/
		FORCEINLINE int32 GetNumConstraints() const { return NumConstraints; }

		/**
		* Members accessors
		*/
		FORCEINLINE bool IsSleeping() const { return bIsSleeping; }
		FORCEINLINE void SetIsSleeping(const bool bInIsSleepingIn) { bIsSleeping = bInIsSleepingIn; }
		FORCEINLINE bool IsSleepingChanged() const { return bIsSleepingChanged; }
		FORCEINLINE void SetIsSleepingChanged(const bool bInIsSleepingChanged) { bIsSleepingChanged = bInIsSleepingChanged; }

		FORCEINLINE bool IsPersistent() const { return bIsPersistent; }
		FORCEINLINE void SetIsPersistent(const bool bIsPersistentIn) { bIsPersistent = bIsPersistentIn; }
		FORCEINLINE bool NeedsResim() const { return bNeedsResim; }
		FORCEINLINE void SetNeedsResim(const bool bNeedsResimIn) { bNeedsResim = bNeedsResimIn; }
		FORCEINLINE int32 GetSleepCounter() const { return SleepCounter; }
		FORCEINLINE void SetSleepCounter(const int32 SleepCounterIn) { SleepCounter = SleepCounterIn; }
		FORCEINLINE void SetIsUsingCache(const bool bIsUsingCacheIn ) { bIsUsingCache = bIsUsingCacheIn; }
		FORCEINLINE bool IsUsingCache() const { return bIsUsingCache; }
	
		/**
		 * Set the sleep state of all particles and constraints based on the island sleep state
		*/
		void PropagateSleepState(FPBDRigidsSOAs& Particles);

		void UpdateSyncState(FPBDRigidsSOAs& Particles);

		// Debug functionality
		bool DebugContainsParticle(const FConstGenericParticleHandle& ParticleHandle) const
		{
			for (const FPBDIslandParticle& IslandParticle : IslandParticles)
			{
				if (IslandParticle.GetParticle() == ParticleHandle->Handle())
				{
					return true;
				}
			}
			return false;
		}
		bool DebugContainsConstraint(FConstraintHandle* ConstraintHandle) const
		{
			for (const FPBDIslandConstraint& IslandConstraint : IslandConstraintsByType[ConstraintHandle->GetContainerId()])
			{
				if (ConstraintHandle == IslandConstraint.GetConstraint())
				{
					return true;
				}
			}
			return false;
		}

	private:

		int32 IslandIndex;

		/** Flag to check if an island is awake or sleeping */
		bool bIsSleeping = false;

		/** Flag to check if an island need to be re-simulated or not */
		bool bNeedsResim = false;

		/** Flag to check if an island is persistent over time */
		bool bIsPersistent = true;

		/** Flag to check if the sleeping state has changed or not */
		bool bIsSleepingChanged = false;

		/** Sleep counter to trigger island sleeping */
		int32 SleepCounter = 0;

		/** List of all the island particles handles */
		TArray<FPBDIslandParticle> IslandParticles;

		/** List of all the island constraints handles */
		static constexpr int32 NumExpectedConstraintTypes = 5;
		TArray<TArray<FPBDIslandConstraint>, TInlineAllocator<NumExpectedConstraintTypes>> IslandConstraintsByType;
		int32 NumConstraints;
	
		/** Check if the island is using the cache or not */
		bool bIsUsingCache = false;
	
	};
}