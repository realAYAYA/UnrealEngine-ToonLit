// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorViewportProcessors.h"

#include "Components/PrimitiveComponent.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "MassActorSubsystem.h"

void UTypedElementActorViewportFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterSelectionOutlineColorColumnToActor(DataStorage);
}

void UTypedElementActorViewportFactory::RegisterSelectionOutlineColorColumnToActor(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync selection outline color column to actor"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncDataStorageToExternal))
				.ForceToGameThread(true),
			[](FMassActorFragment& Actor, const FTypedElementViewportColorColumn& ViewportColor)
			{
				if (AActor* ActorInstance = Actor.GetMutable(); ActorInstance != nullptr)
				{
					if (UPrimitiveComponent* PrimitiveComponent = ActorInstance->GetComponentByClass<UPrimitiveComponent>())
					{
						PrimitiveComponent->SetSelectionOutlineColorIndex(ViewportColor.SelectionOutlineColorIndex);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}
