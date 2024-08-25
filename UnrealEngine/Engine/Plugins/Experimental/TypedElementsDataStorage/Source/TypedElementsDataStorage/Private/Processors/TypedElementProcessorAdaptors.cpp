// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/TypedElementProcessorAdaptors.h"

#include <utility>
#include "Elements/Common/TypedElementQueryTypes.h"
#include "MassCommonTypes.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "Queries/TypedElementExtendedQueryStore.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseEnvironment.h"

template<typename T>
struct FMassContextCommon : public T
{
	~FMassContextCommon() override = default;

	uint32 GetRowCount() const override
	{
		return Context.GetNumEntities();
	}

	TConstArrayView<TypedElementRowHandle> GetRowHandles() const override
	{
		static_assert(
			sizeof(TypedElementRowHandle) == sizeof(FMassEntityHandle) && alignof(TypedElementRowHandle) == alignof(FMassEntityHandle),
			"TypedElementRowHandle and FMassEntityHandle need to by layout compatible to support Typed Elements Data Storage.");
		TConstArrayView<FMassEntityHandle> Entities = Context.GetEntities();
		return TConstArrayView<TypedElementRowHandle>(reinterpret_cast<const TypedElementRowHandle*>(Entities.GetData()), Entities.Num());
	}
	
	const void* GetColumn(const UScriptStruct* ColumnType) const override
	{
		return Context.GetFragmentView(ColumnType).GetData();
	}

	void* GetMutableColumn(const UScriptStruct* ColumnType) override
	{
		return Context.GetMutableFragmentView(ColumnType).GetData();
	}

	void GetColumns(TArrayView<char*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes,
		TConstArrayView<TypedElementDataStorage::EQueryAccessType> AccessTypes) override
	{
		checkf(RetrievedAddresses.Num() == ColumnTypes.Num(), TEXT("Unable to retrieve a batch of columns as the number of addresses "
			"doesn't match the number of requested column."));
		checkf(RetrievedAddresses.Num() == AccessTypes.Num(), TEXT("Unable to retrieve a batch of columns as the number of addresses "
			"doesn't match the number of access types."));

		GetColumnsUnguarded(ColumnTypes.Num(), RetrievedAddresses.GetData(), ColumnTypes.GetData(), AccessTypes.GetData());
	}
	
	void GetColumnsUnguarded(int32 TypeCount, char** RetrievedAddresses, const TWeakObjectPtr<const UScriptStruct>* ColumnTypes,
		const TypedElementDataStorage::EQueryAccessType* AccessTypes) override
	{
		for (int32 Index = 0; Index < TypeCount; ++Index)
		{
			checkf(ColumnTypes->IsValid(), TEXT("Attempting to retrieve a column that is not available."));
			*RetrievedAddresses = *AccessTypes == ITypedElementDataStorageInterface::EQueryAccessType::ReadOnly
				? const_cast<char*>(reinterpret_cast<const char*>(Context.GetFragmentView(ColumnTypes->Get()).GetData()))
				: reinterpret_cast<char*>(Context.GetMutableFragmentView(ColumnTypes->Get()).GetData());

			++RetrievedAddresses;
			++ColumnTypes;
			++AccessTypes;
		}
	}

	bool HasColumn(const UScriptStruct* ColumnType) const override
	{
		if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
		{
			return Context.DoesArchetypeHaveTag(*ColumnType);
		}
		if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
		{
			return Context.DoesArchetypeHaveFragment(*ColumnType);
		}
		const bool bIsTagOrFragment = false;
		checkf(bIsTagOrFragment, TEXT("Attempting to check for a column type that is not a column or tag."));
		return false;
	}

	bool HasColumn(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ColumnType) const override
	{
		FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
		FMassEntityManager& Manager = Context.GetEntityManagerChecked();
		FMassArchetypeHandle Archetype = Manager.GetArchetypeForEntity(Entity);
		const FMassArchetypeCompositionDescriptor& Composition = Manager.GetArchetypeComposition(Archetype);

		if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
		{
			return Composition.Tags.Contains(*ColumnType);
		}
		if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
		{
			return Composition.Fragments.Contains(*ColumnType);
		}
		const bool bIsTagOrFragment = false;
		checkf(bIsTagOrFragment, TEXT("Attempting to check for a column type that is not a column or tag."));
		return false;
	}

protected:
	explicit FMassContextCommon(FMassExecutionContext& InContext)
		: Context(InContext)
	{}

	FMassExecutionContext& Context;
};

