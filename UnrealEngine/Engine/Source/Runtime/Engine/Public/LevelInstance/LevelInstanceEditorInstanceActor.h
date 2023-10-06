// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstanceEditorInstanceActor.generated.h"

class ILevelInstanceInterface;
class ULevel;

/**
 * Editor Only Actor that is spawned inside every LevelInstance Instance Level so that we can update its Actor Transforms through the ILevelInstanceInterface's (ULevelInstanceComponent)
 * @see ULevelInstanceComponent
 */
UCLASS(transient, notplaceable, MinimalAPI)
class ALevelInstanceEditorInstanceActor : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	static ENGINE_API ALevelInstanceEditorInstanceActor* Create(ILevelInstanceInterface* LevelInstance, ULevel* LoadedLevel);
	
	void SetLevelInstanceID(const FLevelInstanceID& InLevelInstanceID) { LevelInstanceID = InLevelInstanceID; }
	const FLevelInstanceID& GetLevelInstanceID() const { return LevelInstanceID; }
	
	virtual bool IsSelectionParentOfAttachedActors() const override { return true; }
	virtual bool IsSelectionChild() const override { return true; }
	ENGINE_API virtual AActor* GetSelectionParent() const override;

private:
	virtual bool ActorTypeIsMainWorldOnly() const override { return true; }
	friend class ULevelInstanceComponent;
	ENGINE_API void UpdateWorldTransform(const FTransform& WorldTransform);

	FLevelInstanceID LevelInstanceID;
#endif
};
