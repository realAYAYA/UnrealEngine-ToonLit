// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityQuery.h"
#include "MassDebugger.h"
#include "MassEntityManager.h"
#include "MassArchetypeData.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "Async/ParallelFor.h"
#include "Containers/UnrealString.h"
#include "MassProcessor.h"
#include "MassProcessorDependencySolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntityQuery)

#if WITH_MASSENTITY_DEBUG
#include "MassRequirementAccessDetector.h"
#endif // WITH_MASSENTITY_DEBUG


namespace UE::Mass::Tweakables
{
	/**
	 * Controls whether ParallelForEachEntityChunk actually performs ParallelFor operations. If `false` the call is passed
	 * the the regular ForEachEntityChunk call.
	 */
	bool bAllowParallelExecution = true;

	namespace
	{
		static FAutoConsoleVariableRef AnonymousCVars[] = {
			{	TEXT("mass.AllowQueryParallelFor"), bAllowParallelExecution, TEXT("Controls whether EntityQueries are allowed to utilize ParallelFor construct"), ECVF_Cheat }
		};
	}
}

//-----------------------------------------------------------------------------
// FScopedSubsystemRequirementsRestore
//-----------------------------------------------------------------------------
FMassEntityQuery::FScopedSubsystemRequirementsRestore::FScopedSubsystemRequirementsRestore(FMassExecutionContext& ExecutionContext)
	: CachedExecutionContext(ExecutionContext)
{
	CachedExecutionContext.GetSubsystemRequirementBits(ConstSubsystemsBitSet, MutableSubsystemsBitSet);
}

FMassEntityQuery::FScopedSubsystemRequirementsRestore::~FScopedSubsystemRequirementsRestore()
{
	CachedExecutionContext.SetSubsystemRequirementBits(ConstSubsystemsBitSet, MutableSubsystemsBitSet);
}

