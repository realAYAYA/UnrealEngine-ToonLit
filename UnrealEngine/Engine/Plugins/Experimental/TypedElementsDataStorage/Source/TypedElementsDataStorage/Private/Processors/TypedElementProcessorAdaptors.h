// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassExecutionContext.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementProcessorAdaptors.generated.h"

struct FTypedElementExtendedQuery;
class FTypedElementExtendedQueryStore;

struct FPhasePreOrPostAmbleExecutor
{
	FPhasePreOrPostAmbleExecutor(FMassEntityManager& EntityManager, float DeltaTime);
	~FPhasePreOrPostAmbleExecutor();

	void ExecuteQuery(
		ITypedElementDataStorageInterface::FQueryDescription& Description,
		FTypedElementExtendedQueryStore& QueryStore,
		FMassEntityQuery& NativeQuery,
		ITypedElementDataStorageInterface::QueryCallbackRef Callback);

	FMassExecutionContext Context;
};

USTRUCT()
struct FTypedElementQueryProcessorData
{
	GENERATED_BODY()

	FTypedElementQueryProcessorData() = default;
	explicit FTypedElementQueryProcessorData(UMassProcessor& Owner);

	bool CommonQueryConfiguration(UMassProcessor& InOwner, FTypedElementExtendedQuery& InQuery, 
		FTypedElementExtendedQueryStore& InQueryStore, TArrayView<FMassEntityQuery> Subqueries);
	static EMassProcessingPhase MapToMassProcessingPhase(ITypedElementDataStorageInterface::EQueryTickPhase Phase);
	FString GetProcessorName() const;

	static ITypedElementDataStorageInterface::FQueryResult Execute(
		ITypedElementDataStorageInterface::DirectQueryCallbackRef& Callback,
		ITypedElementDataStorageInterface::FQueryDescription& Description, 
		FMassEntityQuery& NativeQuery, 
		FMassEntityManager& EntityManager);
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	static bool PrepareCachedDependenciesOnQuery(
		ITypedElementDataStorageInterface::FQueryDescription& Description, FMassExecutionContext& Context);

	
	FTypedElementExtendedQuery* ParentQuery{ nullptr };
	FTypedElementExtendedQueryStore* QueryStore{ nullptr };
	FMassEntityQuery NativeQuery;
};

/**
 * Adapts processor queries callback for MASS.
 */
UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessorBase : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementQueryProcessorCallbackAdapterProcessorBase();

	FMassEntityQuery& GetQuery();
	virtual bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore);

protected:
	bool ConfigureQueryCallbackData(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore, 
		TArrayView<FMassEntityQuery> Subqueries);
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& TargetParentQuery) override;
	
	void PostInitProperties() override;
	FString GetProcessorName() const override;

private:
	UPROPERTY(transient)
	FTypedElementQueryProcessorData Data;
};

UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessor final : public UTypedElementQueryProcessorCallbackAdapterProcessorBase
{
	GENERATED_BODY()
};

/**
 * Mass verifies that queries that are used by processors are on the processor themselves. It does this by taking the address of the query 
 * and seeing if it's within the start and end address of the processor. When a dynamic array is used those addresses are going to be 
 * elsewhere, so the two options are to store a single fixed size array on a processor or have multiple instances. With Mass' queries being 
 * not an insignificant size it's preferable to have several variants with queries to allow the choice for the minimal size. Unfortunately 
 * UHT doesn't allow for templates so it had to be done in an explicit way.
 */

UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessorWith1Subquery final : public UTypedElementQueryProcessorCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[1];
};

UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessorWith2Subqueries final : public UTypedElementQueryProcessorCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[2];
};

UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessorWith3Subqueries final : public UTypedElementQueryProcessorCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[3];
};

UCLASS()
class UTypedElementQueryProcessorCallbackAdapterProcessorWith4Subqueries final : public UTypedElementQueryProcessorCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[4];
};

/**
 * Adapts observer queries callback for MASS.
 */
UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessorBase : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UTypedElementQueryObserverCallbackAdapterProcessorBase();

	FMassEntityQuery& GetQuery();
	const UScriptStruct* GetObservedType() const;
	EMassObservedOperation GetObservedOperation() const;
	virtual bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore);

protected:
	bool ConfigureQueryCallbackData(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore,
		TArrayView<FMassEntityQuery> Subqueries);
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& TargetParentQuery) override;

	void PostInitProperties() override;
	void Register() override;
	FString GetProcessorName() const override;

private:
	UPROPERTY(transient)
	FTypedElementQueryProcessorData Data;
};

UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessor final : public UTypedElementQueryObserverCallbackAdapterProcessorBase
{
	GENERATED_BODY()
};

/**
 * Mass verifies that queries that are used by processors are on the processor themselves. It does this by taking the address of the query
 * and seeing if it's within the start and end address of the processor. When a dynamic array is used those addresses are going to be
 * elsewhere, so the two options are to store a single fixed size array on a processor or have multiple instances. With Mass' queries being
 * not an insignificant size it's preferable to have several variants with queries to allow the choice for the minimal size. Unfortunately
 * UHT doesn't allow for templates so it had to be done in an explicit way.
 */

UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessorWith1Subquery final : public UTypedElementQueryObserverCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[1];
};

UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessorWith2Subqueries final : public UTypedElementQueryObserverCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[2];
};

UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessorWith3Subqueries final : public UTypedElementQueryObserverCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[3];
};

UCLASS()
class UTypedElementQueryObserverCallbackAdapterProcessorWith4Subqueries final : public UTypedElementQueryObserverCallbackAdapterProcessorBase
{
	GENERATED_BODY()

public:
	bool ConfigureQueryCallback(FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore& QueryStore) override;

private:
	UPROPERTY(transient)
	FMassEntityQuery NativeSubqueries[4];
};
