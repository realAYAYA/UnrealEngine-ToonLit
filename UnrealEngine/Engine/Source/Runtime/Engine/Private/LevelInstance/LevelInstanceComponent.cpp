// Copyright Epic Games, Inc. All Rights Reserved.
#include "LevelInstance/LevelInstanceComponent.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceComponent)

ULevelInstanceComponent::ULevelInstanceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	bWantsOnUpdateTransform = true;
#endif
}

#if WITH_EDITOR
void ULevelInstanceComponent::OnRegister()
{
#if WITH_EDITORONLY_DATA
	// Prevents USceneComponent from creating the SpriteComponent in OnRegister because we want to provide a different texture and condition
	bVisualizeComponent = false;
#endif

	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	AActor* Owner = GetOwner();
	// Only show Sprite for non-instanced LevelInstances
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		// Re-enable before calling CreateSpriteComponent
		bVisualizeComponent = true;
		CreateSpriteComponent(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/LevelInstance")), false);
		if (SpriteComponent)
		{
			SpriteComponent->bShowLockedLocation = false;
			SpriteComponent->SetVisibility(ShouldShowSpriteComponent());
			SpriteComponent->RegisterComponent();
		}
	}
#endif //WITH_EDITORONLY_DATA
}

bool ULevelInstanceComponent::ShouldShowSpriteComponent() const
{
	return GetOwner() && GetOwner()->GetLevel() && (GetOwner()->GetLevel()->IsPersistentLevel() || !GetOwner()->GetLevel()->IsInstancedLevel());
}

void ULevelInstanceComponent::PostEditUndo()
{
	Super::PostEditUndo();
		
	UpdateComponentToWorld();
	UpdateEditorInstanceActor();
}

void ULevelInstanceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateEditorInstanceActor();
}

void ULevelInstanceComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	UpdateEditorInstanceActor();
}

void ULevelInstanceComponent::UpdateEditorInstanceActor()
{
	if (!CachedEditorInstanceActorPtr.IsValid())
	{
		if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(GetOwner()))
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = LevelInstance->GetLevelInstanceSubsystem())
			{
				if (LevelInstanceSubsystem->IsLoaded(LevelInstance))
				{
					LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstance, [this, LevelInstance](AActor* LevelActor)
					{
						if (ALevelInstanceEditorInstanceActor* LevelInstanceEditorInstanceActor = Cast<ALevelInstanceEditorInstanceActor>(LevelActor))
						{
							check(LevelInstanceEditorInstanceActor->GetLevelInstanceID() == LevelInstance->GetLevelInstanceID());
							CachedEditorInstanceActorPtr = LevelInstanceEditorInstanceActor;
							return false;
						}
						return true;
					});
				}
			}
		}
	}

	if (AActor* EditorInstanceActor = CachedEditorInstanceActorPtr.Get())
	{
		EditorInstanceActor->GetRootComponent()->SetWorldTransform(GetComponentTransform());
	}
}

void ULevelInstanceComponent::OnEdit()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(false);
	}
}

void ULevelInstanceComponent::OnCommit()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(ShouldShowSpriteComponent());
	}
}

#endif
