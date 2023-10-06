// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "MovieSceneTracksComponentTypes.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneAudioTriggerChannel.h"
#include "EntitySystem/MovieSceneEntitySystem.h"

#include "MovieSceneAudioSystem.generated.h"

class IAudioParameterControllerInterface;
class UAudioComponent;
class UMovieSceneAudioSection;
class USoundBase;
struct FMoveSceneAudioTriggerState;

namespace UE::MovieScene
{
	struct FGatherAudioInputs;
	struct FGatherAudioTriggers;
	struct FEvaluateAudio;
	struct FPreAnimatedAudioStorage;

	struct FAudioComponentInputEvaluationData
	{
		TMap<FName, float> Inputs_Float;
		TMap<FName, FString> Inputs_String;
		TMap<FName, bool> Inputs_Bool;
		TMap<FName, int32> Inputs_Int;
		TArray<FName> Inputs_Trigger;
	};

	struct FAudioComponentEvaluationData
	{
		/** The audio component that was created to play audio */
		TWeakObjectPtr<UAudioComponent> AudioComponent;

		/** Volume multiplier to use this frame */
		double VolumeMultiplier;

		/** Pitch multiplier to use this frame */
		double PitchMultiplier;

		/**
		 * Set whenever we ask the Audio component to start playing a sound.
		 * Used to detect desyncs caused when Sequencer evaluates at more-than-real-time.
		 */
		TOptional<float> PartialDesyncComputation;

		/** Flag to keep track of audio components evaluated on a given frame */
		bool bEvaluatedThisFrame;
	};
}

/**
 * System for evaluating audio tracks
 */
UCLASS()
class UMovieSceneAudioSystem : public UMovieSceneEntitySystem
{
	GENERATED_BODY()

public:

	using FMovieSceneEntityID = UE::MovieScene::FMovieSceneEntityID;
	using FAudioComponentEvaluationData = UE::MovieScene::FAudioComponentEvaluationData;
	using FAudioComponentInputEvaluationData = UE::MovieScene::FAudioComponentInputEvaluationData;

	UMovieSceneAudioSystem(const FObjectInitializer& ObjInit);

	//~ UMovieSceneEntitySystem members
	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

	/**
	 * Get the evaluation data for the given actor and section. Pass a null actor key for root (world) audio.
	 */
	FAudioComponentEvaluationData* GetAudioComponentEvaluationData(FObjectKey ActorKey, FObjectKey SectionKey);

	/**
	 * Adds an audio component to the given bound sequencer object.
	 * WARNING: Only to be called on the game thread.
	 */
	FAudioComponentEvaluationData* AddBoundObjectAudioComponent(UMovieSceneAudioSection* Section, UObject* PrincipalObject);

	/**
	 * Adds an audio component to the world, for playing root audio tracks.
	 * WARNING: Only to be called on the game thread.
	 */
	FAudioComponentEvaluationData* AddRootAudioComponent(UMovieSceneAudioSection* Section, UWorld* World);

	/**
	 * Stop the audio on the audio component associated with the given audio section.
	 */
	void StopSound(FObjectKey ActorKey, FObjectKey SectionKey);

	/**
	 * Reset shared accumulation data required every evaluation frame
	 */
	void ResetSharedData();

private:

	/** Map of all created audio components */
	using FAudioComponentBySectionKey = TMap<FObjectKey, FAudioComponentEvaluationData>;
	using FAudioComponentsByActorKey = TMap<FObjectKey, FAudioComponentBySectionKey>;
	FAudioComponentsByActorKey AudioComponentsByActorKey;

	/** Map of audio input values, rebuilt every frame */
	using FAudioInputsBySectionKey = TMap<FObjectKey, FAudioComponentInputEvaluationData>;
	FAudioInputsBySectionKey AudioInputsBySectionKey;

	/** Pre-animated state */
	TSharedPtr<UE::MovieScene::FPreAnimatedAudioStorage> PreAnimatedStorage;

	friend struct UE::MovieScene::FGatherAudioInputs;
	friend struct UE::MovieScene::FGatherAudioTriggers;
	friend struct UE::MovieScene::FEvaluateAudio;
};