template<typename T>
struct FMassWithEnvironmentContextCommon : public FMassContextCommon<T>
{
	using Parent = FMassContextCommon<T>;
protected:
	void TedsColumnsToMassDescriptorIfActiveTable(
		FMassArchetypeCompositionDescriptor& Descriptor,
		TConstArrayView<const UScriptStruct*> ColumnTypes)
	{
		for (const UScriptStruct* ColumnType : ColumnTypes)
		{
			if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
			{
				if (this->Context.DoesArchetypeHaveTag(*ColumnType))
				{
					Descriptor.Tags.Add(*ColumnType);
				}
			}
			else
			{
				checkf(ColumnType->IsChildOf(FMassFragment::StaticStruct()),
					TEXT("Given struct type is not a valid fragment or tag type."));
				if (this->Context.DoesArchetypeHaveFragment(*ColumnType))
				{
					Descriptor.Fragments.Add(*ColumnType);
				}
			}
		}
	}

	void TedsColumnsToMassDescriptor(
		FMassArchetypeCompositionDescriptor& Descriptor,
		TConstArrayView<const UScriptStruct*> ColumnTypes)
	{
		for (const UScriptStruct* ColumnType : ColumnTypes)
		{
			if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
			{
				Descriptor.Tags.Add(*ColumnType);
			}
			else
			{
				checkf(ColumnType->IsChildOf(FMassFragment::StaticStruct()),
					TEXT("Given struct type is not a valid fragment or tag type."));
				Descriptor.Fragments.Add(*ColumnType);

			}
		}
	}

public:
	FMassWithEnvironmentContextCommon(FMassExecutionContext& InContext, FTypedElementDatabaseEnvironment& InEnvironment)
		: FMassContextCommon<T>(InContext)
		, Environment(InEnvironment)
	{}

	~FMassWithEnvironmentContextCommon() override = default;

	uint64 GetUpdateCycleId() const override
	{
		return Environment.GetUpdateCycleId();
	}

