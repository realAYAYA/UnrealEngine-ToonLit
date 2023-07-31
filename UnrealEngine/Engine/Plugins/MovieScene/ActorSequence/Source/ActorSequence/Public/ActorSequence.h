// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "ActorSequenceObjectReference.h"
#include "ActorSequence.generated.h"

/**
 * Movie scene animation embedded within an actor.
 */
UCLASS(BlueprintType, Experimental, DefaultToInstanced)
class ACTORSEQUENCE_API UActorSequence
	: public UMovieSceneSequence
{
public:
	GENERATED_BODY()

	UActorSequence(const FObjectInitializer& ObjectInitializer);

	//~ UMovieSceneSequence interface
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual void UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* Context) override {}
	virtual void UnbindInvalidObjects(const FGuid& ObjectId, UObject* Context) override {}
	virtual UObject* CreateDirectorInstance(IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID) override;

#if WITH_EDITOR
	virtual FText GetDisplayName() const override;
	virtual ETrackSupport IsTrackSupported(TSubclassOf<class UMovieSceneTrack> InTrackClass) const override;
#endif

#if WITH_EDITORONLY_DATA
	UBlueprint* GetParentBlueprint() const;
#endif

	bool IsEditable() const;
	
private:

	//~ UObject interface
	virtual void PostInitProperties() override;

private:
	
	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY(Instanced)
	TObjectPtr<UMovieScene> MovieScene;

	/** Collection of object references. */
	UPROPERTY()
	FActorSequenceObjectReferenceMap ObjectReferences;

#if WITH_EDITOR
public:

	/** Event that is fired to initialize default state for a sequence */
	DECLARE_EVENT_OneParam(UActorSequence, FOnInitialize, UActorSequence*)
	static FOnInitialize& OnInitializeSequence() { return OnInitializeSequenceEvent; }

private:
	static FOnInitialize OnInitializeSequenceEvent;
#endif

#if WITH_EDITORONLY_DATA
private:
	UPROPERTY()
	bool bHasBeenInitialized;
#endif
};