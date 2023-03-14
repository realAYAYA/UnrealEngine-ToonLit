// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstanceEditorLevelStreaming.generated.h"

class ILevelInstanceInterface;

UCLASS(Transient, MinimalAPI)
class ULevelStreamingLevelInstanceEditor : public ULevelStreamingAlwaysLoaded
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	virtual bool ShowInLevelCollection() const override { return false; }
	ILevelInstanceInterface* GetLevelInstance() const;
	const FLevelInstanceID& GetLevelInstanceID() const { return LevelInstanceID; }
	FBox GetBounds() const;

	virtual TOptional<FFolder::FRootObject> GetFolderRootObject() const override;
protected:
	void OnLevelActorAdded(AActor* InActor);
	void OnLoadedActorAddedToLevel(AActor& InActor);

	friend class ULevelInstanceSubsystem;

	static ULevelStreamingLevelInstanceEditor* Load(ILevelInstanceInterface* LevelInstance);
	static void Unload(ULevelStreamingLevelInstanceEditor* LevelStreaming);

	virtual void OnLevelLoadedChanged(ULevel* Level) override;
private:
	FLevelInstanceID LevelInstanceID;
#endif
};