// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassEntityQuery.h"
#include "MassRequirements.h"
#include "TypedElementHandleStore.h"
#include "UObject/StrongObjectPtr.h"

struct FMassEntityManager;
struct FMassProcessingPhaseManager;
class UMassProcessor;
class FOutputDevice;
class FTypedElementDatabaseEnvironment;

struct FTypedElementExtendedQuery
{
	FMassEntityQuery NativeQuery; // Used if there's no processor bound.
	ITypedElementDataStorageInterface::FQueryDescription Description;
	TStrongObjectPtr<UMassProcessor> Processor;
};

/**
 * Storage and utilities for Typed Element queries after they've been processed by the Data Storage implementation.
 */
class FTypedElementExtendedQueryStore
{
private:
	using QueryStore = TTypedElementHandleStore<FTypedElementExtendedQuery>;
public:
	using Handle = QueryStore::Handle;
	using ListAliveEntriesConstCallback = QueryStore::ListAliveEntriesConstCallback;

	/**
	 * @section Registration
	 * @description A set of functions to manage the registration of queries.
	 */

	/** Adds a new query to the store and initializes the query with the provided arguments. */
	Handle RegisterQuery(
		ITypedElementDataStorageInterface::FQueryDescription Query, 
		FTypedElementDatabaseEnvironment& Environment,
		FMassEntityManager& EntityManager,
		FMassProcessingPhaseManager& PhaseManager);
	/** Removes the query at the given handle if still alive and otherwise does nothing. */
	void UnregisterQuery(Handle Query, FMassProcessingPhaseManager& PhaseManager);

	/** Removes all data in the query store. */
	void Clear(FMassProcessingPhaseManager& PhaseManager);
	
	/** Register the defaults for a tick group. These will be applied on top of any settings provided with a query registration. */
	void RegisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase,
		FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread);
	/** Removes a previously registered set of tick group defaults. */
	void UnregisterTickGroup(FName GroupName, ITypedElementDataStorageInterface::EQueryTickPhase Phase);

	/**
	 * @section Retrieval
	 * @description Functions to retrieve data or information on queries.
	 */

	/** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
	FTypedElementExtendedQuery* Get(Handle Entry);
	/** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
	FTypedElementExtendedQuery* GetMutable(Handle Entry);
	/** Retrieves the query at the provided handle, if still alive or otherwise returns a nullptr. */
	const FTypedElementExtendedQuery* Get(Handle Entry) const;

	/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
	FTypedElementExtendedQuery& GetChecked(Handle Entry);
	/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
	FTypedElementExtendedQuery& GetMutableChecked(Handle Entry);
	/** Retrieves the query at the provided handle, if still alive. It's up to the caller to guarantee the query is still alive. */
	const FTypedElementExtendedQuery& GetChecked(Handle Entry) const;

	/** Gets the original description used to create an extended query or an empty default if the provided query isn't alive. */
	const ITypedElementDataStorageInterface::FQueryDescription& GetQueryDescription(Handle Query) const;

	/** Checks to see if a query is still available or has been removed. */
	bool IsAlive(Handle Entry) const;

	/** Calls the provided callback for each query that's available. */
	void ListAliveEntries(const ListAliveEntriesConstCallback& Callback) const;

	/**
	 * @section Execution
	 * @description Various functions to run queries.
	 */
	TypedElementDataStorage::FQueryResult RunQuery(FMassEntityManager& EntityManager, Handle Query);
	TypedElementDataStorage::FQueryResult RunQuery(
		FMassEntityManager& EntityManager, 
		FTypedElementDatabaseEnvironment& Environment,
		Handle Query,
		TypedElementDataStorage::DirectQueryCallbackRef Callback);
	TypedElementDataStorage::FQueryResult RunQuery(
		FMassEntityManager& EntityManager,
		FTypedElementDatabaseEnvironment& Environment,
		FMassExecutionContext& ParentContext,
		Handle Query,
		TypedElementDataStorage::SubqueryCallbackRef Callback);
	TypedElementDataStorage::FQueryResult RunQuery(
		FMassEntityManager& EntityManager, 
		FTypedElementDatabaseEnvironment& Environment,
		FMassExecutionContext& ParentContext,
		Handle Query,
		TypedElementRowHandle Row, 
		TypedElementDataStorage::SubqueryCallbackRef Callback);
	void RunPhasePreambleQueries(
		FMassEntityManager& EntityManager, 
		FTypedElementDatabaseEnvironment& Environment,
		ITypedElementDataStorageInterface::EQueryTickPhase Phase, 
		float DeltaTime);
	void RunPhasePostambleQueries(
		FMassEntityManager& EntityManager, 
		FTypedElementDatabaseEnvironment& Environment,
		ITypedElementDataStorageInterface::EQueryTickPhase Phase, 
		float DeltaTime);

	void DebugPrintQueryCallbacks(FOutputDevice& Output) const;

