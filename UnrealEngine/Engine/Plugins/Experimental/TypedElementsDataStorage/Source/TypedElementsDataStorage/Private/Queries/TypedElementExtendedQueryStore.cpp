// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queries/TypedElementExtendedQueryStore.h"

#include "MassProcessingPhaseManager.h"
#include "MassProcessor.h"
#include "Misc/OutputDevice.h"
#include "Processors/TypedElementProcessorAdaptors.h"

const ITypedElementDataStorageInterface::FQueryDescription FTypedElementExtendedQueryStore::EmptyDescription{};

FTypedElementExtendedQueryStore::Handle FTypedElementExtendedQueryStore::RegisterQuery(
	ITypedElementDataStorageInterface::FQueryDescription Query, 
	FTypedElementDatabaseEnvironment& Environment,
	FMassEntityManager& EntityManager, 
	FMassProcessingPhaseManager& PhaseManager)
{
	FTypedElementExtendedQueryStore::Handle Result = Queries.Emplace();
	FTypedElementExtendedQuery& StoredQuery = GetMutableChecked(Result);
	StoredQuery.Description = MoveTemp(Query);

	FMassEntityQuery& NativeQuery =		SetupNativeQuery(StoredQuery.Description, StoredQuery);
	bool bContinueSetup =				SetupSelectedColumns(StoredQuery.Description, NativeQuery);
	bContinueSetup = bContinueSetup &&	SetupConditions(StoredQuery.Description, NativeQuery);
	bContinueSetup = bContinueSetup &&	SetupDependencies(StoredQuery.Description, NativeQuery);
	bContinueSetup = bContinueSetup &&	SetupTickGroupDefaults(StoredQuery.Description);
	bContinueSetup = bContinueSetup &&	SetupProcessors(Result, StoredQuery, Environment, EntityManager, PhaseManager);
	
	if (!bContinueSetup)
	{
		// This will also make the handle invalid.
		Queries.Remove(Result);
	}

	return Result;
}

void FTypedElementExtendedQueryStore::UnregisterQuery(Handle Query, FMassProcessingPhaseManager& PhaseManager)
{
	if (FTypedElementExtendedQuery* QueryData = Get(Query))
	{
		UnregisterQueryData(Query, *QueryData, PhaseManager);
		Queries.Remove(Query);
	}
}

void FTypedElementExtendedQueryStore::Clear(FMassProcessingPhaseManager& PhaseManager)
{
	TickGroupDescriptions.Empty();

	Queries.ListAliveEntries([this, &PhaseManager](Handle Query, FTypedElementExtendedQuery& QueryData)
		{
			if (QueryData.Processor && QueryData.Processor->IsA<UTypedElementQueryObserverCallbackAdapterProcessorBase>())
			{
				// Observers can't be unregistered at this point, so skip these for now.
				return;
			}
			UnregisterQueryData(Query, QueryData, PhaseManager);
		});
}

void FTypedElementExtendedQueryStore::RegisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase,
	FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread)
{
	FTickGroupDescription& Group = TickGroupDescriptions.FindOrAdd({ GroupName, Phase });

	if (!Group.BeforeGroups.Find(BeforeGroup))
	{
		Group.BeforeGroups.Add(BeforeGroup);
	}

	if (!Group.AfterGroups.Find(AfterGroup))
	{
		Group.AfterGroups.Add(AfterGroup);
	}

	if (bRequiresMainThread)
	{
		Group.bRequiresMainThread = true;
	}
}

void FTypedElementExtendedQueryStore::UnregisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase)
{
	TickGroupDescriptions.Remove({ GroupName, Phase });
}

FTypedElementExtendedQuery* FTypedElementExtendedQueryStore::Get(Handle Entry)
{
	return IsAlive(Entry) ? &Queries.Get(Entry) : nullptr;
}

FTypedElementExtendedQuery* FTypedElementExtendedQueryStore::GetMutable(Handle Entry)
{
	return IsAlive(Entry) ? &Queries.GetMutable(Entry) : nullptr;
}

const FTypedElementExtendedQuery* FTypedElementExtendedQueryStore::Get(Handle Entry) const
{
	return IsAlive(Entry) ? &Queries.Get(Entry) : nullptr;
}

