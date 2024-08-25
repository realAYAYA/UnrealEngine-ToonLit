// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseCompatibility.h"

#include <utility>
#include "Algo/Unique.h"
#include "Async/UniqueLock.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "MassActorSubsystem.h"
#include "TypedElementDataStorageProfilingMacros.h"

void UTypedElementDatabaseCompatibility::Initialize(ITypedElementDataStorageInterface* StorageInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));
	
	Storage = StorageInterface;
	Prepare();

	StorageInterface->OnUpdate().AddUObject(this, &UTypedElementDatabaseCompatibility::Tick);

	PreEditChangePropertyDelegateHandle = FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPrePropertyChanged);
	PostEditChangePropertyDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPostEditChangeProperty);
	ObjectModifiedDelegateHandle = FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UTypedElementDatabaseCompatibility::OnObjectModified);
	ObjectReinstancedDelegateHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementDatabaseCompatibility::OnObjectReinstanced);


	PostWorldInitializationDelegateHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPostWorldInitialization);
	PreWorldFinishDestroyDelegateHandle = FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPreWorldFinishDestroy);
}

void UTypedElementDatabaseCompatibility::Deinitialize()
{
	for (TPair<UWorld*, FDelegateHandle>& It : ActorDestroyedDelegateHandles)
	{
		It.Key->RemoveOnActorDestroyededHandler(It.Value);
	}

	FWorldDelegates::OnPreWorldFinishDestroy.Remove(PreWorldFinishDestroyDelegateHandle);
	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationDelegateHandle);
	
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ObjectReinstancedDelegateHandle);
	FCoreUObjectDelegates::OnObjectModified.Remove(ObjectModifiedDelegateHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PostEditChangePropertyDelegateHandle);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.Remove(PreEditChangePropertyDelegateHandle);
	
	Reset();
}

void UTypedElementDatabaseCompatibility::RegisterRegistrationFilter(ObjectRegistrationFilter Filter)
{
	ObjectRegistrationFilters.Add(MoveTemp(Filter));
}

void UTypedElementDatabaseCompatibility::RegisterDealiaserCallback(ObjectToRowDealiaser Dealiaser)
{
	ObjectToRowDialiasers.Add(MoveTemp(Dealiaser));
}

void UTypedElementDatabaseCompatibility::RegisterTypeTableAssociation(
	TObjectPtr<UStruct> TypeInfo, TypedElementDataStorage::TableHandle Table)
{
	TypeToTableMap.Add(TypeInfo, Table);
}

FDelegateHandle UTypedElementDatabaseCompatibility::RegisterObjectAddedCallback(ObjectAddedCallback&& OnObjectAdded)
{
	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	ObjectAddedCallbackList.Emplace(MoveTemp(OnObjectAdded), Handle);
	return Handle;
}

void UTypedElementDatabaseCompatibility::UnregisterObjectAddedCallback(FDelegateHandle Handle)
{
	ObjectAddedCallbackList.RemoveAll([Handle](const TPair<ObjectAddedCallback, FDelegateHandle>& Element)->bool
	{
		return Element.Value == Handle;
	});
}

FDelegateHandle UTypedElementDatabaseCompatibility::RegisterObjectRemovedCallback(ObjectRemovedCallback&& OnObjectAdded)
{
	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	PreObjectRemovedCallbackList.Emplace(MoveTemp(OnObjectAdded), Handle);
	return Handle;
}

