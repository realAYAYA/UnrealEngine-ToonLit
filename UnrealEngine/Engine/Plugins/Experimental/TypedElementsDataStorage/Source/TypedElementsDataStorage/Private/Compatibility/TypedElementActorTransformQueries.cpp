// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorTransformQueries.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "MassActorSubsystem.h"

void UTypedElementActorTransformFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterActorAddTransformColumn(DataStorage);
	RegisterActorLocalTransformToColumn(DataStorage);
	RegisterLocalTransformColumnToActor(DataStorage);
}

void UTypedElementActorTransformFactory::RegisterActorAddTransformColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Add transform column to actor"),
			FProcessor(DSI::EQueryTickPhase::PrePhysics, 
				DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FMassActorFragment& Actor)
			{
				const AActor* ActorInstance = Actor.Get();
				if (ActorInstance != nullptr && ActorInstance->GetRootComponent())
				{
					Context.AddColumn(Row, FTypedElementLocalTransformColumn{ .Transform = ActorInstance->GetActorTransform() });
				}
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag>()
			.None<FTypedElementLocalTransformColumn>()
		.Compile()
	);
}

void UTypedElementActorTransformFactory::RegisterActorLocalTransformToColumn(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync actor transform to column"),
			FProcessor(DSI::EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FMassActorFragment& Actor, FTypedElementLocalTransformColumn& Transform)
			{
				const AActor* ActorInstance = Actor.Get();
				if (ActorInstance != nullptr && ActorInstance->GetRootComponent() != nullptr)
				{
					Transform.Transform = ActorInstance->GetActorTransform();
				}
				else
				{
					Context.RemoveColumns<FTypedElementLocalTransformColumn>(Row);
				}
			}
		)
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncFromWorldInteractiveTag>()
		.Compile()
	);
}

void UTypedElementActorTransformFactory::RegisterLocalTransformColumnToActor(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync transform column to actor"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncDataStorageToExternal))
				.ForceToGameThread(true),
			[](FMassActorFragment& Actor, const FTypedElementLocalTransformColumn& Transform)
			{
				if (AActor* ActorInstance = Actor.GetMutable(); ActorInstance != nullptr)
				{
					ActorInstance->SetActorTransform(Transform.Transform);
				}
			}
		)
		.Where()
			.All< FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}
