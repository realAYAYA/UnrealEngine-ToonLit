// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementObjectReinstancingManager.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "Memento/TypedElementMementoInterface.h"

DECLARE_LOG_CATEGORY_CLASS(LogTedsObjectReinstancing, Log, Log)

UTypedElementObjectReinstancingManager::UTypedElementObjectReinstancingManager()
	: MementoRowBaseTable(TypedElementInvalidTableHandle)
{
}

void UTypedElementObjectReinstancingManager::Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility, UTypedElementMementoSystem& InMementoSystem)
{
	Database = &InDatabase;
	DataStorageCompatibility = &InDataStorageCompatibility;
	MementoSystem = &InMementoSystem;

	ReinstancingCallbackHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced);
	ObjectRemovedCallbackHandle = DataStorageCompatibility->RegisterObjectRemovedCallback([this](const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row)
	{
		HandleOnObjectPreRemoved(Object, TypeInfo, Row);
	});
	
	RegisterQueries();
}

void UTypedElementObjectReinstancingManager::Deinitialize()
{
	UnregisterQueries();
	
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ReinstancingCallbackHandle);
	DataStorageCompatibility->UnregisterObjectRemovedCallback(ObjectRemovedCallbackHandle);

	MementoSystem = nullptr;
	DataStorageCompatibility = nullptr;
	Database = nullptr;
}

void UTypedElementObjectReinstancingManager::RegisterQueries()
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	{
		TypedElementQueryHandle QueryHandle = Database->RegisterQuery(
		Select(
		TEXT("Memento cleanup"),
		FProcessor(DSI::EQueryTickPhase::FrameEnd, Database->GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
			[this](TypedElementDataStorage::IQueryContext& Context, const TypedElementRowHandle* Row, const FTypedElementsReinstanceableSourceObject*)
			{
				Context.RemoveRows(Context.GetRowHandles());
				
				TConstArrayView<const FTypedElementsReinstanceableSourceObject> ReinstanceableArray = MakeArrayView(Context.GetColumn<FTypedElementsReinstanceableSourceObject>(), Context.GetRowCount());
				for (const FTypedElementsReinstanceableSourceObject& Reinstanceable : ReinstanceableArray)
				{
					OldObjectToMementoMap.Remove(Reinstanceable.Object);
				}
				
			})
			.Where().All<FTypedElementMementoTag, FTypedElementMementoPopulated>()
			.Compile()
		);
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}
}

void UTypedElementObjectReinstancingManager::UnregisterQueries()
{
	// TODO: Observer queries cannot be unregistered in Mass at this time
}

void UTypedElementObjectReinstancingManager::HandleOnObjectPreRemoved(const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle ObjectRow)
{
	// This is the chance to record the old object to memento
	TypedElementRowHandle Memento = MementoSystem->CreateMemento(Database.Get());
	
	if (!ensureMsgf(Database->HasColumns(Memento, TConstArrayView<const UScriptStruct*>({FTypedElementMementoTag::StaticStruct()})),
		TEXT("Memento System should create rows with the MementoTag")))
	{
		Database->RemoveRow(Memento);
		return;
	}
	
	Database->AddOrGetColumn(ObjectRow, FTypedElementMementoOnDelete{ .Memento = Memento });
	
	Database->AddOrGetColumn(Memento, FTypedElementsReinstanceableSourceObject{.Object = Object});

	OldObjectToMementoMap.Add(Object, Memento);
}

void UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced(
	const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap)
{
	for (FCoreUObjectDelegates::FReplacementObjectMap::TConstIterator Iter = ObjectReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		const void* PreDeleteObject = Iter->Key;
		if (const TypedElementRowHandle* MementoRowPtr = OldObjectToMementoMap.Find(PreDeleteObject))
		{
			TypedElementRowHandle Memento = *MementoRowPtr;
			
			UObject* NewInstanceObject = Iter->Value;
			if (NewInstanceObject == nullptr)
			{
				// Reinstancing resulted in no target object.  Delete the memento.
				Database->AddOrGetColumn<FTypedElementMementoReinstanceAborted>(Memento);
				continue;
			}
			
			TypedElementRowHandle NewObjectRow = DataStorageCompatibility->FindRowWithCompatibleObjectExplicit(NewInstanceObject);
			// Do the addition only if there's a recorded memento. Having a memento implies the object was previously registered and there's
			// still an interest in it. Any other objects can therefore be ignored.
			if (!Database->IsRowAvailable(NewObjectRow))
			{
				NewObjectRow = DataStorageCompatibility->AddCompatibleObjectExplicit(NewInstanceObject);
			}

			// Kick off re-instantiation of NewObjectRow from the Memento
			Database->AddOrGetColumn(Memento, FTypedElementMementoReinstanceTarget{ .Target = NewObjectRow });
		}
	}
}