	void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType) override
	{
		return AddColumnUninitialized(Row, ObjectType,
			[](const UScriptStruct& TypeInfo, void* Destination, void* Source)
			{
				TypeInfo.CopyScriptStruct(Destination, Source);
			});
	}

	void* AddColumnUninitialized(TypedElementDataStorage::RowHandle Row, const UScriptStruct* ObjectType, typename Parent::ObjectCopyOrMove Relocator) override
	{
		checkf(ObjectType->IsChildOf(FMassFragment::StaticStruct()), TEXT("Column [%s] can not be a tag"), *ObjectType->GetName());
		
		struct FAddValueColumn
		{
			typename Parent::ObjectCopyOrMove Relocator;
			const UScriptStruct* FragmentType;
			FMassEntityHandle Entity;
			void* Object;

			FAddValueColumn() = default;
			FAddValueColumn(typename Parent::ObjectCopyOrMove InRelocator, const UScriptStruct* InFragmentType, FMassEntityHandle InEntity, void* InObject)
				: Relocator(InRelocator)
				, FragmentType(InFragmentType)
				, Entity(InEntity)
				, Object(InObject)
			{}

			~FAddValueColumn()
			{
				if ((this->FragmentType->StructFlags & (STRUCT_IsPlainOldData | STRUCT_NoDestructor)) == 0)
				{
					this->FragmentType->DestroyStruct(this->Object);
				}
			}
		};

		FTypedElementDatabaseScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
		void* ColumnData = ScratchBuffer.Allocate(ObjectType->GetStructureSize(), ObjectType->GetMinAlignment());
		FAddValueColumn* AddedColumn =
			ScratchBuffer.Emplace<FAddValueColumn>(Relocator, ObjectType, FMassEntityHandle::FromNumber(Row), ColumnData);
		
		this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
			[AddedColumn](FMassEntityManager& System)
			{
				// Check entity before proceeding. It's possible it may have been invalidated before this deferred call fired.
				if (System.IsEntityActive(AddedColumn->Entity))
				{
					// Check before adding.  Mass's AddFragmentToEntity is not idempotent and will assert if adding
					// column to a row that already has one
					FStructView Fragment = System.GetFragmentDataStruct(AddedColumn->Entity, AddedColumn->FragmentType);
					if (!Fragment.IsValid())
					{
						System.AddFragmentToEntity(AddedColumn->Entity, AddedColumn->FragmentType, 
							[AddedColumn](void* Fragment, const UScriptStruct& FragmentType)
							{
								AddedColumn->Relocator(FragmentType, Fragment, AddedColumn->Object);
							});
					}
					else
					{
						AddedColumn->Relocator(*AddedColumn->FragmentType, Fragment.GetMemory(), AddedColumn->Object);
					}
				}
			});
		
		return ColumnData;
	}

	void AddColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override
	{
		struct FAddedColumns
		{
			FMassArchetypeCompositionDescriptor AddDescriptor;
			FMassEntityHandle Entity;
		};

		FAddedColumns* AddedColumns = Environment.GetScratchBuffer().template Emplace<FAddedColumns>();
		TedsColumnsToMassDescriptor(AddedColumns->AddDescriptor, ColumnTypes);
		AddedColumns->Entity = FMassEntityHandle::FromNumber(Row);

		this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
			[AddedColumns](FMassEntityManager& System)
			{
				if (System.IsEntityValid(AddedColumns->Entity))
				{
					System.AddCompositionToEntity_GetDelta(AddedColumns->Entity, AddedColumns->AddDescriptor);
				}
			});
	}

	void AddColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override
	{
		struct FAddedColumns
		{
			FMassArchetypeCompositionDescriptor AddDescriptor;
			FMassEntityHandle* Entities;
			int32 EntityCount;
		};

		FTypedElementDatabaseScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
		FAddedColumns* AddedColumns = ScratchBuffer.Emplace<FAddedColumns>();
		TedsColumnsToMassDescriptor(AddedColumns->AddDescriptor, ColumnTypes);
		
		FMassEntityHandle* Entities = ScratchBuffer.EmplaceArray<FMassEntityHandle>(Rows.Num());
		AddedColumns->Entities = Entities;
		for (TypedElementRowHandle Row : Rows)
		{
			*Entities = FMassEntityHandle::FromNumber(Row);
			Entities++;
		}
		AddedColumns->EntityCount = Rows.Num();

		this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
			[AddedColumns](FMassEntityManager& System)
			{
				FMassEntityHandle* Entities = AddedColumns->Entities;
				int32 Count = AddedColumns->EntityCount;
				for (int32 Counter = 0; Counter < Count; ++Counter)
				{
					if (System.IsEntityValid(*Entities))
					{
						System.AddCompositionToEntity_GetDelta(*Entities++, AddedColumns->AddDescriptor);
					}
				}
			});
	}

	void RemoveColumns(TypedElementDataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) override
	{
		struct FRemovedColumns
		{
			FMassArchetypeCompositionDescriptor RemoveDescriptor;
			FMassEntityHandle Entity;
		};

		FRemovedColumns* RemovedColumns = Environment.GetScratchBuffer().template Emplace<FRemovedColumns>();
		TedsColumnsToMassDescriptorIfActiveTable(RemovedColumns->RemoveDescriptor, ColumnTypes);
		if (!RemovedColumns->RemoveDescriptor.IsEmpty())
		{
			RemovedColumns->Entity = FMassEntityHandle::FromNumber(Row);

			this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
				[RemovedColumns](FMassEntityManager& System)
				{
					if (System.IsEntityValid(RemovedColumns->Entity))
					{
						System.RemoveCompositionFromEntity(RemovedColumns->Entity, RemovedColumns->RemoveDescriptor);
					}
				});
		}
	}

	void RemoveColumns(TConstArrayView<TypedElementDataStorage::RowHandle> Rows, TConstArrayView<const UScriptStruct*> ColumnTypes) override
	{
		struct FRemovedColumns
		{
			FMassArchetypeCompositionDescriptor RemoveDescriptor;
			FMassEntityHandle* Entities;
			int32 EntityCount;
		};

		FTypedElementDatabaseScratchBuffer& ScratchBuffer = Environment.GetScratchBuffer();
		FRemovedColumns* RemovedColumns = ScratchBuffer.Emplace<FRemovedColumns>();
		TedsColumnsToMassDescriptorIfActiveTable(RemovedColumns->RemoveDescriptor, ColumnTypes);

		FMassEntityHandle* Entities = ScratchBuffer.EmplaceArray<FMassEntityHandle>(Rows.Num());
		RemovedColumns->Entities = Entities;
		for (TypedElementRowHandle Row : Rows)
		{
			*Entities = FMassEntityHandle::FromNumber(Row);
			Entities++;
		}
		RemovedColumns->EntityCount = Rows.Num();

		this->Context.Defer().template PushCommand<FMassDeferredAddCommand>(
			[RemovedColumns](FMassEntityManager& System)
			{
				FMassEntityHandle* Entities = RemovedColumns->Entities;
				int32 Count = RemovedColumns->EntityCount;
				for (int32 Counter = 0; Counter < Count; ++Counter)
				{
					if (System.IsEntityValid(*Entities))
					{
						System.RemoveCompositionFromEntity(*Entities++, RemovedColumns->RemoveDescriptor);
					}
				}
			});
	}

