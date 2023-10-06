// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorPartition/PartitionActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PartitionActor)

#if WITH_EDITOR
#include "Engine/Level.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "WorldPartition/DataLayer/DataLayerEditorContext.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#endif

#define LOCTEXT_NAMESPACE "PartitionActor"

DEFINE_LOG_CATEGORY_STATIC(LogPartitionActor, Log, All);

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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
uint32 APartitionActor::GetGridSize() const
{
	return GridSize;
} 

void APartitionActor::SetGridSize(uint32 InGridSize)
{
	if (InGridSize == 0)
	{
		UE_LOG(LogPartitionActor, Error, TEXT("APartitionActor::SetGridSize() called for actor %s with grid size == 0. Grid size must be greater than zero."), *GetName());
		InGridSize = 1;
	}
	GridSize = InGridSize;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FString APartitionActor::GetActorName(UWorld* World, const UClass* Class, const FGuid& GridGuid, const FActorPartitionIdentifier& ActorPartitionId, uint32 GridSize, int32 CellCoordsX, int32 CellCoordsY, int32 CellCoordsZ, uint32 DataLayerEditorContext)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ActorNameBuilder;

	ActorNameBuilder += Class->GetName();
	ActorNameBuilder += TEXT("_");

	if (GridGuid.IsValid())
	{
		ActorNameBuilder += GridGuid.ToString(EGuidFormats::Base36Encoded);
		ActorNameBuilder += TEXT("_");
	}

	if (Class->GetDefaultObject<APartitionActor>()->ShouldIncludeGridSizeInName(World, ActorPartitionId))
	{
		ActorNameBuilder += FString::Printf(TEXT("%d_"), GridSize);
	}

	ActorNameBuilder += FString::Printf(TEXT("%d_%d_%d"), CellCoordsX, CellCoordsY, CellCoordsZ);

	if (DataLayerEditorContext != FDataLayerEditorContext::EmptyHash)
	{
		ActorNameBuilder += FString::Printf(TEXT("_%X"), DataLayerEditorContext);
	}

	return ActorNameBuilder.ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