void UTypedElementDatabaseCompatibility::UnregisterObjectRemovedCallback(FDelegateHandle Handle)
{
	PreObjectRemovedCallbackList.RemoveAll([Handle](const TPair<ObjectRemovedCallback, FDelegateHandle>& Element)->bool
	{
		return Element.Value == Handle;
	});
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(UObject* Object)
{
	bool bCanAddObject =
		ensureMsgf(Storage, TEXT("Trying to add a UObject to Typed Element's Data Storage before the storage is available.")) &&
		ShouldAddObject(Object);
	return bCanAddObject ? AddCompatibleObjectExplicitTransactionable<true>(Object) : TypedElementDataStorage::InvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo)
{
	using namespace TypedElementDataStorage;
	
	if (ensureMsgf(Storage, TEXT("Trying to add an object to Typed Element's Data Storage before the storage is available.")))
	{
		TypedElementRowHandle Result = FindRowWithCompatibleObjectExplicit(Object);
		if (!Storage->IsRowAvailable(Result))
		{
			Result = Storage->ReserveRow();
			Storage->IndexRow(GenerateIndexHash(Object), Result);
			ExternalObjectsPendingRegistration.Add(Result, ExternalObjectRegistration{ .Object = Object, .TypeInfo = TypeInfo });
		}
		return Result;
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(UObject* Object)
{
	RemoveCompatibleObjectExplicitTransactionable<true>(Object);
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(void* Object)
{
	using namespace TypedElementDataStorage;

	checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
	IndexHash Hash = GenerateIndexHash(Object);
	RowHandle Row = Storage->FindIndexedRow(Hash);
	if (Storage->IsRowAvailable(Row))
	{
		const FTypedElementScriptStructTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row);
		if (Storage->HasRowBeenAssigned(Row) && ensureMsgf(TypeInfoColumn, TEXT("Missing type information for removed void* object at ptr 0x%p"), Object))
		{
			OnPreObjectRemoved(Object, TypeInfoColumn->TypeInfo.Get(), Row);
		}
		Storage->RemoveRow(Row);
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const UObject* Object) const
{
	using namespace TypedElementDataStorage;

	if (Object && Storage && Storage->IsAvailable())
	{
		RowHandle Row = Storage->FindIndexedRow(GenerateIndexHash(Object));
		return Storage->IsRowAvailable(Row) ? Row : DealiasObject(Object);
	}
	return TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const void* Object) const
{
	using namespace TypedElementDataStorage;

	return (Object && Storage && Storage->IsAvailable()) ? Storage->FindIndexedRow(GenerateIndexHash(Object)) : InvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::Prepare()
{
	CreateStandardArchetypes();
	RegisterTypeInformationQueries();
}

void UTypedElementDatabaseCompatibility::Reset()
{
}

void UTypedElementDatabaseCompatibility::CreateStandardArchetypes()
{
	StandardActorTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FMassActorFragment, FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementLabelColumn, FTypedElementLabelHashColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardActorTable"));

	StandardActorWithTransformTable = Storage->RegisterTable(StandardActorTable,
		TTypedElementColumnTypeList<FTypedElementLocalTransformColumn>(),
		FName("Editor_StandardActorWithTransformTable"));

	StandardUObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardUObjectTable"));

	StandardExternalObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementExternalObjectColumn, FTypedElementScriptStructTypeInfoColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardExternalObjectTable"));

	RegisterTypeTableAssociation(AActor::StaticClass(), StandardActorTable);
	RegisterTypeTableAssociation(UObject::StaticClass(), StandardUObjectTable);
}

void UTypedElementDatabaseCompatibility::RegisterTypeInformationQueries()
{
	using namespace TypedElementQueryBuilder;

	ClassTypeInfoQuery = Storage->RegisterQuery(
		Select()
			.ReadWrite<FTypedElementClassTypeInfoColumn>()
		.Compile());
	
	ScriptStructTypeInfoQuery = Storage->RegisterQuery(
		Select()
			.ReadWrite<FTypedElementScriptStructTypeInfoColumn>()
		.Compile());
}

bool UTypedElementDatabaseCompatibility::ShouldAddObject(const UObject* Object) const
{
	using namespace TypedElementDataStorage;

	bool Include = true;
	if (!Storage->IsRowAvailable(Storage->FindIndexedRow(GenerateIndexHash(Object))))
	{
		const ObjectRegistrationFilter* Filter = ObjectRegistrationFilters.GetData();
		const ObjectRegistrationFilter* FilterEnd = Filter + ObjectRegistrationFilters.Num();
		for (; Include && Filter != FilterEnd; ++Filter)
		{
			Include = (*Filter)(*this, Object);
		}
	}
	return Include;
}

TypedElementDataStorage::TableHandle UTypedElementDatabaseCompatibility::FindBestMatchingTable(const UStruct* TypeInfo) const
{
	using namespace TypedElementDataStorage;

	while (TypeInfo)
	{
		if (const TableHandle* Table = TypeToTableMap.Find(TypeInfo))
		{
			return *Table;
		}
		TypeInfo = TypeInfo->GetSuperStruct();
	}

	return InvalidTableHandle;
}

template<bool bEnableTransactions>
TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicitTransactionable(UObject* Object)
{
	using namespace TypedElementDataStorage;

	TypedElementRowHandle Result = FindRowWithCompatibleObjectExplicit(Object);
	if (!Storage->IsRowAvailable(Result))
	{
		Result = Storage->ReserveRow();
		Storage->IndexRow(GenerateIndexHash(Object), Result);
		UObjectsPendingRegistration.Add(Result, Object);

		if constexpr (bEnableTransactions)
		{
			if (GUndo)
			{
				GUndo->StoreUndo(this, MakeUnique<FRegistrationCommandChange>(Object));
			}
		}
	}

	return Result;
}

template<bool bEnableTransactions>
void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicitTransactionable(UObject* Object)
{
	using namespace TypedElementDataStorage;

	checkf(Storage, 
		TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
	IndexHash Hash = GenerateIndexHash(Object);
	RowHandle Row = Storage->FindIndexedRow(Hash);
	if (Storage->IsRowAvailable(Row))
	{
		const FTypedElementClassTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementClassTypeInfoColumn>(Row);
		if (Storage->HasRowBeenAssigned(Row) && 
			ensureMsgf(TypeInfoColumn, TEXT("Missing type information for removed UObject at ptr 0x%p [%s]"), Object, *Object->GetName()))
		{
			OnPreObjectRemoved(Object, TypeInfoColumn->TypeInfo.Get(), Row);

			if constexpr (bEnableTransactions)
			{
				if (GUndo)
				{
					GUndo->StoreUndo(this, MakeUnique<FDeregistrationCommandChange>(Object));
				}
			}
		}

		Storage->RemoveRow(Row);
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::DealiasObject(const UObject* Object) const
{
	for (const ObjectToRowDealiaser& Dealiaser : ObjectToRowDialiasers)
	{
		if (TypedElementRowHandle Row = Dealiaser(*this, Object); Storage->IsRowAvailable(Row))
		{
			return Row;
		}
	}
	return TypedElementDataStorage::InvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::Tick()
{
	TEDS_EVENT_SCOPE(TEXT("Compatibility Tick"))
	
	PendingTypeInformationUpdate.Process(*this);

	// Delay processing until the required systems are available by not clearing any lists or doing any work.
	if (Storage && Storage->IsAvailable())
	{
		TickPendingUObjectRegistration();
		TickPendingExternalObjectRegistration();
		TickObjectSync();
	}
}



//
// FPendingTypeInformatUpdate
// 

UTypedElementDatabaseCompatibility::FPendingTypeInformationUpdate::FPendingTypeInformationUpdate()
	: PendingTypeInformationUpdatesActive(&PendingTypeInformationUpdates[0])
	, PendingTypeInformationUpdatesSwapped(&PendingTypeInformationUpdates[1])
{}

void UTypedElementDatabaseCompatibility::FPendingTypeInformationUpdate::AddTypeInformation(const TMap<UObject*, UObject*>& ReplacedObjects)
{
	UE::TUniqueLock Lock(Safeguard);

	for (TMap<UObject*, UObject*>::TConstIterator It = ReplacedObjects.CreateConstIterator(); It; ++It)
	{
		if (It->Key->IsA<UStruct>())
		{
			PendingTypeInformationUpdatesActive->Add(*It);
			bHasPendingUpdate = true;
		}
	}
}

void UTypedElementDatabaseCompatibility::FPendingTypeInformationUpdate::Process(UTypedElementDatabaseCompatibility& Compatibility)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	if (bHasPendingUpdate)
	{
		// Swap to release the lock as soon as possible.
		{
			UE::TUniqueLock Lock(Safeguard);
			std::swap(PendingTypeInformationUpdatesActive, PendingTypeInformationUpdatesSwapped);
		}

		for (TypeToTableMapType::TIterator It = Compatibility.TypeToTableMap.CreateIterator(); It; ++It)
		{
			if (TOptional<TWeakObjectPtr<UObject>> NewObject = ProcessResolveTypeRecursively(It.Key()); NewObject.IsSet())
			{
				UpdatedTypeInfoScratchBuffer.Emplace(Cast<UStruct>(*NewObject), It.Value());
				It.RemoveCurrent();
			}
		}
		for (TPair<TWeakObjectPtr<UStruct>, TypedElementDataStorage::TableHandle>& UpdatedEntry : UpdatedTypeInfoScratchBuffer)
		{
			checkf(UpdatedEntry.Key.IsValid(),
				TEXT("Type info column in data storage has been re-instanced to an object without type information"));
			Compatibility.TypeToTableMap.Add(UpdatedEntry);
		}
		UpdatedTypeInfoScratchBuffer.Reset();

		Compatibility.Storage->RunQuery(Compatibility.ClassTypeInfoQuery, CreateDirectQueryCallbackBinding(
			[this](IDirectQueryContext& Context, FTypedElementClassTypeInfoColumn& Type)
			{
				if (TOptional<TWeakObjectPtr<UObject>> NewObject = ProcessResolveTypeRecursively(Type.TypeInfo); NewObject.IsSet())
				{
					Type.TypeInfo = Cast<UClass>(*NewObject);
					checkf(Type.TypeInfo.IsValid(),
						TEXT("Type info column in data storage has been re-instanced to an object without class type information"));
				}
			}));
		Compatibility.Storage->RunQuery(Compatibility.ScriptStructTypeInfoQuery, CreateDirectQueryCallbackBinding(
			[this](IDirectQueryContext& Context, FTypedElementScriptStructTypeInfoColumn& Type)
			{
				if (TOptional<TWeakObjectPtr<UObject>> NewObject = ProcessResolveTypeRecursively(Type.TypeInfo); NewObject.IsSet())
				{
					Type.TypeInfo = Cast<UScriptStruct>(*NewObject);
					checkf(Type.TypeInfo.IsValid(),
						TEXT("Type info column in data storage has been re-instanced to an object without struct type information"));
				}
			}));

		Compatibility.ExternalObjectsPendingRegistration.ForEachAddress(
			[this](ExternalObjectRegistration& Entry)
			{
				if (TOptional<TWeakObjectPtr<UObject>> NewObject = ProcessResolveTypeRecursively(Entry.TypeInfo); NewObject.IsSet())
				{
					Entry.TypeInfo = Cast<UScriptStruct>(*NewObject);
					checkf(Entry.TypeInfo.Get(),
						TEXT("Type info pending processing in data storage has been re-instanced to an object without struct type information"));
				}
			});

		PendingTypeInformationUpdatesSwapped->Reset();
		bHasPendingUpdate = false;
	}
}

TOptional<TWeakObjectPtr<UObject>> UTypedElementDatabaseCompatibility::FPendingTypeInformationUpdate::ProcessResolveTypeRecursively(
	const TWeakObjectPtr<const UObject>& Target)
{
	if (const TWeakObjectPtr<UObject>* NewObject = PendingTypeInformationUpdatesSwapped->Find(Target))
	{
		TWeakObjectPtr<UObject> LastNewObject = *NewObject;
		while (const TWeakObjectPtr<UObject>* NextNewObject = PendingTypeInformationUpdatesSwapped->Find(LastNewObject))
		{
			LastNewObject = *NextNewObject;
		}
		return LastNewObject;
	}
	return TOptional<TWeakObjectPtr<UObject>>();
}




//
// PendingRegistration
//

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Add(TypedElementRowHandle ReservedRowHandle, AddressType Address)
{
	Entries.Emplace(FEntry{ .Address = Address, .Row = ReservedRowHandle });
}

template<typename AddressType>
bool UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::IsEmpty() const
{
	return Entries.IsEmpty();
}

template<typename AddressType>
int32 UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Num() const
{
	return Entries.Num();
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::ForEachAddress(const TFunctionRef<void(AddressType&)>& Callback)
{
	for (FEntry& Entry : Entries)
	{
		Callback(Entry.Address);
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::ProcessEntries(ITypedElementDataStorageInterface& StorageInterface,
	UTypedElementDatabaseCompatibility& Compatibility, const TFunctionRef<void(TypedElementRowHandle, const AddressType&)>& SetupRowCallback)
{
	using namespace TypedElementDataStorage;

	// Start by removing any entries that are no longer valid.
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		bool bIsValid = StorageInterface.IsRowAvailable(It->Row);
		if constexpr (std::is_same_v<AddressType, TWeakObjectPtr<UObject>>)
		{
			bIsValid = bIsValid && It->Address.IsValid();
		}
		else if constexpr (std::is_same_v<AddressType, ExternalObjectRegistration>)
		{
			bIsValid = bIsValid && (It->Address.Object != nullptr);
		}
		else
		{
			static_assert(sizeof(AddressType) == 0, "Unsupported type for pending object registration in data storage compatibility.");
		}

		if (!bIsValid)
		{
			It.RemoveCurrentSwap();
		}
	}

	// Check for empty here are the above code could potentially leave an empty array behind. This would result in break the assumption
	// that there is at least one entry later in this function.
	if (!Entries.IsEmpty())
	{
		// Next resolve the required table handles.
		for (FEntry& Entry : Entries)
		{
			if constexpr (std::is_same_v<AddressType, TWeakObjectPtr<UObject>>)
			{
				Entry.Table = Compatibility.FindBestMatchingTable(Entry.Address->GetClass());
				checkf(Entry.Table != InvalidTableHandle, 
					TEXT("The data storage could not find any matching tables for object of type '%s'. "
					"This can mean that the object doesn't derive from UObject or that a table for UObject is no longer registered."), 
					*Entry.Address->GetClass()->GetFName().ToString());

			}
			else if constexpr (std::is_same_v<AddressType, ExternalObjectRegistration>)
			{
				Entry.Table = Compatibility.FindBestMatchingTable(Entry.Address.TypeInfo.Get());
				Entry.Table = (Entry.Table != InvalidTableHandle) ? Entry.Table : Compatibility.StandardExternalObjectTable;
			}
			else
			{
				static_assert(sizeof(AddressType) == 0, "Unsupported type for pending object registration in data storage compatibility.");
			}
		}

		// Next sort them by table then by row handle to allow batch insertion.
		Entries.Sort(
			[](const FEntry& Lhs, const FEntry& Rhs)
			{
				if (Lhs.Table < Rhs.Table)
				{
					return true;
				}
				else if (Lhs.Table > Rhs.Table)
				{
					return false;
				}
				else
				{
					return Lhs.Row < Rhs.Row;
				}
			});

		// Batch up the entries and add them to the storage.
		FEntry* Current = Entries.GetData();
		FEntry* End = Current + Entries.Num();
		
		FEntry* TableFront = Current;
		TableHandle CurrentTable = Entries[0].Table;
		
		for (; Current != End; ++Current)
		{
			if (Current->Table != CurrentTable)
			{
				StorageInterface.BatchAddRow(CurrentTable, Compatibility.RowScratchBuffer,
					[&SetupRowCallback, &TableFront](TypedElementRowHandle Row)
					{
						SetupRowCallback(Row, TableFront->Address);
						++TableFront;
					});

				CurrentTable = Current->Table;
				Compatibility.RowScratchBuffer.Reset();
			}
			Compatibility.RowScratchBuffer.Add(Current->Row);
		}
		StorageInterface.BatchAddRow(CurrentTable, Compatibility.RowScratchBuffer,
			[&SetupRowCallback, &TableFront](TypedElementRowHandle Row)
			{
				SetupRowCallback(Row, TableFront->Address);
				++TableFront;
			});
		Compatibility.RowScratchBuffer.Reset();
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Reset()
{
	Entries.Reset();
}

void UTypedElementDatabaseCompatibility::TickPendingUObjectRegistration()
{
	if (!UObjectsPendingRegistration.IsEmpty())
	{
		UObjectsPendingRegistration.ProcessEntries(*Storage, *this,
			[this](TypedElementRowHandle Row, const TWeakObjectPtr<UObject>& Object)
			{
				if (AActor* Actor = Cast<AActor>(Object))
				{
					constexpr bool bIsOwnedByMass = false;
					Storage->AddOrGetColumn<FMassActorFragment>(Row)->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), Actor, bIsOwnedByMass);
				}

				Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = Object });
				Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Object->GetClass() });
				// Make sure the new row is tagged for update.
				Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
				OnObjectAdded(Object.Get(), Object->GetClass(), Row);
			});

		UObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickPendingExternalObjectRegistration()
{
	if (!ExternalObjectsPendingRegistration.IsEmpty())
	{
		ExternalObjectsPendingRegistration.ProcessEntries(*Storage, *this,
			[this](TypedElementRowHandle Row, const ExternalObjectRegistration& Object)
			{
				Storage->AddOrGetColumn<FTypedElementExternalObjectColumn>(Row, FTypedElementExternalObjectColumn{ .Object = Object.Object });
				Storage->AddOrGetColumn<FTypedElementScriptStructTypeInfoColumn>(Row, FTypedElementScriptStructTypeInfoColumn{ .TypeInfo = Object.TypeInfo });
				// Make sure the new row is tagged for update.
				Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);

				OnObjectAdded(Object.Object, Object.TypeInfo.Get(), Row);
			});

		ExternalObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickObjectSync()
{
	using namespace TypedElementDataStorage;
	
	if (!ObjectsNeedingSyncTags.IsEmpty())
	{
		TEDS_EVENT_SCOPE(TEXT("Process ObjectsNeedingSyncTags"));
		
		using ColumnArray = TArray<const UScriptStruct*, TInlineAllocator<MaxExpectedTagsForObjectSync>>;
		ColumnArray ColumnsToAdd;
		ColumnArray ColumnsToRemove;
		ColumnArray* ColumnsToAddPtr = &ColumnsToAdd;
		ColumnArray* ColumnsToRemovePtr = &ColumnsToRemove;
		bool bHasUpdates = false;
		for (TPair<ObjectsNeedingSyncTagsMapKey, ObjectsNeedingSyncTagsMapValue>& ObjectToSync : ObjectsNeedingSyncTags)
		{
			const RowHandle Row = FindRowWithCompatibleObject(ObjectToSync.Key);
			if (Storage->IsRowAvailable(Row))
			{
				for (FSyncTagInfo& Column : ObjectToSync.Value)
				{
					if (Column.ColumnType.IsValid())
					{
						ColumnArray* TargetColumn = Column.bAddColumn ? ColumnsToAddPtr : ColumnsToRemovePtr;
						TargetColumn->Add(Column.ColumnType.Get());
						bHasUpdates = true;
					}
				}
				if (bHasUpdates)
				{
					Storage->AddRemoveColumns(Row, ColumnsToAdd, ColumnsToRemove);
				}
			}
			bHasUpdates = false;
			ColumnsToAdd.Reset();
			ColumnsToRemove.Reset();
		}

		ObjectsNeedingSyncTags.Reset();
	}
}

void UTypedElementDatabaseCompatibility::OnPrePropertyChanged(UObject* Object, const FEditPropertyChain& PropertyChain)
{
	ObjectsNeedingSyncTags.FindOrAdd(Object).AddUnique(
		FSyncTagInfo{ .ColumnType = FTypedElementSyncFromWorldInteractiveTag::StaticStruct(), .bAddColumn = true });
}

void UTypedElementDatabaseCompatibility::OnPostEditChangeProperty(
	UObject* Object,
	FPropertyChangedEvent& PropertyChangedEvent)
{
	// Determining the object is being tracked in the database can't be done safely as it may be queued for addition.
	// It would also add a small bit of performance overhead as access the lookup table can be done faster as a
	// batch operation during the tick step.
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		ObjectsNeedingSyncTagsMapValue& SyncValue = ObjectsNeedingSyncTags.FindOrAdd(Object);
		SyncValue.AddUnique(FSyncTagInfo{ .ColumnType = FTypedElementSyncFromWorldTag::StaticStruct(), .bAddColumn = true });
		SyncValue.AddUnique(FSyncTagInfo{ .ColumnType = FTypedElementSyncFromWorldInteractiveTag::StaticStruct(), .bAddColumn = false });
	}
}

void UTypedElementDatabaseCompatibility::OnObjectModified(UObject* Object)
{
	// Determining the object is being tracked in the database can't be done safely as it may be queued for addition.
	// It would also add a small bit of performance overhead as access the lookup table can be done faster as a
	// batch operation during the tick step.
	ObjectsNeedingSyncTags.FindOrAdd(Object).AddUnique(
		FSyncTagInfo{ .ColumnType = FTypedElementSyncFromWorldTag::StaticStruct(), .bAddColumn = true });
}

void UTypedElementDatabaseCompatibility::OnObjectAdded(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const
{
	for (const TPair<ObjectAddedCallback, FDelegateHandle>& CallbackPair : ObjectAddedCallbackList)
	{
		const ObjectAddedCallback& Callback = CallbackPair.Key;
		Callback(Object, TypeInfo, Row);
	}
}

void UTypedElementDatabaseCompatibility::OnPreObjectRemoved(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const
{
	for (const TPair<ObjectRemovedCallback, FDelegateHandle>& CallbackPair : PreObjectRemovedCallbackList)
	{
		const ObjectRemovedCallback& Callback = CallbackPair.Key;
		Callback(Object, TypeInfo, Row);
	}
}

void UTypedElementDatabaseCompatibility::OnObjectReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ReplacedObjects)
{
	PendingTypeInformationUpdate.AddTypeInformation(ReplacedObjects);
}

void UTypedElementDatabaseCompatibility::OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues)
{
	FDelegateHandle Handle = World->AddOnActorDestroyedHandler(
		FOnActorDestroyed::FDelegate::CreateUObject(this, &UTypedElementDatabaseCompatibility::OnActorDestroyed));
	ActorDestroyedDelegateHandles.Add(World, Handle);
}

void UTypedElementDatabaseCompatibility::OnPreWorldFinishDestroy(UWorld* World)
{
	FDelegateHandle Handle;
	if (ActorDestroyedDelegateHandles.RemoveAndCopyValue(World, Handle))
	{
		World->RemoveOnActorDestroyededHandler(Handle);
	}
}

void UTypedElementDatabaseCompatibility::OnActorDestroyed(AActor* Actor)
{
	RemoveCompatibleObjectExplicit(Actor);
}

SIZE_T GetTypeHash(const UTypedElementDatabaseCompatibility::FSyncTagInfo& Column)
{
	return HashCombine(Column.ColumnType.GetWeakPtrTypeHash(), Column.bAddColumn);
}

//
// UTypedElementDatabaseCompatibility::FRegistrationCommandChange
//

UTypedElementDatabaseCompatibility::FRegistrationCommandChange::FRegistrationCommandChange(UObject* InTargetObject)
	: TargetObject(InTargetObject)
{
}

void UTypedElementDatabaseCompatibility::FRegistrationCommandChange::Apply(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

void UTypedElementDatabaseCompatibility::FRegistrationCommandChange::Revert(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->RemoveCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

FString UTypedElementDatabaseCompatibility::FRegistrationCommandChange::ToString() const
{
	return TEXT("Typed Element Data Storage Compatibility - Registration");
}


//
// UTypedElementDatabaseCompatibility::FDeregistrationCommandChange
//

UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::FDeregistrationCommandChange(UObject* InTargetObject)
	: TargetObject(InTargetObject)
{
}

void UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::Apply(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->RemoveCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

void UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::Revert(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

FString UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::ToString() const
{
	return TEXT("Typed Element Data Storage Compatibility - Deregistration");
}