protected:
	FTypedElementDatabaseEnvironment& Environment;
};

struct FMassDirectContextForwarder final : public FMassContextCommon<ITypedElementDataStorageInterface::IDirectQueryContext>
{
	explicit FMassDirectContextForwarder(FMassExecutionContext& InContext)
		: FMassContextCommon(InContext)
	{}

	~FMassDirectContextForwarder() override = default;
};

struct FMassSubqueryContextForwarder final : public FMassWithEnvironmentContextCommon<ITypedElementDataStorageInterface::ISubqueryContext>
{
	FMassSubqueryContextForwarder(FMassExecutionContext& InContext, FTypedElementDatabaseEnvironment& InEnvironment)
		: FMassWithEnvironmentContextCommon(InContext, InEnvironment)
	{}

	~FMassSubqueryContextForwarder() override = default;
};

struct FMassContextForwarder final : public FMassWithEnvironmentContextCommon<ITypedElementDataStorageInterface::IQueryContext>
{
public:
	FMassContextForwarder(ITypedElementDataStorageInterface::FQueryDescription& InQueryDescription, FMassExecutionContext& InContext, 
		FTypedElementExtendedQueryStore& InQueryStore, FTypedElementDatabaseEnvironment& InEnvironment)
		: FMassWithEnvironmentContextCommon(InContext, InEnvironment)
		, QueryDescription(InQueryDescription)
		, QueryStore(InQueryStore)
	{}

	~FMassContextForwarder() override = default;

	UObject* GetMutableDependency(const UClass* DependencyClass) override
	{
		return Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(DependencyClass));
	}

	const UObject* GetDependency(const UClass* DependencyClass) override
	{
		return Context.GetSubsystem<USubsystem>(const_cast<UClass*>(DependencyClass));
	}

	void GetDependencies(TArrayView<UObject*> RetrievedAddresses, TConstArrayView<TWeakObjectPtr<const UClass>> SubsystemTypes,
		TConstArrayView<ITypedElementDataStorageInterface::EQueryAccessType> AccessTypes) override
	{
		checkf(RetrievedAddresses.Num() == SubsystemTypes.Num(), TEXT("Unable to retrieve a batch of subsystem as the number of addresses "
			"doesn't match the number of requested subsystem types."));

		GetDependenciesUnguarded(RetrievedAddresses.Num(), RetrievedAddresses.GetData(), SubsystemTypes.GetData(), AccessTypes.GetData());
	}

	void GetDependenciesUnguarded(int32 SubsystemCount, UObject** RetrievedAddresses, const TWeakObjectPtr<const UClass>* DependencyTypes,
		const ITypedElementDataStorageInterface::EQueryAccessType* AccessTypes)
	{
		for (int32 Index = 0; Index < SubsystemCount; ++Index)
		{
			checkf(DependencyTypes->IsValid(), TEXT("Attempting to retrieve a subsystem that's no longer valid."));
			*RetrievedAddresses = *AccessTypes == ITypedElementDataStorageInterface::EQueryAccessType::ReadOnly
				? const_cast<USubsystem*>(Context.GetSubsystem<USubsystem>(const_cast<UClass*>(DependencyTypes->Get())))
				: Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(DependencyTypes->Get()));

			++RetrievedAddresses;
			++DependencyTypes;
			++AccessTypes;
		}
	}

	bool IsRowAvailable(TypedElementDataStorage::RowHandle Row) const override
	{
		return Environment.GetMassEntityManager().IsEntityValid(FMassEntityHandle::FromNumber(Row));
	}

	bool HasRowBeenAssigned(TypedElementDataStorage::RowHandle Row) const override
	{
		return Environment.GetMassEntityManager().IsEntityActive(FMassEntityHandle::FromNumber(Row));
	}

	void RemoveRow(TypedElementRowHandle Row) override
	{
		Context.Defer().DestroyEntity(FMassEntityHandle::FromNumber(Row));
	}

	void RemoveRows(TConstArrayView<TypedElementRowHandle> Rows) override
	{
		// Row handles and entities map 1:1 for data, so a reintpret_cast can be safely done to avoid
		// having to allocate memory and iterating over the rows.

		static_assert(sizeof(FMassEntityHandle) == sizeof(TypedElementRowHandle), 
			"Sizes of mass entity and data storage row have gone out of sync.");
		static_assert(alignof(FMassEntityHandle) == alignof(TypedElementRowHandle),
			"Alignment of mass entity and data storage row have gone out of sync.");

		Context.Defer().DestroyEntities(
			TConstArrayView<FMassEntityHandle>(reinterpret_cast<const FMassEntityHandle*>(Rows.begin()), Rows.Num()));
	}

	TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const override
	{
		return Environment.GetIndexTable().FindIndexedRow(Index);
	}

	TypedElementDataStorage::FQueryResult RunQuery(TypedElementQueryHandle Query) override
	{
		const FTypedElementExtendedQueryStore::Handle Handle(Query);
		// This can be safely called because there's not callback, which means no columns are accessed, even for select queries.
		return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Handle);
	}

	TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex) override
	{
		return SubqueryIndex < QueryDescription.Subqueries.Num() ?
			RunQuery(QueryDescription.Subqueries[SubqueryIndex]) :
			TypedElementDataStorage::FQueryResult{};
	}

	TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::SubqueryCallbackRef Callback) override
	{
		if (SubqueryIndex < QueryDescription.Subqueries.Num())
		{
			const TypedElementQueryHandle SubqueryHandle = QueryDescription.Subqueries[SubqueryIndex];
			const FTypedElementExtendedQueryStore::Handle StorageHandle(SubqueryHandle);
			return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Environment, Context, StorageHandle, Callback);
		}
		else
		{
			return TypedElementDataStorage::FQueryResult{};
		}
	}

	TypedElementDataStorage::FQueryResult RunSubquery(int32 SubqueryIndex, TypedElementDataStorage::RowHandle Row,
		TypedElementDataStorage::SubqueryCallbackRef Callback) override
	{
		if (SubqueryIndex < QueryDescription.Subqueries.Num())
		{
			const TypedElementQueryHandle SubqueryHandle = QueryDescription.Subqueries[SubqueryIndex];
			const FTypedElementExtendedQueryStore::Handle StorageHandle(SubqueryHandle);
			return QueryStore.RunQuery(Context.GetEntityManagerChecked(), Environment, Context, StorageHandle, Row, Callback);
		}
		else
		{
			return TypedElementDataStorage::FQueryResult{};
		}
	}

	ITypedElementDataStorageInterface::FQueryDescription& QueryDescription;
	FTypedElementExtendedQueryStore& QueryStore;
};





