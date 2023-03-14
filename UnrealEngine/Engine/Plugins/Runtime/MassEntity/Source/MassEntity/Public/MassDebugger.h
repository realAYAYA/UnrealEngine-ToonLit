// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessingTypes.h"
#if WITH_MASSENTITY_DEBUG
#include "Containers/ContainersFwd.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "Misc/SpinLock.h"
#endif // WITH_MASSENTITY_DEBUG

class FOutputDevice;
class UMassProcessor;
struct FMassEntityQuery;
struct FMassEntityManager;
struct FMassArchetypeHandle;
struct FMassFragmentRequirements;
struct FMassFragmentRequirementDescription;
enum class EMassFragmentAccess : uint8;
enum class EMassFragmentPresence : uint8;

namespace UE::Mass::Debug
{
#if WITH_MASSENTITY_DEBUG
struct MASSENTITY_API FQueryRequirementsView
{
	TConstArrayView<FMassFragmentRequirementDescription> FragmentRequirements;
	TConstArrayView<FMassFragmentRequirementDescription> ChunkRequirements;
	TConstArrayView<FMassFragmentRequirementDescription> ConstSharedRequirements;
	TConstArrayView<FMassFragmentRequirementDescription> SharedRequirements;
	const FMassTagBitSet& RequiredAllTags;
	const FMassTagBitSet& RequiredAnyTags;
	const FMassTagBitSet& RequiredNoneTags;
	const FMassExternalSubsystemBitSet& RequiredConstSubsystems;
	const FMassExternalSubsystemBitSet& RequiredMutableSubsystems;
};

FString DebugGetFragmentAccessString(EMassFragmentAccess Access);
MASSENTITY_API extern void DebugOutputDescription(TConstArrayView<UMassProcessor*> Processors, FOutputDevice& Ar);
#endif // WITH_MASSENTITY_DEBUG

struct FArchetypeStats
{
	/** Number of active entities of the archetype. */
	int32 EntitiesCount = 0;
	/** Number of entities that fit per chunk. */
	int32 EntitiesCountPerChunk = 0;
	/** Number of allocated chunks. */
	int32 ChunksCount = 0;
	/** Total amount of memory taken by this archetype */
	SIZE_T AllocatedSize = 0;
};
} // namespace UE::Mass::Debug

#if WITH_MASSENTITY_DEBUG

struct MASSENTITY_API FMassDebugger
{
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEntitySelected, const FMassEntityManager&, const FMassEntityHandle);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMassEntityManagerEvent, const FMassEntityManager&);


	static TConstArrayView<FMassEntityQuery*> GetProcessorQueries(const UMassProcessor& Processor);
	/** fetches all queries registered for given Processor. Note that in order to get up to date information
	 *  FMassEntityQuery::CacheArchetypes will be called on each query */
	static TConstArrayView<FMassEntityQuery*> GetUpToDateProcessorQueries(const FMassEntityManager& EntitySubsystem, UMassProcessor& Processor);

	static UE::Mass::Debug::FQueryRequirementsView GetQueryRequirements(const FMassEntityQuery& Query);
	static void GetQueryExecutionRequirements(const FMassEntityQuery& Query, FMassExecutionRequirements& OutExecutionRequirements);

	static TArray<FMassArchetypeHandle> GetAllArchetypes(const FMassEntityManager& EntitySubsystem);
	static const FMassArchetypeCompositionDescriptor& GetArchetypeComposition(const FMassArchetypeHandle& ArchetypeHandle);

	static void GetArchetypeEntityStats(const FMassArchetypeHandle& ArchetypeHandle, UE::Mass::Debug::FArchetypeStats& OutStats);
	static const TConstArrayView<FName> GetArchetypeDebugNames(const FMassArchetypeHandle& ArchetypeHandle);

	static TConstArrayView<UMassCompositeProcessor::FDependencyNode> GetProcessingGraph(const UMassCompositeProcessor& GraphOwner);
	
	static FString GetSingleRequirementDescription(const FMassFragmentRequirementDescription& Requirement);
	static FString GetRequirementsDescription(const FMassFragmentRequirements& Requirements);
	static FString GetArchetypeRequirementCompatibilityDescription(const FMassFragmentRequirements& Requirements, const FMassArchetypeHandle& ArchetypeHandle);

	static void OutputArchetypeDescription(FOutputDevice& Ar, const FMassArchetypeHandle& Archetype);
	static void OutputEntityDescription(FOutputDevice& Ar, const FMassEntityManager& EntityManager, const int32 EntityIndex, const TCHAR* InPrefix = TEXT(""));
	static void OutputEntityDescription(FOutputDevice& Ar, const FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const TCHAR* InPrefix = TEXT(""));

	static void SelectEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);

	static FOnEntitySelected OnEntitySelectedDelegate;

	static FOnMassEntityManagerEvent OnEntityManagerInitialized;
	static FOnMassEntityManagerEvent OnEntityManagerDeinitialized;

	static void RegisterEntityManager(FMassEntityManager& EntityManager);
	static void UnregisterEntityManager(FMassEntityManager& EntityManager);
	static TConstArrayView<TWeakPtr<const FMassEntityManager>> GetEntityManagers() { return ActiveEntityManagers; }

private:
	static TArray<TWeakPtr<const FMassEntityManager>> ActiveEntityManagers;
	static UE::FSpinLock EntityManagerRegistrationLock;
};

#else

struct MASSENTITY_API FMassDebugger
{
	static FString GetSingleRequirementDescription(const FMassFragmentRequirementDescription& Requirement) { return TEXT("[no debug information]"); }
	static FString GetRequirementsDescription(const FMassFragmentRequirements& Requirements) { return TEXT("[no debug information]"); }
	static FString GetArchetypeRequirementCompatibilityDescription(const FMassFragmentRequirements& Requirements, const FMassArchetypeHandle& ArchetypeHandle) { return TEXT("[no debug information]"); }
};

#endif // WITH_MASSENTITY_DEBUG