FTypedElementExtendedQuery& FTypedElementExtendedQueryStore::GetChecked(Handle Entry)
{
	return Queries.Get(Entry);
}

FTypedElementExtendedQuery& FTypedElementExtendedQueryStore::GetMutableChecked(Handle Entry)
{
	return Queries.GetMutable(Entry);
}

const FTypedElementExtendedQuery& FTypedElementExtendedQueryStore::GetChecked(Handle Entry) const
{
	return Queries.Get(Entry);
}

const ITypedElementDataStorageInterface::FQueryDescription& FTypedElementExtendedQueryStore::GetQueryDescription(Handle Query) const
{
	const FTypedElementExtendedQuery* QueryData = Get(Query);
	return QueryData ? QueryData->Description : EmptyDescription;
}

bool FTypedElementExtendedQueryStore::IsAlive(Handle Entry) const
{
	return Queries.IsAlive(Entry);
}

void FTypedElementExtendedQueryStore::ListAliveEntries(const ListAliveEntriesConstCallback& Callback) const
{
	Queries.ListAliveEntries(Callback);
}

TypedElementDataStorage::FQueryResult FTypedElementExtendedQueryStore::RunQuery(FMassEntityManager& EntityManager, Handle Query)
{
	using ActionType = ITypedElementDataStorageInterface::FQueryDescription::EActionType;
	using CompletionType = ITypedElementDataStorageInterface::FQueryResult::ECompletion;

	ITypedElementDataStorageInterface::FQueryResult Result;

	if (FTypedElementExtendedQuery* QueryData = Get(Query))
	{
		if (QueryData->Description.bSimpleQuery)
		{
			switch (QueryData->Description.Action)
			{
			case ActionType::None:
				Result.Completed = CompletionType::Fully;
				break;
			case ActionType::Select:
				// Fall through: There's nothing to callback to, so only return the total count.
			case ActionType::Count:
				Result.Count = QueryData->NativeQuery.GetNumMatchingEntities(EntityManager);
				Result.Completed = CompletionType::Fully;
				break;
			default:
				Result.Completed = CompletionType::Unsupported;
				break;
			}
		}
		else
		{
			Result.Completed = CompletionType::Unsupported;
		}
	}
	else
	{
		Result.Completed = CompletionType::Unavailable;
	}

	return Result;
}

template<typename CallbackReference>
TypedElementDataStorage::FQueryResult FTypedElementExtendedQueryStore::RunQueryCallbackCommon(
	FMassEntityManager& EntityManager, 
	FTypedElementDatabaseEnvironment& Environment,
	FMassExecutionContext* ParentContext,
	Handle Query,
	CallbackReference Callback)
{
	using ActionType = ITypedElementDataStorageInterface::FQueryDescription::EActionType;
	using CompletionType = ITypedElementDataStorageInterface::FQueryResult::ECompletion;

	ITypedElementDataStorageInterface::FQueryResult Result;

	if (FTypedElementExtendedQuery* QueryData = Get(Query))
	{
		switch (QueryData->Description.Action)
		{
		case ActionType::None:
			Result.Completed = CompletionType::Fully;
			break;
		case ActionType::Select:
			if (!QueryData->Processor.IsValid())
			{
				if constexpr (std::is_same_v<CallbackReference, TypedElementDataStorage::DirectQueryCallbackRef>)
				{
					Result = FTypedElementQueryProcessorData::Execute(
						Callback, QueryData->Description, QueryData->NativeQuery, EntityManager, Environment);
				}
				else
				{
					Result = FTypedElementQueryProcessorData::Execute(
						Callback, QueryData->Description, QueryData->NativeQuery, EntityManager, Environment, *ParentContext);
				}
			}
			else
			{
				Result.Completed = CompletionType::Unsupported;
			}
			break;
		case ActionType::Count:
			// Only the count is requested so no need to trigger the callback.
			Result.Count = QueryData->NativeQuery.GetNumMatchingEntities(EntityManager);
			Result.Completed = CompletionType::Fully;
			break;
		default:
			Result.Completed = CompletionType::Unsupported;
			break;
		}
	}
	else
	{
		Result.Completed = CompletionType::Unavailable;
	}

	return Result;
}

