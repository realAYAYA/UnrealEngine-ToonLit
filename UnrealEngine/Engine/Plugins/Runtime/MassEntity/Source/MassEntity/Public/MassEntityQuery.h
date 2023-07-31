// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "MassEntityTypes.h"
#include "MassArchetypeTypes.h"
#include "MassExternalSubsystemTraits.h"
#include "MassRequirements.h"
#include "MassEntityQuery.generated.h"


/** 
 *  FMassEntityQuery is a structure that is used to trigger calculations on cached set of valid archetypes as described 
 *  by requirements. See the parent classes FMassFragmentRequirements and FMassSubsystemRequirements for setting up the 
 *	required fragments and subsystems.
 * 
 *  A query to be considered valid needs declared at least one EMassFragmentPresence::All, EMassFragmentPresence::Any 
 *  EMassFragmentPresence::Optional fragment requirement.
 */
USTRUCT()
struct MASSENTITY_API FMassEntityQuery : public FMassFragmentRequirements, public FMassSubsystemRequirements
{
	GENERATED_BODY()

	friend struct FMassDebugger;

public:
	FMassEntityQuery();
	FMassEntityQuery(std::initializer_list<UScriptStruct*> InitList);
	FMassEntityQuery(TConstArrayView<const UScriptStruct*> InitList);
	FMassEntityQuery(UMassProcessor& Owner);

	void RegisterWithProcessor(UMassProcessor& Owner);

	/** Runs ExecuteFunction on all entities matching Requirements */
	void ForEachEntityChunk(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction);
	
	/** Will first verify that the archetype given with Collection matches the query's requirements, and if so will run the other, more generic ForEachEntityChunk implementation */
	void ForEachEntityChunk(const FMassArchetypeEntityCollection& Collection, FMassEntityManager& EntitySubsystem, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction);

	/** Will gather all archetypes from InEntityManager matching this->Requirements.
	 *  Note that no work will be done if the cached data is up to date (as tracked by EntitySubsystemHash and 
	 *	ArchetypeDataVersion properties). */
	void CacheArchetypes(const FMassEntityManager& InEntityManager);

	void Clear()
	{
		FMassFragmentRequirements::Reset();
		FMassSubsystemRequirements::Reset();
		DirtyCachedData();
	}

	FORCEINLINE void DirtyCachedData()
	{
		EntitySubsystemHash = 0;
		LastUpdatedArchetypeDataVersion = 0;
	}
	
	bool DoesRequireGameThreadExecution() const 
	{ 
		return FMassFragmentRequirements::DoesRequireGameThreadExecution() 
			|| FMassSubsystemRequirements::DoesRequireGameThreadExecution() 
			|| bRequiresMutatingWorldAccess;
	}

	void RequireMutatingWorldAccess() { bRequiresMutatingWorldAccess = true; }

	const TArray<FMassArchetypeHandle>& GetArchetypes() const
	{ 
		return ValidArchetypes; 
	}

	/** 
	 * Goes through ValidArchetypes and sums up the number of entities contained in them.
	 * Note that the function is not const because calling it can result in re-caching of ValidArchetypes 
	 * @return the number of entities this given query would process if called "now"
	 */
	int32 GetNumMatchingEntities(FMassEntityManager& InEntityManager);

	/**
	 * Checks if any of ValidArchetypes has any entities.
	 * Note that the function is not const because calling it can result in re-caching of ValidArchetypes
	 * @return "true" if any of the ValidArchetypes has any entities, "false" otherwise
	 */
	bool HasMatchingEntities(FMassEntityManager& InEntityManager);

	/** 
	 * Sets a chunk filter condition that will applied to each chunk of all valid archetypes. Note 
	 * that this condition won't be applied when a specific entity colleciton is used (via FMassArchetypeEntityCollection )
	 * The value returned by InFunction controls whether to allow execution (true) or block it (false).
	 */
	void SetChunkFilter(const FMassChunkConditionFunction& InFunction) { checkf(!HasChunkFilter(), TEXT("Chunk filter needs to be cleared before setting a new one.")); ChunkCondition = InFunction; }

	void ClearChunkFilter() { ChunkCondition.Reset(); }

	bool HasChunkFilter() const { return bool(ChunkCondition); }

	/** 
	 * If ArchetypeHandle is among ValidArchetypes then the function retrieves requirements mapping cached for it,
	 * otherwise an empty mapping will be returned (and the requirements binding will be done the slow way).
	 */
	const FMassQueryRequirementIndicesMapping& GetFragmentMappingForArchetype(const FMassArchetypeHandle ArchetypeHandle) const;

	void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

protected:
	void ReadCommandlineParams();

private:
	/** 
	 * This function represents a condition that will be called for every chunk to be processed before the actual 
	 * execution function is called. The chunk fragment requirements are already bound and ready to be used by the time 
	 * ChunkCondition is executed.
	 */
	FMassChunkConditionFunction ChunkCondition;

	uint32 EntitySubsystemHash = 0;
	uint32 LastUpdatedArchetypeDataVersion = 0;

	TArray<FMassArchetypeHandle> ValidArchetypes;
	TArray<FMassQueryRequirementIndicesMapping> ArchetypeFragmentMapping;

	uint8 bAllowParallelExecution : 1;
	uint8 bRequiresGameThreadExecution : 1;
	uint8 bRequiresMutatingWorldAccess : 1;

	EMassExecutionContextType ExpectedContextType = EMassExecutionContextType::Local;

#if WITH_MASSENTITY_DEBUG
	uint8 bRegistered : 1;
#endif // WITH_MASSENTITY_DEBUG
};