//-----------------------------------------------------------------------------
// FMassEntityQuery
//-----------------------------------------------------------------------------
FMassEntityQuery::FMassEntityQuery()
{
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

void FMassEntityQuery::CacheArchetypes(const FMassEntityManager& InEntityManager)
{
	const uint32 InEntityManagerHash = PointerHash(&InEntityManager);

	// Do an incremental update if the last updated archetype data version is different 
    bool bUpdateArchetypes = InEntityManager.GetArchetypeDataVersion() != LastUpdatedArchetypeDataVersion;

	// Force a full update if the entity system changed or if the requirements changed
	if (EntitySubsystemHash != InEntityManagerHash || HasIncrementalChanges())
	{
		bUpdateArchetypes = true;
		EntitySubsystemHash = InEntityManagerHash;
		ValidArchetypes.Reset();
		LastUpdatedArchetypeDataVersion = 0;
		ArchetypeFragmentMapping.Reset();

		if (HasIncrementalChanges())
		{
			ConsumeIncrementalChangesCount();
			if (CheckValidity())
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
		InEntityManager.GetMatchingArchetypes(*this, NewValidArchetypes, LastUpdatedArchetypeDataVersion);
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

void FMassEntityQuery::ForEachEntityChunkInCollections(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections, FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction)
{
	for (const FMassArchetypeEntityCollection& EntityCollection : EntityCollections)
	{
		ForEachEntityChunk(EntityCollection, EntityManager, ExecutionContext, ExecuteFunction);
	}
}

void FMassEntityQuery::ForEachEntityChunk(const FMassArchetypeEntityCollection& EntityCollection, FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction)
{
	// mz@todo I don't like that we're copying data here.
	ExecutionContext.SetEntityCollection(EntityCollection);
	ForEachEntityChunk(EntityManager, ExecutionContext, ExecuteFunction);
	ExecutionContext.ClearEntityCollection();
}

void FMassEntityQuery::ForEachEntityChunk(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction)
{
#if WITH_MASSENTITY_DEBUG
	checkf(ExecutionContext.ExecutionType == ExpectedContextType && (ExpectedContextType == EMassExecutionContextType::Local || bRegistered)
		, TEXT("ExecutionContextType mismatch, make sure all the queries run as part of processor execution are registered with some processor with a FMassEntityQuery::RegisterWithProcessor call"));

	EntityManager.GetRequirementAccessDetector().RequireAccess(*this);
#endif

	FScopedSubsystemRequirementsRestore SubsystemRestore(ExecutionContext);

	if (ExecutionContext.CacheSubsystemRequirements(*this) == false)
	{
		// required subsystems are not available, bail out.
		return;
	}

	if (FMassFragmentRequirements::IsEmpty())
	{
		if (ensureMsgf(ExecutionContext.GetEntityCollection().IsSet(), TEXT("Using empty queries is only supported in combination with Entity Collections that explicitly indicate entities to process")))
		{
			static const FMassQueryRequirementIndicesMapping EmptyMapping;

			const FMassArchetypeHandle& ArchetypeHandle = ExecutionContext.GetEntityCollection().GetArchetype();
			FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
			ArchetypeData.ExecuteFunction(ExecutionContext, ExecuteFunction
				, EmptyMapping
				, ExecutionContext.GetEntityCollection().GetRanges()
				, ChunkCondition);
		}
	}
	else
	{
		// note that the following function will usually only resort to verifying that the data is up to date by
			// checking the version number. In rare cases when it would result in non trivial cost we actually
			// do need those calculations.
		CacheArchetypes(EntityManager);

		// if there's a chunk collection set by the external code - use that
		if (ExecutionContext.GetEntityCollection().IsSet())
		{
			const FMassArchetypeHandle& ArchetypeHandle = ExecutionContext.GetEntityCollection().GetArchetype();
			const int32 ArchetypeIndex = ValidArchetypes.Find(ArchetypeHandle);

			// if given ArchetypeHandle cannot be found in ValidArchetypes then it doesn't match the query's requirements
			if (ArchetypeIndex == INDEX_NONE)
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
				, GetRequirementsMappingForArchetype(ArchetypeHandle)
				, ExecutionContext.GetEntityCollection().GetRanges()
				, ChunkCondition);
		}
		else
		{
			// it's important to set requirements after caching archetypes due to that call potentially sorting the requirements and the order is relevant here.
			ExecutionContext.SetFragmentRequirements(*this);

			for (int i = 0; i < ValidArchetypes.Num(); ++i)
			{
				const FMassArchetypeHandle& ArchetypeHandle = ValidArchetypes[i];
				FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
				ArchetypeData.ExecuteFunction(ExecutionContext, ExecuteFunction, ArchetypeFragmentMapping[i], ChunkCondition);
				ExecutionContext.ClearFragmentViews();
			}
		}
	}

#if WITH_MASSENTITY_DEBUG
	EntityManager.GetRequirementAccessDetector().ReleaseAccess(*this);
#endif

	ExecutionContext.ClearExecutionData();
	ExecutionContext.FlushDeferred();
}

void FMassEntityQuery::ParallelForEachEntityChunkInCollection(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections
	, FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext, const FMassExecuteFunction& ExecuteFunction
	, const EParallelForMode ParallelMode)
{
	if (UE::Mass::Tweakables::bAllowParallelExecution == false && ParallelMode != ForceParallelExecution)
	{
		ForEachEntityChunkInCollections(EntityCollections, EntityManager, ExecutionContext, ExecuteFunction);
		return;
	}

	ParallelFor(EntityCollections.Num(), [this, &EntityManager, &ExecutionContext, &ExecuteFunction, &EntityCollections, ParallelMode](const int32 JobIndex)
	{
		FMassExecutionContext LocalExecutionContext = ExecutionContext; 
		LocalExecutionContext.SetEntityCollection(EntityCollections[JobIndex]);
		ParallelForEachEntityChunk(EntityManager, LocalExecutionContext, ExecuteFunction, ParallelMode);
	});
}

void FMassEntityQuery::ParallelForEachEntityChunk(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext
	, const FMassExecuteFunction& ExecuteFunction, const EParallelForMode ParallelMode)
{
	if (UE::Mass::Tweakables::bAllowParallelExecution == false && ParallelMode != ForceParallelExecution)
	{
		ForEachEntityChunk(EntityManager, ExecutionContext, ExecuteFunction);
		return;
	}

#if WITH_MASSENTITY_DEBUG
	checkf(ExecutionContext.ExecutionType == ExpectedContextType && (ExpectedContextType == EMassExecutionContextType::Local || bRegistered)
		, TEXT("ExecutionContextType mismatch, make sure all the queries run as part of processor execution are registered with some processor with a FMassEntityQuery::RegisterWithProcessor call"));

	EntityManager.GetRequirementAccessDetector().RequireAccess(*this);
#endif

	FScopedSubsystemRequirementsRestore SubsystemRestore(ExecutionContext);

	if (ExecutionContext.CacheSubsystemRequirements(*this) == false)
	{
		// required subsystems are not available, bail out.
		return;
	}

	struct FChunkJob
	{
		FMassArchetypeData& Archetype;
		const int32 ArchetypeIndex;
		const FMassArchetypeEntityCollection::FArchetypeEntityRange EntityRange;
	};
	TArray<FChunkJob> Jobs;

	if (FMassFragmentRequirements::IsEmpty())
	{
		if (ensureMsgf(ExecutionContext.GetEntityCollection().IsSet(), TEXT("Using empty queries is only supported in combination with Entity Collections that explicitly indicate entities to process")))
		{
			const FMassArchetypeHandle& ArchetypeHandle = ExecutionContext.GetEntityCollection().GetArchetype();
			FMassArchetypeData& ArchetypeRef = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
			for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& EntityRange : ExecutionContext.GetEntityCollection().GetRanges())
			{
				Jobs.Add({ ArchetypeRef, INDEX_NONE, EntityRange });
			}
		}
	}
	else
	{

		// note that the following function will usualy only resort to verifying that the data is up to date by
		// checking the version number. In rare cases when it would result in non trivial cost we actually
		// do need those calculations.
		CacheArchetypes(EntityManager);

		// if there's a chunk collection set by the external code - use that
		if (ExecutionContext.GetEntityCollection().IsSet())
		{
			const FMassArchetypeHandle& ArchetypeHandle = ExecutionContext.GetEntityCollection().GetArchetype();
			const int32 ArchetypeIndex = ValidArchetypes.Find(ArchetypeHandle);

			// if given ArchetypeHandle cannot be found in ValidArchetypes then it doesn't match the query's requirements
			if (ArchetypeIndex == INDEX_NONE)
			{
				UE_VLOG_UELOG(EntityManager.GetOwner(), LogMass, Log, TEXT("Attempted to execute FMassEntityQuery with an incompatible Archetype: %s")
					, *FMassDebugger::GetArchetypeRequirementCompatibilityDescription(*this, ExecutionContext.GetEntityCollection().GetArchetype()));

#if WITH_MASSENTITY_DEBUG
				EntityManager.GetRequirementAccessDetector().ReleaseAccess(*this);
#endif // WITH_MASSENTITY_DEBUG
				return;
			}

			ExecutionContext.SetFragmentRequirements(*this);

			FMassArchetypeData& ArchetypeRef = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
			for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& EntityRange : ExecutionContext.GetEntityCollection().GetRanges())
			{
				Jobs.Add({ ArchetypeRef, ArchetypeIndex, EntityRange });
			}
		}
		else
		{
			ExecutionContext.SetFragmentRequirements(*this);
			for (int ArchetypeIndex = 0; ArchetypeIndex < ValidArchetypes.Num(); ++ArchetypeIndex)
			{
				FMassArchetypeHandle& ArchetypeHandle = ValidArchetypes[ArchetypeIndex];
				FMassArchetypeData& ArchetypeRef = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
				const FMassArchetypeEntityCollection AsEntityCollection(ArchetypeHandle);
				for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& EntityRange : AsEntityCollection.GetRanges())
				{
					Jobs.Add({ ArchetypeRef, ArchetypeIndex, EntityRange });
				}
			}
		}
	}

	if (Jobs.Num())
	{
		if (bAllowParallelCommands)
		{
			struct FTaskContext
			{
				FTaskContext()
				{
					CommandBuffer = MakeShared<FMassCommandBuffer>();
				}
				TSharedPtr<FMassCommandBuffer> CommandBuffer;
			};

			TArray<FTaskContext> TaskContext;

			ParallelForWithTaskContext(TaskContext, Jobs.Num(), [this, &ExecutionContext, &ExecuteFunction, &Jobs](FTaskContext& TaskContext, const int32 JobIndex)
				{
					FMassExecutionContext LocalExecutionContext = ExecutionContext;

					LocalExecutionContext.SetDeferredCommandBuffer(TaskContext.CommandBuffer);

					Jobs[JobIndex].Archetype.ExecutionFunctionForChunk(LocalExecutionContext, ExecuteFunction
						, Jobs[JobIndex].ArchetypeIndex != INDEX_NONE ? ArchetypeFragmentMapping[Jobs[JobIndex].ArchetypeIndex] : FMassQueryRequirementIndicesMapping()
						, Jobs[JobIndex].EntityRange
						, ChunkCondition);
				});

			// merge all command buffers
			for (FTaskContext& CommandContext : TaskContext)
			{
				ExecutionContext.Defer().MoveAppend(*CommandContext.CommandBuffer);
			}
		}
		else
		{
			ParallelFor(Jobs.Num(), [this, &ExecutionContext, &ExecuteFunction, &Jobs](const int32 JobIndex)
				{
					FMassExecutionContext LocalExecutionContext = ExecutionContext;
					Jobs[JobIndex].Archetype.ExecutionFunctionForChunk(LocalExecutionContext, ExecuteFunction
						, Jobs[JobIndex].ArchetypeIndex != INDEX_NONE ? ArchetypeFragmentMapping[Jobs[JobIndex].ArchetypeIndex] : FMassQueryRequirementIndicesMapping()
						, Jobs[JobIndex].EntityRange
						, ChunkCondition);
				});
		}
	}

#if WITH_MASSENTITY_DEBUG
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

int32 FMassEntityQuery::GetNumMatchingEntities(TConstArrayView<FMassArchetypeEntityCollection> EntityCollections)
{
	int32 TotalEntities = 0;
	for (const FMassArchetypeEntityCollection& EntityCollection : EntityCollections)
	{
		if (DoesArchetypeMatchRequirements(EntityCollection.GetArchetype()))
		{
			for (const FMassArchetypeEntityCollection::FArchetypeEntityRange& EntityRange : EntityCollection.GetRanges())
			{
				TotalEntities += EntityRange.Length;
			}
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

const FMassQueryRequirementIndicesMapping& FMassEntityQuery::GetRequirementsMappingForArchetype(const FMassArchetypeHandle ArchetypeHandle) const
{
	static const FMassQueryRequirementIndicesMapping FallbackEmptyMapping;
	checkf(HasIncrementalChanges() == false, TEXT("Fetching cached fragments mapping while the query's cached data is out of sync!"));
	const int32 ArchetypeIndex = ValidArchetypes.Find(ArchetypeHandle);
	return ArchetypeIndex != INDEX_NONE ? ArchetypeFragmentMapping[ArchetypeIndex] : FallbackEmptyMapping;
}

void FMassEntityQuery::ExportRequirements(FMassExecutionRequirements& OutRequirements) const
{
	FMassSubsystemRequirements::ExportRequirements(OutRequirements);
	FMassFragmentRequirements::ExportRequirements(OutRequirements);
}
