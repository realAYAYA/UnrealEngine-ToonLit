// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectCollection.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectCollection)

//----------------------------------------------------------------------//
// ADEPRECATED_SmartObjectCollection 
//----------------------------------------------------------------------//
ADEPRECATED_SmartObjectCollection::ADEPRECATED_SmartObjectCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif

	PrimaryActorTick.bCanEverTick = false;
	bNetLoadOnClient = false;
	SetCanBeDamaged(false);
}

void ADEPRECATED_SmartObjectCollection::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bBuildCollectionAutomatically = !bBuildOnDemand_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (UWorld* World = GetWorld())
	{
		if (World->IsEditorWorld())
		{
			USmartObjectSubsystem::CreatePersistentCollectionFromDeprecatedData(*World, *this);
			ClearCollection();
			SetActorLabel(TEXT("DEPRECATED_SmartObjectCollection"));
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ADEPRECATED_SmartObjectCollection::ClearCollection()
{
	CollectionEntries.Reset();
	RegisteredIdToObjectMap.Empty();
	Definitions.Reset();
}
