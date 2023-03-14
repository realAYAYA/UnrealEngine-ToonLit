// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "MovieSceneSequencePlayer.h"
#include "ActorSequenceComponent.generated.h"


class UActorSequence;
class UActorSequencePlayer;


/**
 * Movie scene animation embedded within an actor.
 */
UCLASS(Blueprintable, Experimental, ClassGroup=Sequence, hidecategories=(Collision, Cooking, Activation), meta=(BlueprintSpawnableComponent))
class ACTORSEQUENCE_API UActorSequenceComponent
	: public UActorComponent
{
public:
	GENERATED_BODY()

	UActorSequenceComponent(const FObjectInitializer& Init);

	UActorSequence* GetSequence() const
	{
		return Sequence;
	}

	UActorSequencePlayer* GetSequencePlayer() const 
	{
		return SequencePlayer;
	}

	/** Calls the Play function on the SequencePlayer if its valid. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void PlaySequence();

	/** Calls the Pause function on the SequencePlayer if its valid. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void PauseSequence();

	/** Calls the Stop function on the SequencePlayer if its valid. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void StopSequence();
	
	// UActorComponent interface
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

protected:

	UPROPERTY(EditAnywhere, Category="Playback", meta=(ShowOnlyInnerProperties))
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** Embedded actor sequence data */
	UPROPERTY(EditAnywhere, Instanced, Category=Animation)
	TObjectPtr<UActorSequence> Sequence;

	UPROPERTY(transient, BlueprintReadOnly, Category=Animation)
	TObjectPtr<UActorSequencePlayer> SequencePlayer;
};