TypedElementDataStorage::FQueryResult FTypedElementExtendedQueryStore::RunQuery(FMassEntityManager& EntityManager, 
	FTypedElementDatabaseEnvironment& Environment, Handle Query, TypedElementDataStorage::DirectQueryCallbackRef Callback)
{
	return RunQueryCallbackCommon(EntityManager, Environment, nullptr, Query, Callback);
}

TypedElementDataStorage::FQueryResult FTypedElementExtendedQueryStore::RunQuery(FMassEntityManager& EntityManager, 
	FTypedElementDatabaseEnvironment& Environment, FMassExecutionContext& ParentContext, Handle Query, 
	TypedElementDataStorage::SubqueryCallbackRef Callback)
{
	return RunQueryCallbackCommon(EntityManager, Environment, &ParentContext, Query, Callback);
}

TypedElementDataStorage::FQueryResult FTypedElementExtendedQueryStore::RunQuery(FMassEntityManager& EntityManager, 
	FTypedElementDatabaseEnvironment& Environment, FMassExecutionContext& ParentContext, Handle Query, TypedElementRowHandle Row,
	TypedElementDataStorage::SubqueryCallbackRef Callback)
{
	using ActionType = ITypedElementDataStorageInterface::FQueryDescription::EActionType;
	using CompletionType = ITypedElementDataStorageInterface::FQueryResult::ECompletion;

	ITypedElementDataStorageInterface::FQueryResult Result;

	if (FTypedElementExtendedQuery* QueryData = Get(Query))
	{
		switch (QueryData->Description.Action)
		{
		case ActionType::None:
			Result.Completed = CompletionType::Fully;
			break;
		case ActionType::Select:
			if (!QueryData->Processor.IsValid())
			{
				Result = FTypedElementQueryProcessorData::Execute(
					Callback, QueryData->Description, Row, QueryData->NativeQuery, EntityManager, Environment, ParentContext);
			}
			else
			{
				Result.Completed = CompletionType::Unsupported;
			}
			break;
		case ActionType::Count:
			// Only the count is requested so no need to trigger the callback.
			Result.Count = 1;
			Result.Completed = CompletionType::Fully;
			break;
		default:
			Result.Completed = CompletionType::Unsupported;
			break;
		}
	}
	else
	{
		Result.Completed = CompletionType::Unavailable;
	}

	return Result;
}
void FTypedElementExtendedQueryStore::RunPhasePreambleQueries(FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment, ITypedElementDataStorageInterface::EQueryTickPhase Phase, float DeltaTime)
{
	RunPhasePreOrPostAmbleQueries(EntityManager, Environment, Phase, DeltaTime, 
		PhasePreparationQueries[static_cast<QueryTickPhaseType>(Phase)]);
}

void FTypedElementExtendedQueryStore::RunPhasePostambleQueries(FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment, ITypedElementDataStorageInterface::EQueryTickPhase Phase, float DeltaTime)
{
	RunPhasePreOrPostAmbleQueries(EntityManager, Environment, Phase, DeltaTime, 
		PhaseFinalizationQueries[static_cast<QueryTickPhaseType>(Phase)]);
}

void FTypedElementExtendedQueryStore::DebugPrintQueryCallbacks(FOutputDevice& Output) const
{
	Output.Log(TEXT("The Typed Elements Data Storage has the following query callbacks:"));
	Queries.ListAliveEntries(
		[&Output](Handle QueryHandle, const FTypedElementExtendedQuery& Query)
		{
			if (Query.Processor)
			{
				Output.Logf(TEXT("    [%s] %s"),
					IsValid(Query.Processor.Get()) ? TEXT("Valid") : TEXT("Invalid"),
					*(Query.Processor->GetProcessorName()));
			}
		});

	for (QueryTickPhaseType PhaseId = 0; PhaseId < static_cast<QueryTickPhaseType>(MaxTickPhase); ++PhaseId)
	{
		for (Handle QueryHandle : PhasePreparationQueries[PhaseId])
		{
			const FTypedElementExtendedQuery& QueryData = GetChecked(QueryHandle);
			Output.Logf(TEXT("    [Valid] %s [Editor Phase Preamble]"), *QueryData.Description.Callback.Name.ToString());
		}
		for (Handle QueryHandle : PhaseFinalizationQueries[PhaseId])
		{
			const FTypedElementExtendedQuery& QueryData = GetChecked(QueryHandle);
			Output.Logf(TEXT("    [Valid] %s [Editor Phase Postamble]"), *QueryData.Description.Callback.Name.ToString());
		}
	}

	Output.Log(TEXT("End of Typed Elements Data Storage query callback list."));
}

