// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEDSTypedElementActorHandleFactory.h"

#include "MassActorSubsystem.h"
#include "Compatibility/TEDSTypedElementBridge.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementList.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

void UTEDSTypedElementActorHandleFactory::PreRegister(ITypedElementDataStorageInterface& DataStorage)
{
	Super::PreRegister(DataStorage);
	
	BridgeEnableDelegateHandle = UTEDSTypedElementBridge::OnEnabled().AddUObject(this, &UTEDSTypedElementActorHandleFactory::HandleBridgeEnabled);
}

void UTEDSTypedElementActorHandleFactory::PreShutdown(ITypedElementDataStorageInterface& DataStorage)
{
	UTEDSTypedElementBridge::OnEnabled().Remove(BridgeEnableDelegateHandle);
	BridgeEnableDelegateHandle.Reset();
}

void UTEDSTypedElementActorHandleFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	Super::RegisterQueries(DataStorage);

	if (UTEDSTypedElementBridge::IsEnabled())
	{
		RegisterQuery_ActorHandlePopulate(DataStorage);
	}

	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;
	GetAllActorsQuery = DataStorage.RegisterQuery(
	Select()
		.ReadOnly<FMassActorFragment>()
	.Compile());
}

void UTEDSTypedElementActorHandleFactory::RegisterQuery_ActorHandlePopulate(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;
	using DSI = ITypedElementDataStorageInterface;
	if (!ensureMsgf(ActorHandlePopulateQuery == TypedElementInvalidQueryHandle, TEXT("Already registered query")))
	{
		return;
	}
	
	ActorHandlePopulateQuery = DataStorage.RegisterQuery(
	Select(TEXT("Populate actor typed element handles"),
		FObserver::OnAdd<FTypedElementUObjectColumn>(),
		[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& ObjectColumn)
		{
			if (UObject* Object = ObjectColumn.Object.Get())
			{
				checkSlow(Cast<AActor>(Object));
				FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(static_cast<AActor*>(Object));
				Context.AddColumn(Row, FTEDSTypedElementColumn
				{
					.Handle = Handle
				});
			}
		}
	)
	.Where()
		.All<FMassActorFragment>()
	.Compile());
}

void UTEDSTypedElementActorHandleFactory::HandleBridgeEnabled(bool bEnabled)
{
	ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage();
	
	if (bEnabled)
	{
		using namespace TypedElementQueryBuilder;
		using namespace TypedElementDataStorage;
		using DSI = ITypedElementDataStorageInterface;
		
		// Populate all the rows
		TArray<RowHandle> CollatedRowHandles;
		TArray<TWeakObjectPtr<const AActor>> Actors;
		
		DataStorage->RunQuery(GetAllActorsQuery, CreateDirectQueryCallbackBinding([&CollatedRowHandles, &Actors](IDirectQueryContext& Context, const FMassActorFragment* Fragments)
		{
			TConstArrayView<RowHandle> RowHandles = Context.GetRowHandles();

			CollatedRowHandles.Append(RowHandles);

			TConstArrayView<const FMassActorFragment> FragmentView(Fragments, Context.GetRowCount());

			Actors.Reserve(Actors.Num() + FragmentView.Num());
			for (const FMassActorFragment& Fragment : FragmentView)
			{
				const AActor* Actor = Fragment.Get(FMassActorFragment::EActorAccess::OnlyWhenAlive);
				Actors.Add(Actor);
			}
		}));

		for (int32 Index = 0, End = CollatedRowHandles.Num(); Index < End; ++Index)
		{
			if (const AActor* Actor = Actors[Index].Get())
			{
				FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor);
				DataStorage->AddOrGetColumn(CollatedRowHandles[Index], FTEDSTypedElementColumn
				{
					.Handle = Handle
				});
			}
		}

		RegisterQuery_ActorHandlePopulate(*DataStorage);
	}
	else
	{
		DataStorage->UnregisterQuery(ActorHandlePopulateQuery);
		ActorHandlePopulateQuery = TypedElementInvalidQueryHandle;
	}
}