/**
 * FPhasePreOrPostAmbleExecutor
 */
FPhasePreOrPostAmbleExecutor::FPhasePreOrPostAmbleExecutor(FMassEntityManager& EntityManager, float DeltaTime)
	: Context(EntityManager, DeltaTime)
{
	Context.SetDeferredCommandBuffer(MakeShared<FMassCommandBuffer>());
}

FPhasePreOrPostAmbleExecutor::~FPhasePreOrPostAmbleExecutor()
{
	Context.FlushDeferred();
}

void FPhasePreOrPostAmbleExecutor::ExecuteQuery(
	ITypedElementDataStorageInterface::FQueryDescription& Description, 
	FTypedElementExtendedQueryStore& QueryStore, 
	FTypedElementDatabaseEnvironment& Environment,
	FMassEntityQuery& NativeQuery,
	ITypedElementDataStorageInterface::QueryCallbackRef Callback)
{
	NativeQuery.ForEachEntityChunk(Context.GetEntityManagerChecked(), Context,
		[&Callback, &QueryStore, &Environment, &Description](FMassExecutionContext& ExecutionContext)
		{
			if (FTypedElementQueryProcessorData::PrepareCachedDependenciesOnQuery(Description, ExecutionContext))
			{
				FMassContextForwarder QueryContext(Description, ExecutionContext, QueryStore, Environment);
				Callback(Description, QueryContext);
			}
		}
	);
}



/**
 * FTypedElementQueryProcessorData
 */
FTypedElementQueryProcessorData::FTypedElementQueryProcessorData(UMassProcessor& Owner)
	: NativeQuery(Owner)
{
}