FMassEntityQuery& FTypedElementExtendedQueryStore::SetupNativeQuery(
	ITypedElementDataStorageInterface::FQueryDescription& Query, FTypedElementExtendedQuery& StoredQuery)
{
	/**
	 * Mass verifies that queries that are used by processors are on the processor themselves. It does this by taking the address of the query
	 * and seeing if it's within the start and end address of the processor. When a dynamic array is used those addresses are going to be
	 * elsewhere, so the two options are to store a single fixed size array on a processor or have multiple instances. With Mass' queries being
	 * not an insignificant size it's preferable to have several variants with queries to allow the choice for the minimal size. Unfortunately
	 * UHT doesn't allow for templates so it had to be done in an explicit way.
	 */

	using DSI = ITypedElementDataStorageInterface;

	if (Query.Action == DSI::FQueryDescription::EActionType::Select)
	{
		switch (Query.Callback.Type)
		{
		case DSI::EQueryCallbackType::None:
			break;
		case DSI::EQueryCallbackType::Processor:
		{
			UTypedElementQueryProcessorCallbackAdapterProcessorBase* Processor;
			switch (Query.Subqueries.Num())
			{
			case 0:
				Processor = NewObject<UTypedElementQueryProcessorCallbackAdapterProcessor>();
				break;
			case 1:
				Processor = NewObject<UTypedElementQueryProcessorCallbackAdapterProcessorWith1Subquery>();
				break;
			case 2:
				Processor = NewObject<UTypedElementQueryProcessorCallbackAdapterProcessorWith2Subqueries>();
				break;
			case 3:
				Processor = NewObject<UTypedElementQueryProcessorCallbackAdapterProcessorWith3Subqueries>();
				break;
			case 4:
				Processor = NewObject<UTypedElementQueryProcessorCallbackAdapterProcessorWith4Subqueries>();
				break;
			default:
				checkf(false, TEXT("The current Typed Elements Data Storage backend doesn't support %i subqueries per processor query."), 
					Query.Subqueries.Num());
				return StoredQuery.NativeQuery;
			}
			StoredQuery.Processor.Reset(Processor);
			return Processor->GetQuery();
		}
		case DSI::EQueryCallbackType::ObserveAdd:
			// Fall-through
		case DSI::EQueryCallbackType::ObserveRemove:
		{
			UTypedElementQueryObserverCallbackAdapterProcessorBase* Observer;
			switch (Query.Subqueries.Num())
			{
			case 0:
				Observer = NewObject<UTypedElementQueryObserverCallbackAdapterProcessor>();
				break;
			case 1:
				Observer = NewObject<UTypedElementQueryObserverCallbackAdapterProcessorWith1Subquery>();
				break;
			case 2:
				Observer = NewObject<UTypedElementQueryObserverCallbackAdapterProcessorWith2Subqueries>();
				break;
			case 3:
				Observer = NewObject<UTypedElementQueryObserverCallbackAdapterProcessorWith3Subqueries>();
				break;
			case 4:
				Observer = NewObject<UTypedElementQueryObserverCallbackAdapterProcessorWith4Subqueries>();
				break;
			default:
				checkf(false, TEXT("The current Typed Elements Data Storage backend doesn't support %i subqueries per observer query."),
					Query.Subqueries.Num());
				return StoredQuery.NativeQuery;
			}
			StoredQuery.Processor.Reset(Observer);
			return Observer->GetQuery();
		}
		case DSI::EQueryCallbackType::PhasePreparation:
			break;
		case DSI::EQueryCallbackType::PhaseFinalization:
			break;
		default:
			checkf(false, TEXT("Unsupported query callback type %i."), static_cast<int>(Query.Callback.Type));
			break;
		}
	}
	return StoredQuery.NativeQuery;
}

