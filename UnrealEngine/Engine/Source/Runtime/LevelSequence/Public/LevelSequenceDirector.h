// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneObjectBindingID.h"
#include "Misc/QualifiedFrameTime.h"
#include "LevelSequenceDirector.generated.h"

class IMovieScenePlayer;
class ULevelSequencePlayer;

UCLASS(Blueprintable)
class LEVELSEQUENCE_API ULevelSequenceDirector : public UObject
{
public:
	GENERATED_BODY()

	/** Called when this director is created */
	UFUNCTION(BlueprintImplementableEvent, Category="Sequencer")
	void OnCreated();
	
	/**
	 * Get the current time for the outermost (root, or master) sequence
	 * @return The current playback position of the master sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Director")
	FQualifiedFrameTime GetMasterSequenceTime() const;

	/**
	 * Get the current time for this director's sub-sequence (or the master sequence, if this is a master sequence director)
	 * @return The current playback position of this director's sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Director")
	FQualifiedFrameTime GetCurrentTime() const;

	/**
	 * Resolve the bindings inside this sub-sequence that relate to the specified ID
	 * @note: ObjectBinding should be constructed from the same sequence as this Sequence Director's owning Sequence (see the GetSequenceBinding node)
	 *
	 * @param ObjectBinding The ID for the object binding inside this sub-sequence or one of its children to resolve
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Director")
	TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding);


	/**
	 * Resolve the first valid binding inside this sub-sequence that relates to the specified ID
	 * @note: ObjectBinding should be constructed from the same sequence as this Sequence Director's owning Sequence (see the GetSequenceBinding node)
	 *
	 * @param ObjectBinding The ID for the object binding inside this sub-sequence or one of its children to resolve
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Director")
	UObject* GetBoundObject(FMovieSceneObjectBindingID ObjectBinding);


	/**
	 * Resolve the actor bindings inside this sub-sequence that relate to the specified ID
	 * @note: ObjectBinding should be constructed from the same sequence as this Sequence Director's owning Sequence (see the GetSequenceBinding node)
	 *
	 * @param ObjectBinding The ID for the object binding inside this sub-sequence or one of its children to resolve
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Director")
	TArray<AActor*> GetBoundActors(FMovieSceneObjectBindingID ObjectBinding);


	/**
	 * Resolve the first valid Actor binding inside this sub-sequence that relates to the specified ID
	 * @note: ObjectBinding should be constructed from the same sequence as this Sequence Director's owning Sequence (see the GetSequenceBinding node)
	 *
	 * @param ObjectBinding The ID for the object binding inside this sub-sequence or one of its children to resolve
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Director")
	AActor* GetBoundActor(FMovieSceneObjectBindingID ObjectBinding);

	/*
	 * Get the current sequence that this director is playing back within 
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Director")
	UMovieSceneSequence* GetSequence();

public:

	virtual UWorld* GetWorld() const override;

	/** Pointer to the player that's playing back this director's sequence. Only valid in game or in PIE/Simulate. */
	UPROPERTY(BlueprintReadOnly, Category="Cinematics")
	TObjectPtr<ULevelSequencePlayer> Player;

	/** The Sequence ID for the sequence this director is playing back within - has to be stored as an int32 so that it is reinstanced correctly*/
	UPROPERTY()
	int32 SubSequenceID;

	/** Native player interface index - stored by index so that it can be reinstanced correctly */
	UPROPERTY()
	int32 MovieScenePlayerIndex;
};


UCLASS()
class ULegacyLevelSequenceDirectorBlueprint : public UBlueprint
{
	GENERATED_BODY()

	ULegacyLevelSequenceDirectorBlueprint(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
	{
		ParentClass = ULevelSequenceDirector::StaticClass();
	}
};