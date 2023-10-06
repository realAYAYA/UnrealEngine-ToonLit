// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorLabelQueries.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Hash/CityHash.h"
#include "MassActorSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

FAutoConsoleCommandWithOutputDevice PrintActorLabelsConsoleCommand(
	TEXT("TEDS.PrintActorLabels"),
	TEXT("Prints out the labels for all actors found in the Typed Elements Data Storage."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			using namespace TypedElementQueryBuilder;
			using DSI = ITypedElementDataStorageInterface;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.PrintActorLabelsCommand);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle LabelQuery = TypedElementInvalidQueryHandle;
				if (LabelQuery == TypedElementInvalidQueryHandle)
				{
					LabelQuery = DataStorage->RegisterQuery(
						Select()
							.ReadOnly<FTypedElementLabelColumn>()
						.Where()
							.All<FMassActorFragment>()
						.Compile());
				}
				
				if (LabelQuery != TypedElementInvalidQueryHandle)
				{
					FString Message;
					Output.Log(TEXT("The Typed Elements Data Storage has the following actors:"));
					DataStorage->RunQuery(LabelQuery, [&Output, &Message](const DSI::FQueryDescription&, DSI::IDirectQueryContext& Context)
						{
							const FTypedElementLabelColumn* Labels = Context.GetColumn<FTypedElementLabelColumn>();
							const uint32 Count = Context.GetRowCount();

							const FTypedElementLabelColumn* LabelsIt = Labels;
							int32 CharacterCount = 0;
							// Reserve memory first to avoid repeated memory allocations.
							for (uint32 Index = 0; Index < Count; ++Index)
							{
								CharacterCount += 12 /*Prefixed text size*/ + LabelsIt->Label.Len() + 1 /*Trailing new line*/;
								++LabelsIt;
							}
							Message.Reset(CharacterCount);

							LabelsIt = Labels;
							for (uint32 Index = 0; Index < Count; ++Index)
							{
								Message += TEXT("    Actor: ");
								Message += LabelsIt->Label;
								Message += TEXT('\n');
								++LabelsIt;
							}

							Output.Log(Message);
						});
					Output.Log(TEXT("End of Typed Elements Data Storage actors list."));
				}
			}
		}));

void UTypedElementActorLabelFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
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
						ActorInstance->SetActorLabel(Label.Label);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}
