// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorLabelQueries.h"

#include "Editor/EditorEngine.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Hash/CityHash.h"
#include "MassActorSubsystem.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "TypedElementDataStorage"

void UTypedElementActorLabelFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterActorLabelToColumnQuery(DataStorage);
	RegisterLabelColumnToActorQuery(DataStorage);
}

void UTypedElementActorLabelFactory::RegisterActorLabelToColumnQuery(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync actor label to column"),
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[](const FMassActorFragment& Actor, FTypedElementLabelColumn& Label, FTypedElementLabelHashColumn& LabelHash)
			{
				if (const AActor* ActorInstance = Actor.Get(); ActorInstance != nullptr)
				{
					const FString& ActorLabel = ActorInstance->GetActorLabel(false);
					uint64 ActorLabelHash = CityHash64(reinterpret_cast<const char*>(*ActorLabel), ActorLabel.Len() * sizeof(**ActorLabel));
					if (LabelHash.LabelHash != ActorLabelHash)
					{
						Label.Label = ActorLabel;
						LabelHash.LabelHash = ActorLabelHash;
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncFromWorldTag>()
		.Compile()
	);
}

void UTypedElementActorLabelFactory::RegisterLabelColumnToActorQuery(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync label column to actor"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncDataStorageToExternal))
				.ForceToGameThread(true),
			[](FMassActorFragment& Actor, const FTypedElementLabelColumn& Label, const FTypedElementLabelHashColumn& LabelHash)
			{
				if (AActor* ActorInstance = Actor.GetMutable(); ActorInstance != nullptr)
				{
					const FString& ActorLabel = ActorInstance->GetActorLabel(false);
					uint64 ActorLabelHash = CityHash64(reinterpret_cast<const char*>(*ActorLabel), ActorLabel.Len() * sizeof(**ActorLabel));
					if (LabelHash.LabelHash != ActorLabelHash)
					{
						const FScopedTransaction Transaction(LOCTEXT("RenameActorTransaction", "Rename Actor"));
						FActorLabelUtilities::RenameExistingActor(ActorInstance, Label.Label);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}

#undef LOCTEXT_NAMESPACE
