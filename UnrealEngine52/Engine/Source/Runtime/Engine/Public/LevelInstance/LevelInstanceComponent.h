// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "LevelInstanceComponent.generated.h"

/**
 * ULevelInstanceComponent subclasses USceneComponent for Editing purposes so that we can have a proxy to the LevelInstanceActor's RootComponent transform without attaching to it.
 *
 * It is responsible for updating the transform of the ALevelInstanceEditorInstanceActor which is created when loading a LevelInstance Instance Level
 *
 * We use this method to avoid attaching the Instance Level Actors to the ILevelInstanceInterface. (Cross level attachment and undo/redo pain)
 * 
 * The LevelInstance Level Actors are attached to this ALevelInstanceEditorInstanceActor keeping the attachment local to the Instance Level and shielded from the transaction buffer.
 *
 * Avoiding those Level Actors from being part of the Transaction system allows us to unload that level without clearing the transaction buffer. It also allows BP Reinstancing without having to update attachements.
 */
UCLASS()
class ENGINE_API ULevelInstanceComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()
public:
#if WITH_EDITOR
	// Those are the methods that need overriding to be able to properly update the AttachComponent
	virtual void OnRegister() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	void UpdateEditorInstanceActor();
	void OnEdit();
	void OnCommit();
private:
	bool ShouldShowSpriteComponent() const;

	TWeakObjectPtr<AActor> CachedEditorInstanceActorPtr;
#endif
};