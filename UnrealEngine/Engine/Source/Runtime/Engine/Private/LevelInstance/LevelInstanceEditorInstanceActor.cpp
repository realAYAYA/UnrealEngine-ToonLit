// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "GameFramework/WorldSettings.h"
#include "WorldPartition/WorldPartition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceEditorInstanceActor)

ALevelInstanceEditorInstanceActor::ALevelInstanceEditorInstanceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->Mobility = EComponentMobility::Static;
	
	// To keep the behavior of any calls to USceneComponent::GetActorPositionForRenderer() consistent between Editor and Game modes
	// we need to flag the root component so that it isn't considered as an AttachmentRoot.
	RootComponent->bIsNotRenderAttachmentRoot = true;
}

#if WITH_EDITOR
AActor* ALevelInstanceEditorInstanceActor::GetSelectionParent() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
	{
		if (ILevelInstanceInterface* LevelInstance = LevelInstanceSubsystem->GetLevelInstance(LevelInstanceID))
		{
			return CastChecked<AActor>(LevelInstance);
		}
	}

	return nullptr;
}

ALevelInstanceEditorInstanceActor* ALevelInstanceEditorInstanceActor::Create(ILevelInstanceInterface* LevelInstance, ULevel* LoadedLevel)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = LoadedLevel;
	SpawnParams.bHideFromSceneOutliner = true;
	SpawnParams.bCreateActorPackage = false;
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.bNoFail = true;

	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
	ALevelInstanceEditorInstanceActor* InstanceActor = LevelInstanceActor->GetWorld()->SpawnActor<ALevelInstanceEditorInstanceActor>(LevelInstanceActor->GetActorLocation(), LevelInstanceActor->GetActorRotation(), SpawnParams);
	InstanceActor->SetActorScale3D(LevelInstanceActor->GetActorScale3D());
	InstanceActor->SetLevelInstanceID(LevelInstance->GetLevelInstanceID());
	
	for (AActor* LevelActor : LoadedLevel->Actors)
	{
		if (LevelActor && LevelActor->GetAttachParentActor() == nullptr && !LevelActor->IsChildActor() && LevelActor != InstanceActor)
		{
			LevelActor->AttachToActor(InstanceActor, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	InstanceActor->PushSelectionToProxies();

	return InstanceActor;
}

void ALevelInstanceEditorInstanceActor::UpdateWorldTransform(const FTransform& WorldTransform)
{
	GetRootComponent()->SetWorldTransform(WorldTransform);

	const ULevel* Level = GetLevel();

	ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(Level);
	check(LevelStreaming);

	AWorldSettings* WorldSettings = Level->GetWorldSettings();
	check(WorldSettings);
	LevelStreaming->LevelTransform = FTransform(WorldSettings->LevelInstancePivotOffset) * WorldTransform;

	if (UWorldPartition* WorldPartition = WorldSettings->GetWorldPartition())
	{
		WorldPartition->SetInstanceTransform(LevelStreaming->LevelTransform);
	}
}

#endif