bool FTypedElementExtendedQueryStore::SetupSelectedColumns(
	ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery)
{
	using DSI = ITypedElementDataStorageInterface;

	switch (Query.Action)
	{
	case DSI::FQueryDescription::EActionType::None:
		return true;
	case DSI::FQueryDescription::EActionType::Select:
	{
		const int32 SelectionCount = Query.SelectionTypes.Num();
		if(ensureMsgf(SelectionCount == Query.SelectionAccessTypes.Num(),
			TEXT("The number of query selection types (%i) doesn't match the number of selection access types (%i)."),
				SelectionCount, Query.SelectionAccessTypes.Num()))
		{
			for (int SelectionIndex = 0; SelectionIndex < SelectionCount; ++SelectionIndex)
			{
				TWeakObjectPtr<const UScriptStruct>& Type = Query.SelectionTypes[SelectionIndex];
				
				if (ensureMsgf(Type.IsValid(), TEXT("Provided query selection type can not be null.")) &&
					ensureMsgf(
						Type->IsChildOf(FTypedElementDataStorageColumn::StaticStruct()) ||
						Type->IsChildOf(FMassFragment::StaticStruct()),
						TEXT("Provided query selection type '%s' is not based on FTypedElementDataStorageColumn or another supported base type."),
						*Type->GetStructPathName().ToString()))
				{
					NativeQuery.AddRequirement(Type.Get(), ConvertToNativeAccessType(Query.SelectionAccessTypes[SelectionIndex]));
				}
				else
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}
	case DSI::FQueryDescription::EActionType::Count:
	{
		bool bIsSelectionEmpty = Query.SelectionTypes.IsEmpty();
		bool bIsAccessTypesEmpty = Query.SelectionAccessTypes.IsEmpty();
		checkf(bIsSelectionEmpty, TEXT("Count queries for the Typed Elements Data Storage can't have entries for selection."));
		checkf(bIsAccessTypesEmpty, TEXT("Count queries for the Typed Elements Data Storage can't have entries for selection."));
		return bIsSelectionEmpty && bIsAccessTypesEmpty;
	}
	default:
		checkf(false, TEXT("Unexpected query action: %i."), static_cast<int32>(Query.Action));
		return false;
	}
}

bool FTypedElementExtendedQueryStore::SetupConditions(
	ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery)
{
	using DSI = ITypedElementDataStorageInterface;

	if (Query.ConditionTypes.IsEmpty())
	{
		return true;
	}

	if (ensureMsgf(Query.bSimpleQuery, TEXT("The Data Storage back-end currently only supports simple queries.")))
	{
		if (ensureMsgf(Query.ConditionTypes.Num() == Query.ConditionOperators.Num(),
			TEXT("The types and operators for a typed element query have gone out of sync.")))
		{
			const DSI::FQueryDescription::FOperator* Operand = Query.ConditionOperators.GetData();
			for (DSI::FQueryDescription::EOperatorType Type : Query.ConditionTypes)
			{
				EMassFragmentPresence Presence;
				switch (Type)
				{
				case DSI::FQueryDescription::EOperatorType::SimpleAll:
					Presence = EMassFragmentPresence::All;
					break;
				case DSI::FQueryDescription::EOperatorType::SimpleAny:
					Presence = EMassFragmentPresence::Any;
					break;
				case DSI::FQueryDescription::EOperatorType::SimpleNone:
					Presence = EMassFragmentPresence::None;
					break;
				default:
					continue;
				}

				if (Operand->Type->IsChildOf(FMassTag::StaticStruct()))
				{
					NativeQuery.AddTagRequirement(*(Operand->Type), Presence);
				}
				else if (Operand->Type->IsChildOf(FMassFragment::StaticStruct()))
				{
					NativeQuery.AddRequirement(Operand->Type.Get(), EMassFragmentAccess::None, Presence);
				}

				++Operand;
			}
			return true;
		}
	}
	return false;
}

bool FTypedElementExtendedQueryStore::SetupDependencies(
	ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery)
{
	using DSI = ITypedElementDataStorageInterface;

	const int32 DependencyCount = Query.DependencyTypes.Num();
	if (ensureMsgf(DependencyCount == Query.DependencyFlags.Num() && DependencyCount == Query.CachedDependencies.Num(),
		TEXT("The number of query dependencies (%i) doesn't match the number of dependency access types (%i) and/or cached dependencies count (%i)."),
		DependencyCount, Query.DependencyFlags.Num(), Query.CachedDependencies.Num()))
	{
		for (int32 DependencyIndex = 0; DependencyIndex < DependencyCount; ++DependencyIndex)
		{
			TWeakObjectPtr<const UClass>& Type = Query.DependencyTypes[DependencyIndex];
			if (ensureMsgf(Type.IsValid(), TEXT("Provided query dependency type can not be null.")) &&
				ensureMsgf(Type->IsChildOf<USubsystem>(), TEXT("Provided query dependency type '%s' is not based on USubSystem."),
					*Type->GetStructPathName().ToString()))
			{
				DSI::EQueryDependencyFlags Flags = Query.DependencyFlags[DependencyIndex];
				NativeQuery.AddSubsystemRequirement(
					const_cast<UClass*>(Type.Get()),
					EnumHasAllFlags(Flags, DSI::EQueryDependencyFlags::ReadOnly) ? EMassFragmentAccess::ReadOnly : EMassFragmentAccess::ReadWrite,
					EnumHasAllFlags(Flags, DSI::EQueryDependencyFlags::GameThreadBound));
			}
			else
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

bool FTypedElementExtendedQueryStore::SetupTickGroupDefaults(ITypedElementDataStorageInterface::FQueryDescription& Query)
{
	const FTickGroupDescription* TickGroup = TickGroupDescriptions.Find({ Query.Callback.Group, Query.Callback.Phase });
	if (TickGroup)
	{
		for (auto It = Query.Callback.BeforeGroups.CreateIterator(); It; ++It)
		{
			if (TickGroup->BeforeGroups.Contains(*It))
			{
				It.RemoveCurrentSwap();
			}
		}
		Query.Callback.BeforeGroups.Append(TickGroup->BeforeGroups);
		
		for (auto It = Query.Callback.AfterGroups.CreateIterator(); It; ++It)
		{
			if (TickGroup->AfterGroups.Contains(*It))
			{
				It.RemoveCurrentSwap();
			}
		}
		Query.Callback.AfterGroups.Append(TickGroup->AfterGroups);
		
		Query.Callback.bForceToGameThread |= TickGroup->bRequiresMainThread;
	}
	return true;
}

bool FTypedElementExtendedQueryStore::SetupProcessors(Handle QueryHandle, FTypedElementExtendedQuery& StoredQuery, 
	FTypedElementDatabaseEnvironment& Environment, FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager)
{
	using DSI = ITypedElementDataStorageInterface;

	// Register Phase processors locally.
	switch (StoredQuery.Description.Callback.Type)
	{
	case DSI::EQueryCallbackType::PhasePreparation:
		RegisterPreambleQuery(StoredQuery.Description.Callback.Phase, QueryHandle);
		break;
	case DSI::EQueryCallbackType::PhaseFinalization:
		RegisterPostambleQuery(StoredQuery.Description.Callback.Phase, QueryHandle);
		break;
	}

	// Register regular processors and observer with Mass.
	if (StoredQuery.Processor)
	{
		if (StoredQuery.Processor->IsA<UTypedElementQueryProcessorCallbackAdapterProcessorBase>())
		{
			if (static_cast<UTypedElementQueryProcessorCallbackAdapterProcessorBase*>(StoredQuery.Processor.Get())->
				ConfigureQueryCallback(StoredQuery, QueryHandle, *this, Environment))
			{
				PhaseManager.RegisterDynamicProcessor(*StoredQuery.Processor);
			}
			else
			{
				return false;
			}
		}
		else if (StoredQuery.Processor->IsA<UTypedElementQueryObserverCallbackAdapterProcessorBase>())
		{
			if (UTypedElementQueryObserverCallbackAdapterProcessorBase* Observer =
				static_cast<UTypedElementQueryObserverCallbackAdapterProcessorBase*>(StoredQuery.Processor.Get()))
			{
				Observer->ConfigureQueryCallback(StoredQuery, QueryHandle, *this, Environment);
				EntityManager.GetObserverManager().AddObserverInstance(*Observer->GetObservedType(), Observer->GetObservedOperation(), *Observer);
			}
			else
			{
				return false;
			}
		}
		else
		{
			checkf(false, TEXT("Query processor %s is of unsupported type %s."),
				*StoredQuery.Description.Callback.Name.ToString(), *StoredQuery.Processor->GetSparseClassDataStruct()->GetName());
			return false;
		}
	}
	return true;
}

EMassFragmentAccess FTypedElementExtendedQueryStore::ConvertToNativeAccessType(ITypedElementDataStorageInterface::EQueryAccessType AccessType)
{
	switch (AccessType)
	{
	case ITypedElementDataStorageInterface::EQueryAccessType::ReadOnly:
		return EMassFragmentAccess::ReadOnly;
	case ITypedElementDataStorageInterface::EQueryAccessType::ReadWrite:
		return EMassFragmentAccess::ReadWrite;
	default:
		checkf(false, TEXT("Invalid query access type: %i."), static_cast<uint32>(AccessType));
		return EMassFragmentAccess::MAX;
	}
}

void FTypedElementExtendedQueryStore::RegisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query)
{
	PhasePreparationQueries[static_cast<QueryTickPhaseType>(Phase)].Add(Query);
}

void FTypedElementExtendedQueryStore::RegisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query)
{
	PhaseFinalizationQueries[static_cast<QueryTickPhaseType>(Phase)].Add(Query);
}

void FTypedElementExtendedQueryStore::UnregisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query)
{
	int32 Index;
	if (PhasePreparationQueries[static_cast<QueryTickPhaseType>(Phase)].Find(Query, Index))
	{
		PhasePreparationQueries[static_cast<QueryTickPhaseType>(Phase)].RemoveAt(Index);
	}
}

void FTypedElementExtendedQueryStore::UnregisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query)
{
	int32 Index;
	if (PhaseFinalizationQueries[static_cast<QueryTickPhaseType>(Phase)].Find(Query, Index))
	{
		PhaseFinalizationQueries[static_cast<QueryTickPhaseType>(Phase)].RemoveAt(Index);
	}
}

void FTypedElementExtendedQueryStore::RunPhasePreOrPostAmbleQueries(FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment, ITypedElementDataStorageInterface::EQueryTickPhase Phase,
	float DeltaTime, TArray<Handle>& QueryHandles)
{
	if (!QueryHandles.IsEmpty())
	{
		FPhasePreOrPostAmbleExecutor Executor(EntityManager, DeltaTime);
		for (Handle Query : QueryHandles)
		{
			FTypedElementExtendedQuery& QueryData = Queries.Get(Query);
			Executor.ExecuteQuery(QueryData.Description, *this, Environment, QueryData.NativeQuery, QueryData.Description.Callback.Function);
		}
	}
}

void FTypedElementExtendedQueryStore::UnregisterQueryData(Handle Query, FTypedElementExtendedQuery& QueryData, FMassProcessingPhaseManager& PhaseManager)
{
	if (QueryData.Processor)
	{
		if (QueryData.Processor->IsA<UTypedElementQueryProcessorCallbackAdapterProcessorBase>())
		{
			PhaseManager.UnregisterDynamicProcessor(*QueryData.Processor);
		}
		else if (QueryData.Processor->IsA<UTypedElementQueryObserverCallbackAdapterProcessorBase>())
		{
			UTypedElementQueryObserverCallbackAdapterProcessorBase* Observer =
				static_cast<UTypedElementQueryObserverCallbackAdapterProcessorBase*>(QueryData.Processor.Get());
			if (ensure(Observer && PhaseManager.GetEntityManager().IsValid()))
			{
				PhaseManager.GetEntityManager()->GetObserverManager().RemoveObserverInstance(*Observer->GetObservedType(), Observer->GetObservedOperation(), *Observer);
			}
		}
		else
		{
			checkf(false, TEXT("Query processor %s is of unsupported type %s."),
				*QueryData.Description.Callback.Name.ToString(), *QueryData.Processor->GetSparseClassDataStruct()->GetName());
		}
	}
	else if (QueryData.Description.Callback.Type == ITypedElementDataStorageInterface::EQueryCallbackType::PhasePreparation)
	{
		UnregisterPreambleQuery(QueryData.Description.Callback.Phase, Query);
	}
	else if (QueryData.Description.Callback.Type == ITypedElementDataStorageInterface::EQueryCallbackType::PhaseFinalization)
	{
		UnregisterPostambleQuery(QueryData.Description.Callback.Phase, Query);
	}
	else
	{
		QueryData.NativeQuery.Clear();
	}
}