bool FTypedElementQueryProcessorData::CommonQueryConfiguration(
	UMassProcessor& InOwner,
	FTypedElementExtendedQuery& InQuery,
	FTypedElementExtendedQueryStore::Handle InQueryHandle,
	FTypedElementExtendedQueryStore& InQueryStore,
	FTypedElementDatabaseEnvironment& InEnvironment,
	TArrayView<FMassEntityQuery> Subqueries)
{
	ParentQuery = InQueryHandle;
	QueryStore = &InQueryStore;
	Environment = &InEnvironment;

	if (ensureMsgf(InQuery.Description.Subqueries.Num() <= Subqueries.Num(),
		TEXT("Provided query has too many (%i) subqueries."), InQuery.Description.Subqueries.Num()))
	{
		bool Result = true;
		int32 CurrentSubqueryIndex = 0;
		for (TypedElementQueryHandle SubqueryHandle : InQuery.Description.Subqueries)
		{
			const FTypedElementExtendedQueryStore::Handle SubqueryStoreHandle(SubqueryHandle);
			if (const FTypedElementExtendedQuery* Subquery = InQueryStore.Get(SubqueryStoreHandle))
			{
				if (ensureMsgf(Subquery->NativeQuery.CheckValidity(), TEXT("Provided subquery isn't valid. This can be because it couldn't be "
					"constructed properly or because it's been bound to a callback.")))
				{
					Subqueries[CurrentSubqueryIndex] = Subquery->NativeQuery;
					Subqueries[CurrentSubqueryIndex].RegisterWithProcessor(InOwner);
					++CurrentSubqueryIndex;
				}
				else
				{
					Result = false;
				}
			}
			else
			{
				Result = false;
			}
		}
		return Result;
	}
	return false;
}

EMassProcessingPhase FTypedElementQueryProcessorData::MapToMassProcessingPhase(ITypedElementDataStorageInterface::EQueryTickPhase Phase)
{
	switch(Phase)
	{
	case ITypedElementDataStorageInterface::EQueryTickPhase::PrePhysics:
		return EMassProcessingPhase::PrePhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::DuringPhysics:
		return EMassProcessingPhase::DuringPhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::PostPhysics:
		return EMassProcessingPhase::PostPhysics;
	case ITypedElementDataStorageInterface::EQueryTickPhase::FrameEnd:
		return EMassProcessingPhase::FrameEnd;
	default:
		checkf(false, TEXT("Query tick phase '%i' is unsupported."), static_cast<int>(Phase));
		return EMassProcessingPhase::MAX;
	};
}

FString FTypedElementQueryProcessorData::GetProcessorName() const
{
	const FTypedElementExtendedQuery* StoredQuery = QueryStore ? QueryStore->Get(ParentQuery) : nullptr;
	return StoredQuery ? StoredQuery->Description.Callback.Name.ToString() : FString(TEXT("<unnamed>"));
}

bool FTypedElementQueryProcessorData::PrepareCachedDependenciesOnQuery(
	ITypedElementDataStorageInterface::FQueryDescription& Description, FMassExecutionContext& Context)
{
	const int32 DependencyCount = Description.DependencyTypes.Num();
	const TWeakObjectPtr<const UClass>* Types = Description.DependencyTypes.GetData();
	const ITypedElementDataStorageInterface::EQueryDependencyFlags* Flags = Description.DependencyFlags.GetData();
	TWeakObjectPtr<UObject>* Caches = Description.CachedDependencies.GetData();

	for (int32 Index = 0; Index < DependencyCount; ++Index)
	{
		checkf(Types->IsValid(), TEXT("Attempting to retrieve a dependency type that's no longer available."));
		
		if (EnumHasAnyFlags(*Flags, ITypedElementDataStorageInterface::EQueryDependencyFlags::AlwaysRefresh) || !Caches->IsValid())
		{
			*Caches = EnumHasAnyFlags(*Flags, ITypedElementDataStorageInterface::EQueryDependencyFlags::ReadOnly)
				? const_cast<USubsystem*>(Context.GetSubsystem<USubsystem>(const_cast<UClass*>(Types->Get())))
				: Context.GetMutableSubsystem<USubsystem>(const_cast<UClass*>(Types->Get()));
			if (*Caches != nullptr)
			{
				++Types;
				++Flags;
				++Caches;
			}
			else
			{
				checkf(false, TEXT("Unable to retrieve instance of dependency '%s'."), *((*Types)->GetName()));
				return false;
			}
		}
	}
	return true;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::DirectQueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	FMassEntityQuery& NativeQuery, 
	FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment)
{
	FMassExecutionContext Context(EntityManager);
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;
	
	NativeQuery.ForEachEntityChunk(EntityManager, Context,
		[&Result, &Callback, &Description](FMassExecutionContext& Context)
		{
			// No need to cache any subsystem dependencies as these are not accessible from a direct query.
			FMassDirectContextForwarder QueryContext(Context);
			Callback(Description, QueryContext);
			Result.Count += Context.GetNumEntities();
		}
	);
	return Result;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::SubqueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	FMassEntityQuery& NativeQuery,
	FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment,
	FMassExecutionContext& ParentContext)
{
	FMassExecutionContext Context(EntityManager);
	Context.SetDeferredCommandBuffer(ParentContext.GetSharedDeferredCommandBuffer());
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;

	NativeQuery.ForEachEntityChunk(EntityManager, Context,
		[&Result, &Callback, &Description, &Environment](FMassExecutionContext& Context)
		{
			// No need to cache any subsystem dependencies as these are not accessible from a subquery.
			FMassSubqueryContextForwarder QueryContext(Context, Environment);
			Callback(Description, QueryContext);
			Result.Count += Context.GetNumEntities();
		}
	);
	return Result;
}

