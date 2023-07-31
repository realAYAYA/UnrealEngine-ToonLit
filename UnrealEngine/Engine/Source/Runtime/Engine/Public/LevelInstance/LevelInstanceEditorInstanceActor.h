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
UCLASS(transient, notplaceable)
class ENGINE_API ALevelInstanceEditorInstanceActor : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	static ALevelInstanceEditorInstanceActor* Create(ILevelInstanceInterface* LevelInstance, ULevel* LoadedLevel);
	
	void SetLevelInstanceID(const FLevelInstanceID& InLevelInstanceID) { LevelInstanceID = InLevelInstanceID; }
	const FLevelInstanceID& GetLevelInstanceID() const { return LevelInstanceID; }
	
	virtual bool IsSelectionParentOfAttachedActors() const override { return true; }
	virtual bool IsSelectionChild() const override { return true; }
	virtual AActor* GetSelectionParent() const override;

private:

	FLevelInstanceID LevelInstanceID;
#endif
};