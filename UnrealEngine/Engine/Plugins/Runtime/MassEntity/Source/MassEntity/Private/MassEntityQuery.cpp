// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityQuery.h"
#include "MassDebugger.h"
#include "MassEntityManager.h"
#include "MassArchetypeData.h"
#include "MassCommandBuffer.h"
#include "VisualLogger/VisualLogger.h"
#include "Async/ParallelFor.h"
#include "Containers/UnrealString.h"
#include "MassProcessor.h"
#include "MassProcessorDependencySolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntityQuery)

#if WITH_MASSENTITY_DEBUG
#include "MassRequirementAccessDetector.h"
#endif // WITH_MASSENTITY_DEBUG


//////////////////////////////////////////////////////////////////////
// FMassEntityQuery

FMassEntityQuery::FMassEntityQuery()
{
	bAllowParallelExecution = false;
	bRequiresGameThreadExecution = false;
	bRequiresMutatingWorldAccess = false;

	ReadCommandlineParams();
}

FMassEntityQuery::FMassEntityQuery(std::initializer_list<UScriptStruct*> InitList)
	: FMassEntityQuery()
{
	for (const UScriptStruct* FragmentType : InitList)
	{
		AddRequirement(FragmentType, EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	}
}

FMassEntityQuery::FMassEntityQuery(TConstArrayView<const UScriptStruct*> InitList)
	: FMassEntityQuery()
{
	for (const UScriptStruct* FragmentType : InitList)
	{
		AddRequirement(FragmentType, EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
	}
}

FMassEntityQuery::FMassEntityQuery(UMassProcessor& Owner)
{
	RegisterWithProcessor(Owner);
}

void FMassEntityQuery::RegisterWithProcessor(UMassProcessor& Owner)
{
	ExpectedContextType = EMassExecutionContextType::Processor;
	Owner.RegisterQuery(*this);
#if WITH_MASSENTITY_DEBUG
	bRegistered = true;
#endif // WITH_MASSENTITY_DEBUG
}

void FMassEntityQuery::ReadCommandlineParams()
{
	int AllowParallelQueries = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ParallelMassQueries="), AllowParallelQueries))
	{
		bAllowParallelExecution = (AllowParallelQueries != 0);
	}
}

void FMassEntityQuery::CacheArchetypes(const FMassEntityManager& InEntityManager)
{
	const uint32 InEntityManagerHash = PointerHash(&InEntityManager);

	// Do an incremental update if the last updated archetype data version is different 
    bool bUpdateArchetypes = InEntityManager.GetArchetypeDataVersion() != LastUpdatedArchetypeDataVersion;

	// Force a full update if the entity system changed or if the requirements changed
	if (EntitySubsystemHash != InEntityManagerHash || IncrementalChangesCount)
	{
		bUpdateArchetypes = true;
		EntitySubsystemHash = InEntityManagerHash;
		ValidArchetypes.Reset();
		LastUpdatedArchetypeDataVersion = 0;
		ArchetypeFragmentMapping.Reset();

		if (IncrementalChangesCount)
		{
			IncrementalChangesCount = 0;
			if( CheckValidity() )
			{
				SortRequirements();
			}
			else
			{
				bUpdateArchetypes = false;
				UE_VLOG_UELOG(InEntityManager.GetOwner(), LogMass, Error, TEXT("FMassEntityQuery::CacheArchetypes: requirements not valid: %s"), *FMassDebugger::GetRequirementsDescription(*this));
			}
		}
	}
	
	// Process any new archetype that is newer than the LastUpdatedArchetypeDataVersion
	if (bUpdateArchetypes)
	{
		TArray<FMassArchetypeHandle> NewValidArchetypes;
		InEntityManager.GetValidArchetypes(*this, NewValidArchetypes, LastUpdatedArchetypeDataVersion);
		LastUpdatedArchetypeDataVersion = InEntityManager.GetArchetypeDataVersion();
		if (NewValidArchetypes.Num())
		{
			const int32 FirstNewArchetype = ValidArchetypes.Num();
			ValidArchetypes.Append(NewValidArchetypes);

			TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass RequirementsBinding")
			const TConstArrayView<FMassFragmentRequirementDescription> LocalRequirements = GetFragmentRequirements();
			ArchetypeFragmentMapping.AddDefaulted(NewValidArchetypes.Num());
			for (int i = FirstNewArchetype; i < ValidArchetypes.Num(); ++i)
			{
				FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ValidArchetypes[i]);
				ArchetypeData.GetRequirementsFragmentMapping(LocalRequirements, ArchetypeFragmentMapping[i].EntityFragments);
				if (ChunkFragmentRequirements.Num())
				{
					ArchetypeData.GetRequirementsChunkFragmentMapping(ChunkFragmentRequirements, ArchetypeFragmentMapping[i].ChunkFragments);
				}
				if (ConstSharedFragmentRequirements.Num())
				{
					ArchetypeData.GetRequirementsConstSharedFragmentMapping(ConstSharedFragmentRequirements, ArchetypeFragmentMapping[i].ConstSharedFragments);
				}
				if (SharedFragmentRequirements.Num())
				{
					ArchetypeData.GetRequirementsSharedFragmentMapping(SharedFragmentRequirements, ArchetypeFragmentMapping[i].SharedFragments);
				}
			}
		}
	}
}

void FMassEntityQuery::ForEachEntityChunk(const FMassArchetypeEntityCollection& Collection, FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction)
{
	// mz@todo I don't like that we're copying data here.
	ExecutionContext.SetEntityCollection(Collection);
	ForEachEntityChunk(EntityManager, ExecutionContext, ExecuteFunction);
	ExecutionContext.ClearEntityCollection();
}

void FMassEntityQuery::ForEachEntityChunk(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction)
{
#if WITH_MASSENTITY_DEBUG
	int32 NumEntitiesToProcess = 0;

	checkf(ExecutionContext.ExecutionType == ExpectedContextType && (ExpectedContextType == EMassExecutionContextType::Local || bRegistered)
		, TEXT("ExecutionContextType mismatch, make sure all the queries run as part of processor execution are registered with some processor with a FMassEntityQuery::RegisterWithProcessor call"));

	EntityManager.GetRequirementAccessDetector().RequireAccess(*this);
#endif

	struct FScopedSubsystemRequirementsRestore
	{
		FScopedSubsystemRequirementsRestore(FMassExecutionContext& ExecutionContext)
			: CachedExecutionContext(ExecutionContext)
		{
			ConstSubsystemsBitSet = ExecutionContext.ConstSubsystemsBitSet;
			MutableSubsystemsBitSet = ExecutionContext.MutableSubsystemsBitSet;
		}

		~FScopedSubsystemRequirementsRestore()
		{
			CachedExecutionContext.ConstSubsystemsBitSet = ConstSubsystemsBitSet;
			CachedExecutionContext.MutableSubsystemsBitSet = MutableSubsystemsBitSet;
		}

		FMassExecutionContext& CachedExecutionContext;
		FMassExternalSubsystemBitSet ConstSubsystemsBitSet;
		FMassExternalSubsystemBitSet MutableSubsystemsBitSet;
	};
	FScopedSubsystemRequirementsRestore SubsystemRestore(ExecutionContext);

	ExecutionContext.SetSubsystemRequirements(*this);

	// if there's a chunk collection set by the external code - use that
	if (ExecutionContext.GetEntityCollection().IsSet())
	{
		const FMassArchetypeHandle& ArchetypeHandle = ExecutionContext.GetEntityCollection().GetArchetype();
		// verify the archetype matches requirements
		if (DoesArchetypeMatchRequirements(ArchetypeHandle) == false)
		{
			UE_VLOG_UELOG(EntityManager.GetOwner(), LogMass, Log, TEXT("Attempted to execute FMassEntityQuery with an incompatible Archetype: %s")
				, *FMassDebugger::GetArchetypeRequirementCompatibilityDescription(*this, ArchetypeHandle));

#if WITH_MASSENTITY_DEBUG
			EntityManager.GetRequirementAccessDetector().ReleaseAccess(*this);
#endif // WITH_MASSENTITY_DEBUG
			return;
		}
		ExecutionContext.SetFragmentRequirements(*this);
		
		FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
		ArchetypeData.ExecuteFunction(ExecutionContext, ExecuteFunction
			, GetFragmentMappingForArchetype(ArchetypeHandle)
			, ExecutionContext.GetEntityCollection().GetRanges());
#if WITH_MASSENTITY_DEBUG
		NumEntitiesToProcess = ExecutionContext.GetNumEntities();
#endif
	}
	else
	{
		CacheArchetypes(EntityManager);
		// it's important to set requirements after caching archetypes due to that call potentially sorting the requirements and the order is relevant here.
		ExecutionContext.SetFragmentRequirements(*this);

		for (int i = 0; i < ValidArchetypes.Num(); ++i)
		{
			const FMassArchetypeHandle& ArchetypeHandle = ValidArchetypes[i];
			FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
			ArchetypeData.ExecuteFunction(ExecutionContext, ExecuteFunction, ArchetypeFragmentMapping[i], ChunkCondition);
			ExecutionContext.ClearFragmentViews();
#if WITH_MASSENTITY_DEBUG
			NumEntitiesToProcess += ExecutionContext.GetNumEntities();
#endif
		}
	}

#if WITH_MASSENTITY_DEBUG
	// Not using VLOG to be thread safe
	UE_CLOG(!ExecutionContext.DebugGetExecutionDesc().IsEmpty(), LogMass, VeryVerbose,
		TEXT("%s: %d entities sent for processing"), *ExecutionContext.DebugGetExecutionDesc(), NumEntitiesToProcess);

	EntityManager.GetRequirementAccessDetector().ReleaseAccess(*this);
#endif

	ExecutionContext.ClearExecutionData();
	ExecutionContext.FlushDeferred();
}

int32 FMassEntityQuery::GetNumMatchingEntities(FMassEntityManager& InEntityManager)
{
	CacheArchetypes(InEntityManager);
	int32 TotalEntities = 0;
	for (FMassArchetypeHandle& ArchetypeHandle : ValidArchetypes)
	{
		if (const FMassArchetypeData* Archetype = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle))
		{
			TotalEntities += Archetype->GetNumEntities();
		}
	}
	return TotalEntities;
}

bool FMassEntityQuery::HasMatchingEntities(FMassEntityManager& InEntityManager)
{
	CacheArchetypes(InEntityManager);

	for (FMassArchetypeHandle& ArchetypeHandle : ValidArchetypes)
	{
		const FMassArchetypeData* Archetype = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle);
		if (Archetype && Archetype->GetNumEntities() > 0)
		{
			return true;
		}
	}
	return false;
}

const FMassQueryRequirementIndicesMapping& FMassEntityQuery::GetFragmentMappingForArchetype(const FMassArchetypeHandle ArchetypeHandle) const
{
	static const FMassQueryRequirementIndicesMapping FallbackEmptyMapping;
	const int32 ArchetypeIndex = ValidArchetypes.Find(ArchetypeHandle);
	return ArchetypeIndex != INDEX_NONE ? ArchetypeFragmentMapping[ArchetypeIndex] : FallbackEmptyMapping;
}

void FMassEntityQuery::ExportRequirements(FMassExecutionRequirements& OutRequirements) const
{
	FMassSubsystemRequirements::ExportRequirements(OutRequirements);
	FMassFragmentRequirements::ExportRequirements(OutRequirements);
}