TypedElementDataStorage::FQueryResult FTypedElementQueryProcessorData::Execute(
	TypedElementDataStorage::SubqueryCallbackRef& Callback,
	TypedElementDataStorage::FQueryDescription& Description,
	TypedElementDataStorage::RowHandle RowHandle,
	FMassEntityQuery& NativeQuery,
	FMassEntityManager& EntityManager,
	FTypedElementDatabaseEnvironment& Environment,
	FMassExecutionContext& ParentContext)
{
	ITypedElementDataStorageInterface::FQueryResult Result;
	Result.Completed = ITypedElementDataStorageInterface::FQueryResult::ECompletion::Fully;

	FMassEntityHandle NativeEntity = FMassEntityHandle::FromNumber(RowHandle);
	if (EntityManager.IsEntityActive(NativeEntity))
	{
		FMassArchetypeHandle NativeArchetype = EntityManager.GetArchetypeForEntityUnsafe(NativeEntity);
		FMassExecutionContext Context(EntityManager);
		Context.SetEntityCollection(FMassArchetypeEntityCollection(NativeArchetype, { NativeEntity }, FMassArchetypeEntityCollection::NoDuplicates));
		Context.SetDeferredCommandBuffer(ParentContext.GetSharedDeferredCommandBuffer());

		NativeQuery.ForEachEntityChunk(EntityManager, Context,
			[&Result, &Callback, &Description, &Environment, &EntityManager, RowHandle](FMassExecutionContext& Context)
			{
				// No need to cache any subsystem dependencies as these are not accessible from a subquery.
				FMassSubqueryContextForwarder QueryContext(Context, Environment);
				Callback(Description, QueryContext);
				Result.Count += Context.GetNumEntities();
			}
		);
		checkf(Result.Count < 2, TEXT("Single row subquery produced multiple results."));
	}
	return Result;
}

void FTypedElementQueryProcessorData::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FTypedElementExtendedQuery* StoredQuery = QueryStore->GetMutable(ParentQuery);
	checkf(StoredQuery, TEXT("A query callback was registered for execution without an associated query."));
	
	ITypedElementDataStorageInterface::FQueryDescription& Description = StoredQuery->Description;
	NativeQuery.ForEachEntityChunk(EntityManager, Context,
		[this, &Description](FMassExecutionContext& Context)
		{
			if (PrepareCachedDependenciesOnQuery(Description, Context))
			{
				FMassContextForwarder QueryContext(Description, Context, *QueryStore, *Environment);
				Description.Callback.Function(Description, QueryContext);
			}
		}
	);
}



/**
 * UTypedElementQueryProcessorCallbackAdapterProcessor
 */

UTypedElementQueryProcessorCallbackAdapterProcessorBase::UTypedElementQueryProcessorCallbackAdapterProcessorBase()
	: Data(*this)
{
	bAllowMultipleInstances = true;
	bAutoRegisterWithProcessingPhases = false;
}

