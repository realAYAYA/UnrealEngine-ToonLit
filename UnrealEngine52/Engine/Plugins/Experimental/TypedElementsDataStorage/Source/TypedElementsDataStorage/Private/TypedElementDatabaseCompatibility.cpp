// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseCompatibility.h"

#include "Editor.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "MassActorEditorSubsystem.h"
#include "MassActorSubsystem.h"

void UTypedElementDatabaseCompatibility::Initialize(ITypedElementDataStorageInterface* StorageInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));

	Storage = StorageInterface;
	Prepare();

	StorageInterface->OnUpdate().AddUObject(this, &UTypedElementDatabaseCompatibility::AddPendingActors);
}

void UTypedElementDatabaseCompatibility::Deinitialize()
{
	Reset();
}

void UTypedElementDatabaseCompatibility::AddCompatibleObject(AActor* Actor)
{
	// Registration is delayed for two reasons:
	//	1. Allows entity creation in a single batch rather than multiple individual additions.
	//	2. Provides an opportunity to filter out the actors that are created within MASS itself as those will already be registered.
	ActorsPendingRegistration.Add(Actor);
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObject(AActor* Actor)
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
	StandardActorTable = Storage->RegisterTable(MakeArrayView(
		{
			FMassActorFragment::StaticStruct(),
			FTypedElementLabelColumn::StaticStruct(),
			FTypedElementLabelHashColumn::StaticStruct(),
			FTypedElementPackagePathColumn::StaticStruct(),
			FTypedElementPackageLoadedPathColumn::StaticStruct()
		}), FName("Editor_StandardActorTable"));

	StandardActorWithTransformTable = Storage->RegisterTable(StandardActorTable,
		MakeArrayView(
		{
			FTypedElementLabelColumn::StaticStruct(),
			FTypedElementLocalTransformColumn::StaticStruct()
		}), FName("Editor_StandardActorWithTransformTable"));
}

void UTypedElementDatabaseCompatibility::AddPendingActors()
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// Delay processing until the required systems are available by not clearing the pending actor list.
	if (ActorsPendingRegistration.Num() > 0 && Storage && Storage->IsAvailable() && EditorWorld)
	{
		// Filter out the actors that are already registered or already destroyed. 
		// The most common case for this is actors created from within MASS.
		TWeakObjectPtr<AActor>* ActorBegin = ActorsPendingRegistration.GetData();
		TWeakObjectPtr<AActor>* ActorIt = ActorsPendingRegistration.GetData();
		TWeakObjectPtr<AActor>* ActorEnd = ActorBegin + ActorsPendingRegistration.Num();
		while (ActorIt != ActorEnd)
		{
			if (
				(*ActorIt).IsValid() && // If not valid or stale then the actor has already been deleted before it could be registered.
				!ActorSubsystem->GetEntityHandleFromActor((*ActorIt).Get()).IsValid() && // If true, this actor is already registered with the Data Storage.
				(*ActorIt)->GetWorld() == EditorWorld // Only record actors that are added to the editor to avoid taking PIE into account.
				) 
			{
				++ActorIt;
			}
			else
			{
				// Don't shrink the registration array as the array will be reused with a variety of different actor counts.
				// If memory size becomes an issue it's better to resize the array once after this loop rather than within the
				// loop to avoid many resizes happening.
				constexpr bool bAllowToShrink = false;
				ActorsPendingRegistration.RemoveAtSwap(ActorIt - ActorBegin, 1, bAllowToShrink);
				--ActorEnd;
			}
		}

		if (ActorBegin != ActorEnd) // Check if the number of actors to add isn't zero.
		{
			// Add the remaining actors to the data storage.
			ActorIt = ActorsPendingRegistration.GetData();
			Storage->BatchAddRow(StandardActorTable, ActorsPendingRegistration.Num(), [this, &ActorIt, ActorEnd](TypedElementRowHandle Row)
				{
					auto ActorStore = Storage->GetColumn<FMassActorFragment>(Row);
					checkf(ActorStore, TEXT("Newly created row didn't contain the expected FMassActorFragment."));
					
					constexpr bool bIsOwnedByMass = false;
					ActorStore->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), ActorIt->Get(), bIsOwnedByMass);
					ActorSubsystem->SetHandleForActor(ActorIt->Get(), FMassEntityHandle::FromNumber(Row));

					checkf(ActorIt < ActorEnd,
						TEXT("More (%i) entities were added than were requested (%i)."), ActorEnd - ActorIt, ActorsPendingRegistration.Num());
					++ActorIt;
				});

			// Reset the container for next set of actors.
			ActorsPendingRegistration.Reset();
		}
	}
}