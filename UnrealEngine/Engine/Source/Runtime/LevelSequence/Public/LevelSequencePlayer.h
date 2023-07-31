// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "IMovieScenePlayer.h"
#endif
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "MovieSceneSequencePlayer.h"
#include "Misc/QualifiedFrameTime.h"
#include "LevelSequence.h"
#include "LevelSequenceCameraSettings.h"
#include "LevelSequencePlayer.generated.h"

class AActor;
class ALevelSequenceActor;
class FLevelSequenceSpawnRegister;
class FViewportClient;
class UCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelSequencePlayerCameraCutEvent, UCameraComponent*, CameraComponent);

USTRUCT(BlueprintType)
struct UE_DEPRECATED(5.0, "Snapshot settings are deprecated. Use the frame rate from the FQualifiedFrameTime on the MasterTime and the ShotTime") FLevelSequenceSnapshotSettings
{ GENERATED_BODY() };

/**
 * Frame snapshot information for a level sequence
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString MasterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime MasterTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime SourceTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString CurrentShotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime CurrentShotLocalTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FQualifiedFrameTime CurrentShotSourceTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FString SourceTimecode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	TSoftObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	TObjectPtr<ULevelSequence> ActiveShot = nullptr;

	UPROPERTY()
	FMovieSceneSequenceID ShotID;
};

/**
 * ULevelSequencePlayer is used to actually "play" an level sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * an level sequence while its playing.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	ULevelSequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InLevelSequence The level sequence to play.
	 * @param InLevel The level that the animation is played in.
	 * @param InCameraSettings The desired camera settings
	 */
	void Initialize(ULevelSequence* InLevelSequence, ULevel* InLevel, const FLevelSequenceCameraSettings& InCameraSettings);

	UE_DEPRECATED(5.1, "Use SetPlaybackSettings(...) then Initialize(ULevelSequence*, ULevel*, const FLevelSequenceCameraSettings&)")
	void Initialize(ULevelSequence* InLevelSequence, ULevel* InLevel, const FMovieSceneSequencePlaybackSettings& Settings, const FLevelSequenceCameraSettings& InCameraSettings)
	{
		SetPlaybackSettings(Settings);
		Initialize(InLevelSequence, InLevel, InCameraSettings);
	}

public:

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 * @param OutActor The level sequence actor created to play this sequence.
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Player", meta=(WorldContext="WorldContextObject", DynamicOutputParam="OutActor"))
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor);

	/** Event triggered when there is a camera cut */
	UPROPERTY(BlueprintAssignable, Category="Sequencer|Player")
	FOnLevelSequencePlayerCameraCutEvent OnCameraCut;

	/** Get the active camera cut camera */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Player")
	UCameraComponent* GetActiveCameraComponent() const { return CachedCameraComponent.Get(); }

public:

	// IMovieScenePlayer interface
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;

	void RewindForReplay();

protected:

	// IMovieScenePlayer interface
	virtual void UpdateCameraCut(UObject* CameraObject, const EMovieSceneCameraCutParams& CameraCutParams) override;
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;

	//~ UMovieSceneSequencePlayer interface
	virtual bool CanPlay() const override;
	virtual void OnStartedPlaying() override;
	virtual void OnStopped() override;
	virtual void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, const FMovieSceneUpdateArgs& Args) override;

public:

	/** Populate the specified array with any given event contexts for the specified world */
	static void GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts);

	/** Take a snapshot of the current state of this player */
	void TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const;

	/** Set the offset time for the snapshot in play rate frames. */
	void SetSnapshotOffsetFrames(int32 InFrameOffset) { SnapshotOffsetTime = TOptional<int32>(InFrameOffset); }

private:

	void EnableCinematicMode(bool bEnable);

private:

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<UWorld> World;

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<ULevel> Level;

	/** The full asset path (/Game/Folder/MapName.MapName) of the streaming level this player resides within. Bindings to actors with the same FSoftObjectPath::GetAssetPath are resolved within the cached level, rather than globally.. */
	FTopLevelAssetPath StreamedLevelAssetPath;

	/** The camera settings to use when playing the sequence */
	FLevelSequenceCameraSettings CameraSettings;

	/** The last view target to reset to when updating camera cuts to null */
	TWeakObjectPtr<AActor> LastViewTarget;

	/** The last aspect ratio axis constraint to reset to when the camera cut is null */
	TOptional<EAspectRatioAxisConstraint> LastAspectRatioAxisConstraint;

protected:

	TOptional<int32> SnapshotOffsetTime;

	TWeakObjectPtr<UCameraComponent> CachedCameraComponent;

private:

	TOptional<FLevelSequencePlayerSnapshot> PreviousSnapshot;
};
