// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseCompatibility.h"

#include "Algo/Unique.h"
#include "Editor.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "MassActorEditorSubsystem.h"
#include "MassActorSubsystem.h"
#include "TypedElementDataStorageProfilingMacros.h"

void UTypedElementDatabaseCompatibility::Initialize(ITypedElementDataStorageInterface* StorageInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));
	
	Storage = StorageInterface;
	Prepare();

	StorageInterface->OnUpdate().AddUObject(this, &UTypedElementDatabaseCompatibility::Tick);

	PostEditChangePropertyDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPostEditChangeProperty);
}

void UTypedElementDatabaseCompatibility::Deinitialize()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PostEditChangePropertyDelegateHandle);
	
	Reset();
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(UObject* Object)
{
	return AddCompatibleObjectExplicit(Object, StandardUObjectTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(UObject* Object, TypedElementTableHandle Table)
{
	if (Object->IsA<AActor>())
	{
		return AddCompatibleObjectExplicit(static_cast<AActor*>(Object));
	}
	else
	{
		if (ensureMsgf(Storage, TEXT("Trying to add a UObject to Typed Element's Data Storage before the storage is available.")))
		{
			TypedElementRowHandle ReservedRow = Storage->ReserveRow();
			ReverseObjectLookup.Add(Object, ReservedRow);
			
			PendingRegistration<TWeakObjectPtr<UObject>>& Pending = UObjectsPendingRegistration.FindOrAdd(Table);
			Pending.Add(ReservedRow, Object);
			
			return ReservedRow;
		}
		else
		{
			return TypedElementInvalidRowHandle;
		}
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(AActor* Actor)
{
	return AddCompatibleObjectExplicit(Actor, StandardActorTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(AActor* Actor, TypedElementTableHandle Table)
{
	if (ensureMsgf(Storage, TEXT("Trying to add an actor to Typed Element's Data Storage before the storage is available.")))
	{
		// Registration is delayed for two reasons:
		//	1. Allows entity creation in a single batch rather than multiple individual additions.
		//	2. Provides an opportunity to filter out the actors that are created within MASS itself as those will already be registered.
		
		TypedElementRowHandle ReservedRow = Storage->ReserveRow();
		PendingRegistration<TWeakObjectPtr<AActor>>& Pending = ActorsPendingRegistration.FindOrAdd(Table);
		Pending.Add(ReservedRow, Actor);
		return ReservedRow;
	}
	return TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo)
{
	return AddCompatibleObjectExplicit(Object, MoveTemp(TypeInfo), StandardExternalObjectTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(
	void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo, TypedElementTableHandle Table)
{
	if (ensureMsgf(Storage, TEXT("Trying to add an object to Typed Element's Data Storage before the storage is available.")))
	{
		TypedElementRowHandle ReservedRow = Storage->ReserveRow();
		ReverseObjectLookup.Add(Object, ReservedRow);
		PendingRegistration<ExternalObjectRegistration>& Pending = ExternalObjectsPendingRegistration.FindOrAdd(Table);
		Pending.Add(ReservedRow, ExternalObjectRegistration{ .Object = Object, .TypeInfo = TypeInfo });
		return ReservedRow;
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(UObject* Object)
{
	if (Object->IsA<AActor>())
	{
		RemoveCompatibleObjectExplicit(static_cast<AActor*>(Object));
	}
	else
	{
		checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
		if (TypedElementRowHandle* Row = ReverseObjectLookup.Find(Object))
		{
			Storage->RemoveRow(*Row);
		}
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(void* Object)
{
	checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
	if (TypedElementRowHandle* Row = ReverseObjectLookup.Find(Object))
	{
		Storage->RemoveRow(*Row);
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(AActor* Actor)
{
	checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));

	// If there is no actor subsystem it means that the world has been destroyed, including the MASS instance,
	// so there's no references to clean up.
	if (Storage && ActorSubsystem && Storage->IsAvailable())
	{
		FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
		// If there's no entity it may:
		//	- have been deleted earlier, e.g. through an explicit delete.
		//	- be an actor that never had a world assigned and was therefore never registered.
		//	- have registered with a MASS instance in another world, e.g. one created for PIE.
		if (Entity.IsValid())
		{
			auto ActorStore = Storage->GetColumn<FMassActorFragment>(Entity.AsNumber());
			if (ActorStore && !ActorStore->IsOwnedByMass()) // Only remove actors that were externally created.
			{
				ActorSubsystem->RemoveHandleForActor(Actor);
				Storage->RemoveRow(Entity.AsNumber());
			}
		}
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const UObject* Object) const
{
	if (Object && Storage && Storage->IsAvailable())
	{
		const AActor* Actor = Cast<AActor>(Object);
		if (Actor && ActorSubsystem)
		{
			FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
			return Entity.IsValid() ? Entity.AsNumber() : TypedElementInvalidRowHandle;
		}
		else
		{
			const TypedElementRowHandle* Row = ReverseObjectLookup.Find(Object);
			return Row ? *Row : TypedElementInvalidRowHandle;
		}
	}
	return TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const AActor* Actor) const
{
	if (Storage && ActorSubsystem && Storage->IsAvailable())
	{
		FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
		return Entity.IsValid() ? Entity.AsNumber() : TypedElementInvalidRowHandle;
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const void* Object) const
{
	if (Object && Storage && Storage->IsAvailable())
	{
		const TypedElementRowHandle* Row = ReverseObjectLookup.Find(Object);
		return Row ? *Row : TypedElementInvalidRowHandle;
	}
	return TypedElementInvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::Prepare()
{
	UMassActorEditorSubsystem* MassActorEditorSubsystem = Storage->GetExternalSystem<UMassActorEditorSubsystem>();
	check(MassActorEditorSubsystem);
	ActorSubsystem = MassActorEditorSubsystem->GetMutableActorManager().AsShared();

	CreateStandardArchetypes();
}

void UTypedElementDatabaseCompatibility::Reset()
{
	ActorSubsystem = nullptr;
}

void UTypedElementDatabaseCompatibility::CreateStandardArchetypes()
{
	StandardActorTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FMassActorFragment, FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementLabelColumn, FTypedElementLabelHashColumn,
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
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
}

void UTypedElementDatabaseCompatibility::Tick()
{
	TEDS_EVENT_SCOPE(TEXT("Compatibility Tick"))
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// Delay processing until the required systems are available by not clearing any lists or doing any work.
	if (Storage && Storage->IsAvailable() && EditorWorld)
	{
		TickPendingActorRegistration(EditorWorld);
		TickPendingUObjectRegistration();
		TickPendingExternalObjectRegistration();
		TickActorSync();
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Add(TypedElementRowHandle ReservedRowHandle, AddressType Address)
{
	Addresses.Add(Forward<AddressType>(Address));
	ReservedRowHandles.Add(ReservedRowHandle);
}

template<typename AddressType>
bool UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::IsEmpty() const
{
	// ReservedRowHandles can also be returned as they'll both have the same length.
	return Addresses.IsEmpty();
}

template<typename AddressType>
int32 UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Num() const
{
	// ReservedRowHandles can also be returned as they'll both have the same length.
	return Addresses.Num();
}

template<typename AddressType>
TArrayView<AddressType> UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::GetAddresses()
{
	return Addresses;
}

template<typename AddressType>
TArrayView<TypedElementRowHandle> UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::GetReservedRowHandles()
{
	return ReservedRowHandles;
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::RemoveInvalidEntries(
	ITypedElementDataStorageInterface& StorageInterface, const TFunctionRef<bool(const AddressType&)>& Validator)
{
	checkf(Addresses.Num() == ReservedRowHandles.Num(),
		TEXT("The reserved row handle count (%i) didn't match the stored pointer count (%i)."),
		ReservedRowHandles.Num(), Addresses.Num());

	AddressType* AddressBegin = Addresses.GetData();
	AddressType* AddressIt = Addresses.GetData();
	AddressType* AddressEnd = AddressBegin + Addresses.Num();
	TypedElementRowHandle* RowHandleBegin = ReservedRowHandles.GetData();
	TypedElementRowHandle* RowHandleIt = ReservedRowHandles.GetData();
	while (AddressIt != AddressEnd)
	{
		if (Validator(*AddressIt))
		{
			++AddressIt;
			++RowHandleIt;
		}
		else
		{
			// Don't shrink the registration array as the array will be reused with a variety of different actor counts.
			// If memory size becomes an issue it's better to resize the array once after this loop rather than within the
			// loop to avoid many resizes happening.
			constexpr bool bAllowToShrink = false;
			Addresses.RemoveAtSwap(AddressIt - AddressBegin, 1, bAllowToShrink);
			StorageInterface.RemoveRow(*RowHandleIt);
			ReservedRowHandles.RemoveAtSwap(RowHandleIt - RowHandleBegin, 1, bAllowToShrink);
			--AddressEnd;
		}
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::ProcessEntries(ITypedElementDataStorageInterface& StorageInterface,
	TypedElementTableHandle Table, const TFunctionRef<void(TypedElementRowHandle, const AddressType&)>& SetupRowCallback)
{
	if (!IsEmpty())
	{
		AddressType* TargetIt = GetAddresses().GetData();
		AddressType* TargetEnd = TargetIt + Num();
		StorageInterface.BatchAddRow(Table, GetReservedRowHandles(), 
			[this, &SetupRowCallback, &TargetIt, TargetEnd](TypedElementRowHandle Row)
			{
				SetupRowCallback(Row, *TargetIt);
				checkf(TargetIt < TargetEnd, TEXT("More (%i) entities were added than were requested (%i)."), TargetEnd - TargetIt, Num());
				++TargetIt;
			});
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Reset()
{
	Addresses.Reset();
	ReservedRowHandles.Reset();
}

void UTypedElementDatabaseCompatibility::TickPendingActorRegistration(UWorld* EditorWorld)
{
	if (!ActorsPendingRegistration.IsEmpty())
	{
		// Filter out the actors that are already registered or already destroyed. 
		// The most common case for this is actors created from within MASS.
		for (auto It = ActorsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[this, EditorWorld](const TWeakObjectPtr<AActor>& Actor)
				{
					AActor* Instance = Actor.Get();
					return Instance && Instance->GetWorld() == EditorWorld && !ActorSubsystem->GetEntityHandleFromActor(Instance).IsValid();
				});
		}

		// Add the remaining actors to the data storage.
		for (auto It = ActorsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key,
				[this](TypedElementRowHandle Row, const TWeakObjectPtr<AActor>& ActorPtr)
				{
					FMassActorFragment* ActorStore = Storage->AddOrGetColumn<FMassActorFragment>(Row);
					checkf(ActorStore, TEXT("Failed to retrieve or add FMassActorFragment to newly created row."));

					constexpr bool bIsOwnedByMass = false;

					AActor* Actor = ActorPtr.Get();
					check(Actor);
					ActorStore->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), Actor, bIsOwnedByMass);
					ActorSubsystem->SetHandleForActor(Actor, FMassEntityHandle::FromNumber(Row));

					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = ActorPtr });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Actor->GetClass() });
					
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
				});
		}
			
		ActorsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickPendingUObjectRegistration()
{
	if (!UObjectsPendingRegistration.IsEmpty())
	{
		// Filter out the objects that are already registered or already destroyed. 
		for (auto It = UObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[this](const TWeakObjectPtr<UObject>& Object)
				{
					UObject* Instance = Object.Get();
					return Instance && FindRowWithCompatibleObjectExplicit(Instance) != TypedElementInvalidRowHandle;
				});
		}

		// Add the remaining object to the data storage.
		for (auto It = UObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key, 
				[this](TypedElementRowHandle Row, const TWeakObjectPtr<UObject>& Object)
				{
					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = Object });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Object->GetClass() });
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
				});
		}

		UObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickPendingExternalObjectRegistration()
{
	if (!ExternalObjectsPendingRegistration.IsEmpty())
	{
		// Filter out the objects that are already registered or already destroyed. 
		for (auto It = ExternalObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[this](const ExternalObjectRegistration& Object)
				{
					return Object.Object && FindRowWithCompatibleObjectExplicit(Object.Object) != TypedElementInvalidRowHandle;
				});
		}

		// Add the remaining object to the data storage.
		for (auto It = ExternalObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key, 
				[this](TypedElementRowHandle Row, const ExternalObjectRegistration& Object)
				{
					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementExternalObjectColumn{ .Object = Object.Object });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementScriptStructTypeInfoColumn{ .TypeInfo = Object.TypeInfo });
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
				});
		}

		ExternalObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickActorSync()
{
	if (!ActorsNeedingFullSync.IsEmpty())
	{
		TEDS_EVENT_SCOPE(TEXT("Process ActorsNeedingFullSync"));
		// Deduplicate to avoid duplicate reverse lookups or adding tags more than once
		{
			TEDS_EVENT_SCOPE(TEXT("Deduplicate ActorsNeedingFullSync"));
			ActorsNeedingFullSync.Sort();
			ActorsNeedingFullSync.SetNum(Algo::Unique(ActorsNeedingFullSync), /* bAllowShrinking */ false);
		}

		TArray<TypedElementRowHandle> RowHandles;
		{
			TEDS_EVENT_SCOPE(TEXT("Reverse lookup Rows from Actors"));

			RowHandles.SetNumUninitialized(ActorsNeedingFullSync.Num());
			{
				int32 RowHandleIndex = 0;
				for (int32 ActorIndex = 0, End = ActorsNeedingFullSync.Num(); ActorIndex < End; ++ActorIndex)
				{
					const TObjectKey<const AActor> ActorKey = ActorsNeedingFullSync[ActorIndex];
					const TypedElementRowHandle Row = FindRowWithCompatibleObject(ActorKey);
					if (Row != TypedElementInvalidRowHandle)
					{
						RowHandles[RowHandleIndex++] = Row;
					}
				}
				const int32 RowHandleCount = RowHandleIndex;
				const bool bAllowShrinking = false;
				RowHandles.SetNum(RowHandleCount, bAllowShrinking);
			}

			ActorsNeedingFullSync.Reset();
		}

		{
			TEDS_EVENT_SCOPE(TEXT("Add SyncFromWorld Tag"));
			// Tag the rows containing actor data that they need to be synced
			// Note: Watch out for the performance of this, may end up doing a lot of row moves
			for (TypedElementRowHandle Row : RowHandles)
			{
				Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
			}
		}
	}
}

void UTypedElementDatabaseCompatibility::OnPostEditChangeProperty(
	UObject* Object,
	FPropertyChangedEvent& /*PropertyChangedEvent*/)
{
	// We aren't sure if this Actor is tracked by the database
	// Will resolve that during the tick step
	// The aim is to keep this delegate handler as simple as possible to avoid performance side-effects when other code
	// invokes it.
	AActor* Actor = Cast<AActor>(Object);
	if (Actor)
	{
		// Note: This array may end up with duplicates
		ActorsNeedingFullSync.Add(Actor);
	}
}
