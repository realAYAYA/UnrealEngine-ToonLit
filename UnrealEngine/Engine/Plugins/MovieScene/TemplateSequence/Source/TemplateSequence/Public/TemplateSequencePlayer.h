// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequencePlayer.h"
#include "TemplateSequencePlayer.generated.h"

class ATemplateSequenceActor;
class UTemplateSequence;

UCLASS(BlueprintType)
class TEMPLATESEQUENCE_API UTemplateSequencePlayer : public UMovieSceneSequencePlayer
{
public:

	GENERATED_BODY()

	UTemplateSequencePlayer(const FObjectInitializer&);

public:

	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", meta = (WorldContext = "WorldContextObject", DynamicOutputParam = "OutActor"))
	static UTemplateSequencePlayer* CreateTemplateSequencePlayer(UObject* WorldContextObject, UTemplateSequence* TemplateSequence, FMovieSceneSequencePlaybackSettings Settings, ATemplateSequenceActor*& OutActor);

public:

	void Initialize(UMovieSceneSequence* InSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings);

	// IMovieScenePlayer interface
	virtual UObject* GetPlaybackContext() const override;

private:

	/** The world this player will spawn actors in, if needed. */
	TWeakObjectPtr<UWorld> World;
};

/**
 * A spawn register that accepts a "wildcard" object.
 */
class FSequenceCameraShakeSpawnRegister : public FMovieSceneSpawnRegister
{
public:
	void SetSpawnedObject(UObject* InObject) { SpawnedObject = InObject; }

	virtual UObject* SpawnObject(FMovieSceneSpawnable&, FMovieSceneSequenceIDRef, IMovieScenePlayer&) override { return SpawnedObject.Get(); }
	virtual void DestroySpawnedObject(UObject&) override {}

#if WITH_EDITOR
	virtual bool CanSpawnObject(UClass* InClass) const override { return SpawnedObject.IsValid() && SpawnedObject.Get()->GetClass()->IsChildOf(InClass); }
#endif

private:
	FWeakObjectPtr SpawnedObject;
};
