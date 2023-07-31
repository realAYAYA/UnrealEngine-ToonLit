// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorPartition/PartitionActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PartitionActor)

#if WITH_EDITOR
#include "Components/BoxComponent.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#endif

#define LOCTEXT_NAMESPACE "PartitionActor"

APartitionActor::APartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, GridSize(1)
#endif
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;
}

#if WITH_EDITOR
TUniquePtr<FWorldPartitionActorDesc> APartitionActor::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FPartitionActorDesc());
}

bool APartitionActor::IsUserManaged() const
{
	if (!Super::IsUserManaged())
	{
		return false;
	}

	check(GetLevel());
	if (GetLevel()->bIsPartitioned)
	{
		return false;
	}

	return true;
}

bool APartitionActor::ShouldIncludeGridSizeInName(UWorld* InWorld, const FActorPartitionIdentifier& InIdentifier) const
{
	return InWorld->GetWorldSettings()->bIncludeGridSizeInNameForPartitionedActors;
}
#endif

#undef LOCTEXT_NAMESPACE
