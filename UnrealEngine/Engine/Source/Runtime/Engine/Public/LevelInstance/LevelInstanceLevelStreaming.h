// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LevelStreamingDynamic.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstanceLevelStreaming.generated.h"

class ILevelInstanceInterface;
class ALevelInstanceEditorInstanceActor;

UCLASS(Transient, MinimalAPI)
class ULevelStreamingLevelInstance : public ULevelStreamingDynamic
{
	GENERATED_UCLASS_BODY()

public:
	ENGINE_API ILevelInstanceInterface* GetLevelInstance() const;

#if WITH_EDITOR
	virtual bool ShowInLevelCollection() const override { return false; }
	virtual bool IsUserManaged() const override { return false; }

	ENGINE_API FBox GetBounds() const;

	ENGINE_API virtual TOptional<FFolder::FRootObject> GetFolderRootObject() const override;
#endif
	
protected:
	static ENGINE_API ULevelStreamingLevelInstance* LoadInstance(ILevelInstanceInterface* LevelInstanceActor);
	static ENGINE_API void UnloadInstance(ULevelStreamingLevelInstance* LevelStreaming);

	ENGINE_API virtual void OnLevelLoadedChanged(ULevel* Level) override;

	friend class ULevelInstanceSubsystem;

	const FLevelInstanceID& GetLevelInstanceID() const { return LevelInstanceID; }
private:
#if WITH_EDITOR
	ENGINE_API void ResetLevelInstanceLoaders();
	ENGINE_API void PrepareLevelInstanceLoadedActor(AActor& InActor, ILevelInstanceInterface* InLevelInstance, bool bResetLoaders);
	ENGINE_API void OnLoadedActorPreAddedToLevel(const TArray<AActor*>& InActors);
	ENGINE_API void OnLoadedActorAddedToLevel(AActor& InActor);
	ENGINE_API void OnLoadedActorRemovedFromLevel(AActor& InActor);

	TWeakObjectPtr<ALevelInstanceEditorInstanceActor> LevelInstanceEditorInstanceActor;

	mutable FTransform CachedTransform;
	mutable FBox CachedBounds;
	bool bResetLoadersCalled;
#endif
	FLevelInstanceID LevelInstanceID;
};