FMassEntityQuery& UTypedElementQueryProcessorCallbackAdapterProcessorBase::GetQuery()
{
	return Data.NativeQuery;
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query,
	FTypedElementExtendedQueryStore::Handle QueryHandle,
	FTypedElementExtendedQueryStore& QueryStore,
	FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, {});
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ShouldAllowQueryBasedPruning(
	const bool bRuntimeMode) const
{
	// TEDS is much more dynamic with when tables and processors are added and removed
	// Don't prune processors if they have queries where no table is defined, it is possible
	// the table will be dynamically created later.
	return false;
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueryCallbackData(
	FTypedElementExtendedQuery& Query,
	FTypedElementExtendedQueryStore::Handle QueryHandle,
	FTypedElementExtendedQueryStore& QueryStore,
	FTypedElementDatabaseEnvironment& Environment,
	TArrayView<FMassEntityQuery> Subqueries)
{
	bool Result = Data.CommonQueryConfiguration(*this, Query, QueryHandle, QueryStore, Environment, Subqueries);

	bRequiresGameThreadExecution = Query.Description.Callback.bForceToGameThread;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor); 
	ExecutionOrder.ExecuteInGroup = Query.Description.Callback.Group;
	ExecutionOrder.ExecuteBefore = Query.Description.Callback.BeforeGroups;
	ExecutionOrder.ExecuteAfter = Query.Description.Callback.AfterGroups;
	ProcessingPhase = Data.MapToMassProcessingPhase(Query.Description.Callback.Phase);

	Super::PostInitProperties();
	return Result;
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::ConfigureQueries()
{
	// When the extended query information is provided the native query will already be fully configured.
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::PostInitProperties()
{
	Super::Super::PostInitProperties();
}

FString UTypedElementQueryProcessorCallbackAdapterProcessorBase::GetProcessorName() const
{
	FString Name = Data.GetProcessorName();
	Name += TEXT(" [Editor Processor]");
	return Name;
}

void UTypedElementQueryProcessorCallbackAdapterProcessorBase::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Data.Execute(EntityManager, Context);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith1Subquery::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith2Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith3Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryProcessorCallbackAdapterProcessorWith4Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}


/**
 * UTypedElementQueryObserverCallbackAdapterProcessor
 */

UTypedElementQueryObserverCallbackAdapterProcessorBase::UTypedElementQueryObserverCallbackAdapterProcessorBase()
	: Data(*this)
{
	bAllowMultipleInstances = true;
	bAutoRegisterWithProcessingPhases = false;
}

FMassEntityQuery& UTypedElementQueryObserverCallbackAdapterProcessorBase::GetQuery()
{
	return Data.NativeQuery;
}

const UScriptStruct* UTypedElementQueryObserverCallbackAdapterProcessorBase::GetObservedType() const
{
	return ObservedType;
}

EMassObservedOperation UTypedElementQueryObserverCallbackAdapterProcessorBase::GetObservedOperation() const
{
	return Operation;
}

bool UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, {});
}

bool UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueryCallbackData(
	FTypedElementExtendedQuery& Query,
	FTypedElementExtendedQueryStore::Handle QueryHandle,
	FTypedElementExtendedQueryStore& QueryStore,
	FTypedElementDatabaseEnvironment& Environment, TArrayView<FMassEntityQuery> Subqueries)
{
	bool Result = Data.CommonQueryConfiguration(*this, Query, QueryHandle, QueryStore, Environment, Subqueries);

	bRequiresGameThreadExecution = Query.Description.Callback.bForceToGameThread;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	
	ObservedType = const_cast<UScriptStruct*>(Query.Description.Callback.MonitoredType);
	
	switch (Query.Description.Callback.Type)
	{
	case ITypedElementDataStorageInterface::EQueryCallbackType::ObserveAdd:
		Operation = EMassObservedOperation::Add;
		break;
	case ITypedElementDataStorageInterface::EQueryCallbackType::ObserveRemove:
		Operation = EMassObservedOperation::Remove;
		break;
	default:
		checkf(false, TEXT("Query type %i is not supported from the observer processor adapter."),
			static_cast<int>(Query.Description.Callback.Type));
		return false;
	}

	Super::PostInitProperties();
	return Result;
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::ConfigureQueries()
{
	// When the extended query information is provided the native query will already be fully configured.
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::PostInitProperties()
{
	Super::Super::PostInitProperties();
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::Register()
{ 
	// Do nothing as this processor will be explicitly registered.
}

FString UTypedElementQueryObserverCallbackAdapterProcessorBase::GetProcessorName() const
{
	FString Name = Data.GetProcessorName();
	EMassObservedOperation ObservationType = GetObservedOperation();
	if (ObservationType == EMassObservedOperation::Add)
	{
		Name += TEXT(" [Editor Add Observer]");
	}
	else if (ObservationType == EMassObservedOperation::Remove)
	{
		Name += TEXT(" [Editor Remove Observer]");
	}
	else
	{
		Name += TEXT(" [Editor <Unknown> Observer]");
	}
	return Name;
}

void UTypedElementQueryObserverCallbackAdapterProcessorBase::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Data.Execute(EntityManager, Context);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith1Subquery::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith2Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith3Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}

bool UTypedElementQueryObserverCallbackAdapterProcessorWith4Subqueries::ConfigureQueryCallback(
	FTypedElementExtendedQuery& Query, FTypedElementExtendedQueryStore::Handle QueryHandle, FTypedElementExtendedQueryStore& QueryStore, FTypedElementDatabaseEnvironment& Environment)
{
	return ConfigureQueryCallbackData(Query, QueryHandle, QueryStore, Environment, NativeSubqueries);
}