private:
	using QueryTickPhaseType = std::underlying_type_t<ITypedElementDataStorageInterface::EQueryTickPhase>;
	static constexpr QueryTickPhaseType MaxTickPhase = static_cast<QueryTickPhaseType>(ITypedElementDataStorageInterface::EQueryTickPhase::Max);

	struct FTickGroupId
	{
		FName Name;
		ITypedElementDataStorageInterface::EQueryTickPhase Phase;

		friend inline uint32 GetTypeHash(const FTickGroupId& Id) { return HashCombine(GetTypeHash(Id.Name), GetTypeHash(Id.Phase)); }
		friend inline bool operator==(const FTickGroupId& Lhs, const FTickGroupId& Rhs) { return Lhs.Phase == Rhs.Phase && Lhs.Name == Rhs.Name; }
		friend inline bool operator!=(const FTickGroupId& Lhs, const FTickGroupId& Rhs) { return Lhs.Phase != Rhs.Phase || Lhs.Name != Rhs.Name; }
	};

	struct FTickGroupDescription
	{
		TArray<FName> BeforeGroups;
		TArray<FName> AfterGroups;
		bool bRequiresMainThread{ false };
	};

	template<typename CallbackReference>
	TypedElementDataStorage::FQueryResult RunQueryCallbackCommon(
		FMassEntityManager& EntityManager, 
		FTypedElementDatabaseEnvironment& Environment,
		FMassExecutionContext* ParentContext,
		Handle Query, 
		CallbackReference Callback);

	FMassEntityQuery& SetupNativeQuery(ITypedElementDataStorageInterface::FQueryDescription& Query, FTypedElementExtendedQuery& StoredQuery);
	bool SetupSelectedColumns(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
	bool SetupConditions(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
	bool SetupDependencies(ITypedElementDataStorageInterface::FQueryDescription& Query, FMassEntityQuery& NativeQuery);
	bool SetupTickGroupDefaults(ITypedElementDataStorageInterface::FQueryDescription& Query);
	bool SetupProcessors(Handle QueryHandle, FTypedElementExtendedQuery& StoredQuery, FTypedElementDatabaseEnvironment& Environment,
		FMassEntityManager& EntityManager, FMassProcessingPhaseManager& PhaseManager);

	EMassFragmentAccess ConvertToNativeAccessType(ITypedElementDataStorageInterface::EQueryAccessType AccessType);

	void RegisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
	void RegisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
	void UnregisterPreambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
	void UnregisterPostambleQuery(ITypedElementDataStorageInterface::EQueryTickPhase Phase, Handle Query);
	void RunPhasePreOrPostAmbleQueries(FMassEntityManager& EntityManager, FTypedElementDatabaseEnvironment& Environment,
		ITypedElementDataStorageInterface::EQueryTickPhase Phase, float DeltaTime, TArray<Handle>& QueryHandles);

	void UnregisterQueryData(Handle Query, FTypedElementExtendedQuery& QueryData, FMassProcessingPhaseManager& PhaseManager);

	static const ITypedElementDataStorageInterface::FQueryDescription EmptyDescription;

	QueryStore Queries;
	TMap<FTickGroupId, FTickGroupDescription> TickGroupDescriptions;
	TArray<Handle> PhasePreparationQueries[MaxTickPhase];
	TArray<Handle> PhaseFinalizationQueries[MaxTickPhase];
};
